"""Build a destructive factory-install bundle from a verified distribution build."""

from __future__ import annotations

import hashlib
import json
import shutil
import sys
from pathlib import Path

# PlatformIO executes extra scripts through SCons, where __file__ is not
# guaranteed to be defined. Import the PlatformIO environment first and derive
# the scripts directory from PROJECT_DIR when available. Normal Python imports
# retain the usual __file__ path for host tests and tooling.
try:
    Import("env")  # type: ignore[name-defined]  # Provided by SCons/PlatformIO.
except NameError:
    env = None

if env is not None:
    SCRIPT_DIR = Path(env.subst("$PROJECT_DIR")) / "scripts"
else:
    SCRIPT_DIR = Path(__file__).resolve().parent

if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

from build_radar_ota import (
    DISTRIBUTION_BUILD_FLAG,
    _platformio_distribution_enabled,
    read_build_identity,
    validate_distribution_firmware,
)

FACTORY_MANIFEST_SCHEMA = 1
FACTORY_CHIP = "esp32s3"
FACTORY_FLASH_SIZE = "16MB"
FACTORY_FLASH_MODE = "dio"
FACTORY_FLASH_FREQ = "80m"
FACTORY_SCRIPT = "FLASH_RADAR_FACTORY.ps1"
FACTORY_MANIFEST = "factory-manifest.json"
BROWSER_HTML = "INSTALL_RADAR.html"
BROWSER_SCRIPT = "factory-installer.js"
ESPTOOL_JS_VERSION = "0.6.0"
FACTORY_FILES = (
    ("bootloader.bin", 0x00000000),
    ("partitions.bin", 0x00008000),
    ("boot_app0.bin", 0x0000E000),
    ("firmware.bin", 0x00010000),
)


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(128 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _validated_source_files(
    build_dir: Path, boot_app0_path: Path | None = None
) -> list[tuple[str, int, Path]]:
    found: list[tuple[str, int, Path]] = []
    for name, address in FACTORY_FILES:
        path = (
            boot_app0_path
            if name == "boot_app0.bin" and boot_app0_path is not None
            else build_dir / name
        )
        if not path.is_file():
            raise FileNotFoundError(f"required factory build output is missing: {path}")
        if path.stat().st_size <= 0:
            raise ValueError(f"required factory build output is empty: {path}")
        found.append((name, address, path))

    firmware = (build_dir / "firmware.bin").read_bytes()
    validate_distribution_firmware(firmware)
    return found


def _copy_required_file(source: Path, destination: Path, description: str) -> None:
    if not source.is_file():
        raise FileNotFoundError(f"{description} source is missing: {source}")
    shutil.copyfile(source, destination)


def write_factory_bundle(
    build_dir: Path,
    build_info_path: Path,
    installer_script: Path,
    release_root: Path,
    boot_app0_path: Path | None = None,
    browser_html: Path | None = None,
    browser_script: Path | None = None,
) -> Path:
    identity = read_build_identity(build_info_path)
    source_files = _validated_source_files(build_dir, boot_app0_path)

    if browser_html is None:
        browser_html = installer_script.parent / "browser" / BROWSER_HTML
    if browser_script is None:
        browser_script = installer_script.parent / "browser" / BROWSER_SCRIPT

    bundle_dir = release_root / "factory" / f"product-{identity.version_code}"
    if bundle_dir.exists():
        shutil.rmtree(bundle_dir)
    bundle_dir.mkdir(parents=True)

    entries = []
    for name, address, source in source_files:
        destination = bundle_dir / name
        shutil.copyfile(source, destination)
        entries.append(
            {
                "name": name,
                "address": f"0x{address:08X}",
                "size": destination.stat().st_size,
                "sha256": _sha256(destination),
            }
        )

    _copy_required_file(
        installer_script, bundle_dir / FACTORY_SCRIPT, "factory PowerShell installer"
    )
    if not browser_html.is_file():
        raise FileNotFoundError(f"browser installer HTML source is missing: {browser_html}")
    if not browser_script.is_file():
        raise FileNotFoundError(f"browser installer JavaScript source is missing: {browser_script}")
    html_source = browser_html.read_text(encoding="utf-8")
    script_source = browser_script.read_text(encoding="utf-8")
    external_tag = f'<script type="module" src="./{BROWSER_SCRIPT}"></script>'
    if external_tag not in html_source:
        raise ValueError("browser installer HTML is missing the expected module script tag")
    # The generated owner-facing installer is one double-clickable HTML file.
    # Embedding our module removes the file:// local-module CORS failure; the
    # pinned esptool-js dependency is still fetched from HTTPS by that module.
    bundled_html = html_source.replace(
        external_tag,
        '<script type="module">\n' + script_source + "\n</script>",
    )
    (bundle_dir / BROWSER_HTML).write_text(bundled_html, encoding="utf-8")

    manifest = {
        "schema": FACTORY_MANIFEST_SCHEMA,
        "hardware": identity.hardware,
        "chip": FACTORY_CHIP,
        "flash_size": FACTORY_FLASH_SIZE,
        "flash_mode": FACTORY_FLASH_MODE,
        "flash_freq": FACTORY_FLASH_FREQ,
        "version_code": identity.version_code,
        "version_label": identity.version_label,
        "build_id": identity.build_id,
        "distribution_marker": "RADAR-DISTRIBUTION-BUILD",
        "destructive_full_erase": True,
        "browser_installer": {
            "html": BROWSER_HTML,
            "self_contained": True,
            "esptool_js_version": ESPTOOL_JS_VERSION,
        },
        "files": entries,
    }
    (bundle_dir / FACTORY_MANIFEST).write_text(
        json.dumps(manifest, ensure_ascii=True, indent=2, sort_keys=True) + "\n",
        encoding="ascii",
    )
    return bundle_dir


def _platformio_post_action(source, target, env) -> None:
    if not _platformio_distribution_enabled(env):
        raise RuntimeError(
            f"Factory bundle generation requires {DISTRIBUTION_BUILD_FLAG}; "
            "private build refused"
        )

    project_dir = Path(env.subst("$PROJECT_DIR"))
    build_dir = Path(env.subst("$BUILD_DIR"))
    framework_dir = env.PioPlatform().get_package_dir("framework-arduinoespressif32")
    if not framework_dir:
        raise RuntimeError(
            "Arduino-ESP32 framework package directory could not be resolved"
        )
    boot_app0_path = (
        Path(framework_dir) / "tools" / "partitions" / "boot_app0.bin"
    )
    factory_dir = project_dir / "tools" / "factory"
    bundle_dir = write_factory_bundle(
        build_dir=build_dir,
        build_info_path=project_dir / "include" / "build_info.h",
        installer_script=factory_dir / FACTORY_SCRIPT,
        release_root=project_dir / "release",
        boot_app0_path=boot_app0_path,
        browser_html=factory_dir / "browser" / BROWSER_HTML,
        browser_script=factory_dir / "browser" / BROWSER_SCRIPT,
    )
    print(f"Radar destructive factory-install bundle: {bundle_dir}")
    print(f"Browser factory installer: {bundle_dir / BROWSER_HTML}")


if env is not None and not env.IsIntegrationDump():
    env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", _platformio_post_action)
