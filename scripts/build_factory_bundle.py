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


def write_factory_bundle(
    build_dir: Path,
    build_info_path: Path,
    installer_script: Path,
    release_root: Path,
    boot_app0_path: Path | None = None,
) -> Path:
    identity = read_build_identity(build_info_path)
    source_files = _validated_source_files(build_dir, boot_app0_path)
    if not installer_script.is_file():
        raise FileNotFoundError(f"factory installer source is missing: {installer_script}")

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

    shutil.copyfile(installer_script, bundle_dir / FACTORY_SCRIPT)
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
    bundle_dir = write_factory_bundle(
        build_dir=build_dir,
        build_info_path=project_dir / "include" / "build_info.h",
        installer_script=project_dir / "tools" / "factory" / FACTORY_SCRIPT,
        release_root=project_dir / "release",
        boot_app0_path=boot_app0_path,
    )
    print(f"Radar destructive factory-install bundle: {bundle_dir}")


if env is not None and not env.IsIntegrationDump():
    env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", _platformio_post_action)
