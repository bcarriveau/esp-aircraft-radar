"""Create Bill's bounded 7-inch Radar OTA package and release manifest."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import shutil
import struct
from typing import NamedTuple
from pathlib import Path

PACKAGE_MAGIC = b"BILLS-RADAR-OTA"
PACKAGE_HARDWARE_ID = b"WAVESHARE-ESP32-S3-LCD-7"
# Compatibility alias retained for the established package tests and tooling.
HARDWARE_ID = PACKAGE_HARDWARE_ID
FORMAT_VERSION = 1
HEADER_SIZE = 512
ESP_IMAGE_MAGIC = 0xE9
ESP32_S3_CHIP_ID = 9
HEADER_STRUCT = struct.Struct("<16sHH32s96sI32s328s")
MANIFEST_ASSET_NAME = "waveshare-esp32-s3-touch-lcd-7.manifest.json"
MAX_MANIFEST_BYTES = 2048


class BuildIdentity(NamedTuple):
    version_code: int
    version_label: str
    hardware: str
    channel: str
    manifest_schema: int
    updater_version: int
    release_notes: str
    build_id: str


class PackageMetadata(NamedTuple):
    package_size: int
    package_sha256: str
    firmware_size: int
    firmware_sha256: str
    build_id: str


def _fixed(value: bytes, size: int, field: str) -> bytes:
    if len(value) >= size:
        raise ValueError(f"{field} must be shorter than {size} bytes")
    return value + bytes(size - len(value))


def _extract_integer(text: str, name: str) -> int:
    match = re.search(
        rf"constexpr\s+(?:uint16_t|uint32_t)\s+{re.escape(name)}\s*=\s*(\d+)\s*;",
        text,
    )
    if not match:
        raise ValueError(f"{name} was not found")
    return int(match.group(1))


def _extract_string(text: str, name: str) -> str:
    match = re.search(
        rf"constexpr\s+const\s+char\*\s+{re.escape(name)}\s*=\s*"
        rf"((?:\s*\"[^\"]*\"\s*)+)\s*;",
        text,
    )
    if not match:
        raise ValueError(f"{name} was not found")
    parts = re.findall(r'\"([^\"]*)\"', match.group(1))
    return "".join(parts)


def read_build_identity(path: Path) -> BuildIdentity:
    text = path.read_text(encoding="utf-8")
    identity = BuildIdentity(
        version_code=_extract_integer(text, "FIRMWARE_VERSION_CODE"),
        version_label=_extract_string(text, "FIRMWARE_VERSION_LABEL"),
        hardware=_extract_string(text, "FIRMWARE_HARDWARE_ID"),
        channel=_extract_string(text, "FIRMWARE_RELEASE_CHANNEL"),
        manifest_schema=_extract_integer(text, "FIRMWARE_MANIFEST_SCHEMA"),
        updater_version=_extract_integer(text, "FIRMWARE_UPDATER_VERSION"),
        release_notes=_extract_string(text, "FIRMWARE_RELEASE_NOTES"),
        build_id=_extract_string(text, "BUILD_ID"),
    )
    if identity.version_code <= 0:
        raise ValueError("FIRMWARE_VERSION_CODE must be positive")
    if identity.version_label != f"Product {identity.version_code}":
        raise ValueError("FIRMWARE_VERSION_LABEL does not match version code")
    if identity.hardware != "waveshare-esp32-s3-touch-lcd-7":
        raise ValueError("unexpected firmware hardware identifier")
    if identity.channel != "stable":
        raise ValueError("release channel must be stable")
    if identity.manifest_schema != 1 or identity.updater_version < 1:
        raise ValueError("unsupported manifest or updater version")
    if not identity.build_id.startswith("7IN-"):
        raise ValueError("BUILD_ID does not identify the 7-inch radar")
    for name, value, maximum in (
        ("version label", identity.version_label, 31),
        ("build ID", identity.build_id, 95),
        ("release notes", identity.release_notes, 191),
    ):
        if not value or len(value.encode("ascii")) > maximum:
            raise ValueError(f"{name} is empty or too long")
    return identity


def read_build_id(path: Path) -> str:
    """Read only BUILD_ID for backward-compatible package generation."""
    text = path.read_text(encoding="utf-8")
    match = re.search(r'BUILD_ID\s*=\s*(?:\n\s*)?"([^"]+)"', text)
    if not match:
        raise ValueError(f"BUILD_ID was not found in {path}")
    build_id = match.group(1)
    if not build_id.startswith("7IN-"):
        raise ValueError("BUILD_ID does not identify the 7-inch radar")
    return build_id


def validate_firmware(firmware: bytes) -> None:
    if len(firmware) < 24:
        raise ValueError("firmware image is too small")
    if firmware[0] != ESP_IMAGE_MAGIC:
        raise ValueError("firmware does not have ESP application image magic")
    chip_id = struct.unpack_from("<H", firmware, 12)[0]
    if chip_id != ESP32_S3_CHIP_ID:
        raise ValueError(f"firmware chip ID {chip_id} is not ESP32-S3")


def create_package(firmware: bytes, build_id: str) -> bytes:
    validate_firmware(firmware)
    encoded_build_id = build_id.encode("ascii")
    if encoded_build_id not in firmware:
        raise ValueError("BUILD_ID is not embedded in the firmware image")
    digest = hashlib.sha256(firmware).digest()
    header = HEADER_STRUCT.pack(
        _fixed(PACKAGE_MAGIC, 16, "package magic"),
        FORMAT_VERSION,
        HEADER_SIZE,
        _fixed(PACKAGE_HARDWARE_ID, 32, "hardware ID"),
        _fixed(encoded_build_id, 96, "build ID"),
        len(firmware),
        digest,
        bytes(328),
    )
    if len(header) != HEADER_SIZE:
        raise AssertionError("unexpected OTA header size")
    return header + firmware


def validate_package(package: bytes) -> str:
    if len(package) < HEADER_SIZE:
        raise ValueError("OTA package is smaller than its header")
    fields = HEADER_STRUCT.unpack(package[:HEADER_SIZE])
    magic = fields[0].rstrip(b"\0")
    hardware_id = fields[3].rstrip(b"\0")
    build_id_bytes = fields[4].rstrip(b"\0")
    firmware_size = fields[5]
    expected_digest = fields[6]
    firmware = package[HEADER_SIZE:]
    if magic != PACKAGE_MAGIC:
        raise ValueError("OTA package magic is invalid")
    if fields[1] != FORMAT_VERSION or fields[2] != HEADER_SIZE:
        raise ValueError("OTA package version is unsupported")
    if hardware_id != PACKAGE_HARDWARE_ID:
        raise ValueError("OTA package hardware ID is invalid")
    if len(firmware) != firmware_size:
        raise ValueError("OTA package firmware length is invalid")
    validate_firmware(firmware)
    if hashlib.sha256(firmware).digest() != expected_digest:
        raise ValueError("OTA package firmware SHA-256 is invalid")
    build_id = build_id_bytes.decode("ascii")
    if build_id_bytes not in firmware:
        raise ValueError("OTA package build ID is not embedded in firmware")
    if not build_id.startswith("7IN-"):
        raise ValueError("OTA package build ID is invalid")
    return build_id


def write_package(
    firmware_path: Path, build_info_path: Path, output_path: Path
) -> tuple[int, str]:
    firmware = firmware_path.read_bytes()
    build_id = read_build_id(build_info_path)
    package = create_package(firmware, build_id)
    validate_package(package)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_bytes(package)
    return len(package), hashlib.sha256(package).hexdigest()


def package_metadata(package: bytes) -> PackageMetadata:
    build_id = validate_package(package)
    firmware = package[HEADER_SIZE:]
    return PackageMetadata(
        package_size=len(package),
        package_sha256=hashlib.sha256(package).hexdigest(),
        firmware_size=len(firmware),
        firmware_sha256=hashlib.sha256(firmware).hexdigest(),
        build_id=build_id,
    )


def versioned_package_name(identity: BuildIdentity) -> str:
    return f"{identity.hardware}-product-{identity.version_code}.radarota"


def create_release_manifest(
    identity: BuildIdentity, metadata: PackageMetadata, asset_name: str
) -> bytes:
    manifest = {
        "schema": identity.manifest_schema,
        "tag": f"product-{identity.version_code}",
        "hardware": identity.hardware,
        "channel": identity.channel,
        "version_code": identity.version_code,
        "version_label": identity.version_label,
        "build_id": metadata.build_id,
        "asset": asset_name,
        "package_size": metadata.package_size,
        "package_sha256": metadata.package_sha256,
        "firmware_size": metadata.firmware_size,
        "firmware_sha256": metadata.firmware_sha256,
        "min_updater": identity.updater_version,
        "notes": identity.release_notes,
    }
    encoded = json.dumps(
        manifest, ensure_ascii=True, separators=(",", ":"), sort_keys=True
    ).encode("ascii") + b"\n"
    if len(encoded) > MAX_MANIFEST_BYTES:
        raise ValueError("release manifest exceeds the 2048-byte firmware limit")
    return encoded


def write_release_assets(
    package_path: Path, build_info_path: Path, release_dir: Path
) -> tuple[Path, Path, PackageMetadata]:
    identity = read_build_identity(build_info_path)
    package = package_path.read_bytes()
    metadata = package_metadata(package)
    if metadata.build_id != identity.build_id:
        raise ValueError("package and build-info identities do not match")

    release_dir.mkdir(parents=True, exist_ok=True)
    asset_name = versioned_package_name(identity)
    asset_path = release_dir / asset_name
    manifest_path = release_dir / MANIFEST_ASSET_NAME
    shutil.copyfile(package_path, asset_path)
    manifest_path.write_bytes(
        create_release_manifest(identity, metadata, asset_name)
    )
    return asset_path, manifest_path, metadata


def _platformio_post_action(source, target, env) -> None:
    firmware_path = Path(target[0].get_abspath())
    project_dir = Path(env.subst("$PROJECT_DIR"))
    build_info_path = project_dir / "include" / "build_info.h"
    output_path = firmware_path.with_suffix(".radarota")
    package_size, package_sha = write_package(
        firmware_path, build_info_path, output_path
    )
    asset_path, manifest_path, _ = write_release_assets(
        output_path, build_info_path, project_dir / "release"
    )
    print(
        f"Radar OTA package: {output_path} "
        f"({package_size} bytes, SHA256 {package_sha})"
    )
    print(f"Versioned browser/GitHub OTA package: {asset_path}")
    print(f"GitHub Release manifest asset: {manifest_path}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("firmware", type=Path)
    parser.add_argument("build_info", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument(
        "--release-dir",
        type=Path,
        help="also write the versioned GitHub Release package and manifest",
    )
    args = parser.parse_args()
    size, digest = write_package(args.firmware, args.build_info, args.output)
    print(f"{args.output}: {size} bytes, SHA256 {digest}")
    if args.release_dir:
        asset, manifest, _ = write_release_assets(
            args.output, args.build_info, args.release_dir
        )
        print(asset)
        print(manifest)
    return 0


try:
    Import("env")  # type: ignore[name-defined]  # Provided by SCons/PlatformIO.
except NameError:
    env = None

if env is not None and not env.IsIntegrationDump():
    env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", _platformio_post_action)

if __name__ == "__main__":
    raise SystemExit(main())
