#!/usr/bin/env python3
"""Build and validate persistent regional airport database packages.

This module defines the transport/storage format intended to replace the current
compile-time generated airport header. It does not change firmware behavior by
itself. The package contains only the filtered regional airport records and
non-sensitive metadata; generation-center coordinates are deliberately omitted.
"""

from __future__ import annotations

import hashlib
import os
import struct
import tempfile
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, Protocol

PACKAGE_MAGIC = b"BILLS-AIRPORTDB\0"
PACKAGE_FORMAT_VERSION = 1
PACKAGE_HEADER_SIZE = 256
MAX_RECORD_COUNT = 65535
MAX_RADIUS_MILES = 500
MAX_DATE_BYTES = 15
MAX_COVERAGE_BYTES = 63

# Explicit little-endian, packed-on-disk layout. No host/compiler padding.
# ident[8], name[32], latE6, lonE6, elevationFt, runwayLengthFt,
# runwayHeadingDegrees, category.
RECORD_STRUCT = struct.Struct("<8s32siihHHB")
RECORD_SIZE = RECORD_STRUCT.size

# magic, format version, header size, record size, generator version,
# record count, radius miles, flags, date[16], coverage[64], payload size,
# payload SHA-256. Remaining header bytes are reserved and zero-filled.
_HEADER_PREFIX_STRUCT = struct.Struct("<16sHHHHIHH16s64sI32s")


class AirportLike(Protocol):
    ident: str
    name: str
    latitude: float
    longitude: float
    elevation: int
    runway_length: int
    runway_heading: int
    category: int


@dataclass(frozen=True)
class PackageInfo:
    format_version: int
    generator_version: int
    record_count: int
    radius_miles: int
    database_date: str
    coverage: str
    payload_size: int
    payload_sha256: bytes


@dataclass(frozen=True)
class PackageRecord:
    ident: str
    name: str
    latitude_e6: int
    longitude_e6: int
    elevation_ft: int
    runway_length_ft: int
    runway_heading_degrees: int
    category: int


def _encode_fixed(text: str, field_size: int, max_bytes: int) -> bytes:
    encoded = text.encode("ascii", "strict")
    if len(encoded) > max_bytes:
        raise ValueError(f"text exceeds {max_bytes} ASCII bytes: {text!r}")
    return encoded + b"\0" * (field_size - len(encoded))


def _decode_fixed(raw: bytes) -> str:
    return raw.split(b"\0", 1)[0].decode("ascii", "strict")


def _validate_record_values(airport: AirportLike) -> None:
    ident = airport.ident.encode("ascii", "strict")
    name = airport.name.encode("ascii", "strict")
    if not 1 <= len(ident) <= 7:
        raise ValueError(f"invalid airport ident length: {airport.ident!r}")
    if not 1 <= len(name) <= 31:
        raise ValueError(f"invalid airport name length: {airport.name!r}")
    latitude_e6 = round(airport.latitude * 1_000_000)
    longitude_e6 = round(airport.longitude * 1_000_000)
    if not -90_000_000 <= latitude_e6 <= 90_000_000:
        raise ValueError(f"invalid airport latitude: {airport.latitude}")
    if not -180_000_000 <= longitude_e6 <= 180_000_000:
        raise ValueError(f"invalid airport longitude: {airport.longitude}")
    if not -32768 <= airport.elevation <= 32767:
        raise ValueError(f"invalid elevation for {airport.ident}")
    if not 0 <= airport.runway_length <= 65535:
        raise ValueError(f"invalid runway length for {airport.ident}")
    if not 0 <= airport.runway_heading <= 360:
        raise ValueError(f"invalid runway heading for {airport.ident}")
    if not 0 <= airport.category <= 3:
        raise ValueError(f"invalid category for {airport.ident}")


def build_payload(airports: Iterable[AirportLike]) -> tuple[bytes, int]:
    chunks: list[bytes] = []
    count = 0
    for airport in airports:
        _validate_record_values(airport)
        chunks.append(
            RECORD_STRUCT.pack(
                _encode_fixed(airport.ident, 8, 7),
                _encode_fixed(airport.name, 32, 31),
                round(airport.latitude * 1_000_000),
                round(airport.longitude * 1_000_000),
                airport.elevation,
                airport.runway_length,
                airport.runway_heading,
                airport.category,
            )
        )
        count += 1
        if count > MAX_RECORD_COUNT:
            raise ValueError("too many airport records for package format")
    if count == 0:
        raise ValueError("airport package cannot be empty")
    return b"".join(chunks), count


def build_package(
    airports: Iterable[AirportLike],
    *,
    database_date: str,
    coverage: str,
    radius_miles: int,
    generator_version: int,
) -> bytes:
    if not 90 <= radius_miles <= MAX_RADIUS_MILES:
        raise ValueError("radius must be between 90 and 500 miles")
    if not 0 <= generator_version <= 65535:
        raise ValueError("generator version must fit uint16_t")

    payload, record_count = build_payload(airports)
    payload_digest = hashlib.sha256(payload).digest()
    prefix = _HEADER_PREFIX_STRUCT.pack(
        PACKAGE_MAGIC,
        PACKAGE_FORMAT_VERSION,
        PACKAGE_HEADER_SIZE,
        RECORD_SIZE,
        generator_version,
        record_count,
        radius_miles,
        0,
        _encode_fixed(database_date, 16, MAX_DATE_BYTES),
        _encode_fixed(coverage, 64, MAX_COVERAGE_BYTES),
        len(payload),
        payload_digest,
    )
    if len(prefix) > PACKAGE_HEADER_SIZE:
        raise AssertionError("airport package header prefix exceeds fixed header size")
    header = prefix + b"\0" * (PACKAGE_HEADER_SIZE - len(prefix))
    return header + payload


def parse_package(package: bytes) -> tuple[PackageInfo, list[PackageRecord]]:
    if len(package) < PACKAGE_HEADER_SIZE:
        raise ValueError("airport package is shorter than its fixed header")
    fields = _HEADER_PREFIX_STRUCT.unpack_from(package, 0)
    (
        magic,
        format_version,
        header_size,
        record_size,
        generator_version,
        record_count,
        radius_miles,
        flags,
        raw_date,
        raw_coverage,
        payload_size,
        expected_digest,
    ) = fields

    if magic != PACKAGE_MAGIC:
        raise ValueError("airport package magic mismatch")
    if format_version != PACKAGE_FORMAT_VERSION:
        raise ValueError("unsupported airport package format version")
    if header_size != PACKAGE_HEADER_SIZE:
        raise ValueError("airport package header size mismatch")
    if record_size != RECORD_SIZE:
        raise ValueError("airport package record size mismatch")
    if flags != 0:
        raise ValueError("airport package uses unsupported flags")
    if record_count == 0 or record_count > MAX_RECORD_COUNT:
        raise ValueError("airport package record count is invalid")
    if not 90 <= radius_miles <= MAX_RADIUS_MILES:
        raise ValueError("airport package radius is invalid")
    if payload_size != record_count * RECORD_SIZE:
        raise ValueError("airport package payload size does not match record count")
    if len(package) != PACKAGE_HEADER_SIZE + payload_size:
        raise ValueError("airport package total size mismatch")

    reserved = package[_HEADER_PREFIX_STRUCT.size:PACKAGE_HEADER_SIZE]
    if any(reserved):
        raise ValueError("airport package reserved header bytes must be zero")

    payload = package[PACKAGE_HEADER_SIZE:]
    actual_digest = hashlib.sha256(payload).digest()
    if actual_digest != expected_digest:
        raise ValueError("airport package payload SHA-256 mismatch")

    records: list[PackageRecord] = []
    for offset in range(0, payload_size, RECORD_SIZE):
        (
            raw_ident,
            raw_name,
            latitude_e6,
            longitude_e6,
            elevation_ft,
            runway_length_ft,
            runway_heading_degrees,
            category,
        ) = RECORD_STRUCT.unpack_from(payload, offset)
        ident = _decode_fixed(raw_ident)
        name = _decode_fixed(raw_name)
        if not ident or not name:
            raise ValueError("airport package contains an empty ident or name")
        if not -90_000_000 <= latitude_e6 <= 90_000_000:
            raise ValueError(f"airport package latitude is invalid for {ident}")
        if not -180_000_000 <= longitude_e6 <= 180_000_000:
            raise ValueError(f"airport package longitude is invalid for {ident}")
        if runway_heading_degrees > 360 or category > 3:
            raise ValueError(f"airport package record fields are invalid for {ident}")
        records.append(
            PackageRecord(
                ident=ident,
                name=name,
                latitude_e6=latitude_e6,
                longitude_e6=longitude_e6,
                elevation_ft=elevation_ft,
                runway_length_ft=runway_length_ft,
                runway_heading_degrees=runway_heading_degrees,
                category=category,
            )
        )

    info = PackageInfo(
        format_version=format_version,
        generator_version=generator_version,
        record_count=record_count,
        radius_miles=radius_miles,
        database_date=_decode_fixed(raw_date),
        coverage=_decode_fixed(raw_coverage),
        payload_size=payload_size,
        payload_sha256=expected_digest,
    )
    return info, records


def write_package_atomic(path: Path, package: bytes) -> None:
    # Validate before touching the destination so callers cannot atomically
    # install a package this module itself would reject.
    parse_package(package)
    path.parent.mkdir(parents=True, exist_ok=True)
    descriptor, temporary_name = tempfile.mkstemp(
        prefix=f".{path.name}.", suffix=".tmp", dir=path.parent
    )
    temporary = Path(temporary_name)
    try:
        with os.fdopen(descriptor, "wb") as handle:
            handle.write(package)
            handle.flush()
            os.fsync(handle.fileno())
        temporary.replace(path)
    except Exception:
        temporary.unlink(missing_ok=True)
        raise
