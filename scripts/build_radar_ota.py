"""Create the bounded Bill's 7-inch Radar OTA package after PlatformIO builds."""

from __future__ import annotations

import argparse
import hashlib
import re
import struct
from pathlib import Path

PACKAGE_MAGIC = b"BILLS-RADAR-OTA"
HARDWARE_ID = b"WAVESHARE-ESP32-S3-LCD-7"
FORMAT_VERSION = 1
HEADER_SIZE = 512
ESP_IMAGE_MAGIC = 0xE9
ESP32_S3_CHIP_ID = 9
HEADER_STRUCT = struct.Struct("<16sHH32s96sI32s328s")


def _fixed(value: bytes, size: int, field: str) -> bytes:
    if len(value) >= size:
        raise ValueError(f"{field} must be shorter than {size} bytes")
    return value + bytes(size - len(value))


def read_build_id(path: Path) -> str:
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
        _fixed(HARDWARE_ID, 32, "hardware ID"),
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
    if hardware_id != HARDWARE_ID:
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


def write_package(firmware_path: Path, build_info_path: Path,
                  output_path: Path) -> tuple[int, str]:
    firmware = firmware_path.read_bytes()
    build_id = read_build_id(build_info_path)
    package = create_package(firmware, build_id)
    validate_package(package)
    output_path.write_bytes(package)
    return len(package), hashlib.sha256(package).hexdigest()


def _platformio_post_action(source, target, env) -> None:
    firmware_path = Path(target[0].get_abspath())
    project_dir = Path(env.subst("$PROJECT_DIR"))
    output_path = firmware_path.with_suffix(".radarota")
    package_size, package_sha = write_package(
        firmware_path, project_dir / "include" / "build_info.h", output_path
    )
    print(
        f"Radar OTA package: {output_path} "
        f"({package_size} bytes, SHA256 {package_sha})"
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("firmware", type=Path)
    parser.add_argument("build_info", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    size, digest = write_package(args.firmware, args.build_info, args.output)
    print(f"{args.output}: {size} bytes, SHA256 {digest}")
    return 0


try:
    Import("env")  # type: ignore[name-defined]  # Provided by SCons/PlatformIO.
except NameError:
    env = None

if env is not None and not env.IsIntegrationDump():
    env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", _platformio_post_action)

if __name__ == "__main__":
    raise SystemExit(main())
