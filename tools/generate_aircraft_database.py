#!/usr/bin/env python3
"""Generate the Product 40 flash-resident aircraft type database.

The generator consumes a pinned ICAO Doc 8643-derived CSV snapshot and emits a
sorted, deterministic C++ header. Runtime firmware performs exact designator
lookups first and uses a collision-checked description-alias hash table only
when the designator is absent or unresolved.
"""
from __future__ import annotations

import argparse
import csv
import hashlib
import json
import pathlib
import re
import sys
import urllib.request
from collections import defaultdict
from dataclasses import dataclass
from typing import Iterable

SOURCE_URL = (
    "https://raw.githubusercontent.com/siembroeder/OverFlight/"
    "6666bc57a95f64102cf9511d887e73a8e5379d5f/data/icao_8643.csv"
)
SOURCE_REVISION = "6666bc57a95f64102cf9511d887e73a8e5379d5f"
SOURCE_SHA256 = "eeb813e48c670f4366bfc5de7da59d9e390295d10630d0c2c85713e0842bab94"
SOURCE_DATE = "2026-06-01"
EXPECTED_ROWS = 2697

CATEGORY = {
    "AIRLINER": 0,
    "BUSINESS_JET": 1,
    "MILITARY_HEAVY": 2,
    "TURBOPROP": 3,
    "PISTON": 4,
    "HELICOPTER": 5,
    "UNKNOWN": 6,
}

REQUIRED_COLUMNS = {
    "ModelFullName",
    "WTC",
    "WTG",
    "Designator",
    "ManufacturerCode",
    "AircraftDescription",
    "EngineCount",
    "EngineType",
}

# Exact role exceptions are intentionally explicit. Engine/form data remains
# authoritative for piston, turboprop, helicopter, glider, and special types.
MILITARY_HEAVY_CODES = {
    # Large transport, tanker, surveillance and bomber families.
    "A6", "A10", "A3", "A37", "A400", "A50", "AMX", "AN72", "A743", "A178",
    "ARES", "B1", "B2", "B21", "B52", "BT7", "BER2", "BER4", "BUC", "C1", "C15", "C17", "C22J", "CL41", "C5M", "C101", "C130",
    "C135", "E3CF", "E3TF", "E6", "E737", "E767", "E390", "EUFI", "F1",
    "F2", "F4", "F5", "F8", "F9F", "F14", "F15", "F16", "F18H", "F18S",
    "F22", "F35", "F86", "F104", "F117", "HAR", "HAWK", "HUNT", "IL28",
    "IL76", "J10", "J20", "JH7", "JAGR", "K35E", "K35R", "K46A", "K50",
    "KAAN", "KC2", "KE3", "KF21", "KFIR", "LCA", "LTNG", "M15", "M17",
    "ME62", "METR", "M55", "M326", "M339", "M345", "M346", "MG15", "MG17", "MG19",
    "MG21", "MG23", "MG25", "MG29", "MG31", "MG44", "MIRA", "MIR2",
    "MRF1", "P8", "Q4", "Q5", "Q25", "Q28", "Q58", "RFAL", "R135", "S3",
    "SB05", "SB29", "SB32", "SB35", "SB37", "SB39", "SGCD", "SGEF",
    "SHAW", "STRK", "SU7", "SU17", "SU24", "SU25", "SU27", "SU57", "T1", "T2",
    "T4", "T5YY", "T7", "T33", "T37", "T38", "T160", "T22M", "TU16",
    "TU22", "U2", "VAUT", "VF35", "W135", "WB57", "X47B", "X59", "Y20",
    "Y130", "YK28", "YK30", "YURO",
    # Major military turboprop/tiltrotor/rotorcraft designators that historically
    # used the project's MIL/HEAVY presentation.
    "C27J", "C295", "CN35", "V22", "H64", "H47", "H53", "H60", "UH1",
}

BUSINESS_JET_CODES = {
    "A700", "ASTR", "BE40", "BE4W", "BD5J", "C500", "C501", "C510", "C525",
    "C25A", "C25B", "C25C", "C25M", "C550", "C551", "C560", "C55B", "C56X",
    "C650", "C680", "C68A", "C700", "C750", "CL30", "CL35", "CL60", "COZJ",
    "E35L", "E50P", "E545", "E550", "E55P", "EA40", "EA50", "FA10", "FA20",
    "FA50", "FA6X", "FA7X", "FA8X", "F2TH", "F900", "FJ10", "G150", "G280",
    "GA3C", "GA4C", "GA5C", "GA6C", "GA7C", "GA8C", "GALX", "GL5T", "GLEX",
    "GLF2", "GLF3", "GLF4", "GLF5", "GLF6", "GSPN", "HA4T", "H25A", "H25B",
    "H25C", "HDJT", "HF20", "JCOM", "L29A", "L29B", "LAR1", "LJ23", "LJ24",
    "LJ25", "LJ28", "LJ31", "LJ35", "LJ40", "LJ45", "LJ55", "LJ60", "LJ70",
    "LJ75", "PC24", "PRM1", "S601", "S716", "SBR1", "SBR2", "SF50", "SJ30",
    "MS76", "SP33", "TJET", "VANT", "VIPJ", "WW23", "WW24",
}

AIRLINER_CODES = {
    # Explicit mixed-manufacturer civilian jet families.
    "A148", "A158", "AJ27", "BA11", "B461", "B462", "B463", "C919", "CRJ1",
    "CRJ2", "CRJ7", "CRJ9", "CRJX", "DC86", "DC87", "DC91", "DC92", "DC93",
    "DC94", "DC95", "E135", "E145", "E170", "E190", "E195", "E275", "E290", "E295",
    "E45X", "E75L", "E75S", "F28", "F70", "F100", "IL62", "IL86", "IL96",
    "J328", "L101", "MC23", "RJ1H", "RJ70", "RJ85", "SU95", "T134", "T154",
    "T204", "T334", "YK40", "YK42",
}

BUSINESS_MANUFACTURERS = {
    "CESSNA", "GULFSTREAM AEROSPACE", "LEARJET", "GATES LEARJET", "LEAR JET",
    "ECLIPSE", "HONDA", "HAWKER BEECHCRAFT", "AERO COMMANDER", "EMIVEST",
    "SPECTRUM", "STRATOS", "VISIONAIRE", "VIPER", "ADAM (2)",
}

AIRLINER_MANUFACTURERS = {"AIRBUS", "BOEING", "COMAC"}

MILITARY_JET_MANUFACTURERS = {
    "AIDC", "AERO (2)", "AERMACCHI", "AOI", "AVIOANE", "BAC", "BAYKAR",
    "BOEING AUSTRALIA", "CASA", "CHANCE VOUGHT", "CHENGDU", "DE HAVILLAND",
    "DOUGLAS", "FAIRCHILD (1)", "FIAT", "FMA", "FOLLAND", "FUJI", "GUIZHOU",
    "HINDUSTAN", "HONGDU", "IRIAF", "INSTYTUT LOTNICTWA", "KAWASAKI",
    "KOREA AEROSPACE", "KRATOS", "LEONARDO", "MAPO", "MARGANSKI", "MARTIN",
    "MIKOYAN", "MITSUBISHI", "MYASISHCHEV", "NORTHROP", "NORTHROP GRUMMAN",
    "PZL-MIELEC", "ROCKWELL", "SAAB", "SHENYANG", "SIAI-MARCHETTI", "SIPA",
    "SOKO", "SOKO-CNIAR", "SUD", "TAI", "XIAN",
}

@dataclass(frozen=True)
class Record:
    code: str
    category: int
    manufacturer: str
    model: str
    form: str
    engine: str


def normalize(value: str) -> str:
    return "".join(ch for ch in value.upper() if "A" <= ch <= "Z" or "0" <= ch <= "9")


def pack_designator(code: str) -> int:
    if not 1 <= len(code) <= 8 or not re.fullmatch(r"[A-Z0-9]+", code):
        raise ValueError(f"invalid designator {code!r}")
    packed = 0
    for ch in code:
        packed = (packed << 8) | ord(ch)
    for _ in range(8 - len(code)):
        packed <<= 8
    return packed


def fnv1a64(text: str) -> int:
    value = 0xCBF29CE484222325
    for byte in text.encode("ascii"):
        value ^= byte
        value = (value * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF
    return value


def classify(row: dict[str, str]) -> int:
    code = row["Designator"].strip().upper()
    manufacturer = row["ManufacturerCode"].strip().upper()
    model = row["ModelFullName"].strip().upper()
    form = row["AircraftDescription"].strip().upper()
    engine = row["EngineType"].strip().upper()

    if form in {"HELICOPTER", "ULTRALIGHT HELICOPTER"}:
        return CATEGORY["HELICOPTER"]
    if code == "V22":
        return CATEGORY["MILITARY_HEAVY"]
    if form == "TILTROTOR":
        return CATEGORY["HELICOPTER"]
    if code in MILITARY_HEAVY_CODES:
        return CATEGORY["MILITARY_HEAVY"]
    if form in {"GLIDER", "GYROCOPTER", "ULTRALIGHT AUTOGYRO", "AIRSHIP", "BALLOON", '"POWERED PARACHUTE"'}:
        return CATEGORY["UNKNOWN"]

    if engine == "PISTON":
        return CATEGORY["PISTON"]
    if engine == "TURBOPROP/TURBOSHAFT":
        return CATEGORY["TURBOPROP"]
    if engine != "JET":
        return CATEGORY["UNKNOWN"]

    if code in BUSINESS_JET_CODES:
        return CATEGORY["BUSINESS_JET"]
    if code in AIRLINER_CODES:
        return CATEGORY["AIRLINER"]

    # Mixed manufacturers are handled by exact code/model rules first.
    if manufacturer == "DASSAULT":
        return CATEGORY["BUSINESS_JET"] if "FALCON" in model else CATEGORY["MILITARY_HEAVY"]
    if manufacturer == "BOMBARDIER":
        return CATEGORY["AIRLINER"] if code.startswith("CRJ") else CATEGORY["BUSINESS_JET"]
    if manufacturer == "EMBRAER":
        if code in {"AMX", "M326", "E390"}:
            return CATEGORY["MILITARY_HEAVY"]
        if any(word in model for word in ("PHENOM", "LEGACY", "LINEAGE", "PRAETOR")):
            return CATEGORY["BUSINESS_JET"]
        return CATEGORY["AIRLINER"]
    if manufacturer == "LOCKHEED":
        if code == "L101":
            return CATEGORY["AIRLINER"]
        if code in {"L29A", "L29B"}:
            return CATEGORY["BUSINESS_JET"]
        return CATEGORY["MILITARY_HEAVY"]
    if manufacturer == "MCDONNELL DOUGLAS":
        return CATEGORY["AIRLINER"] if code.startswith("DC") else CATEGORY["MILITARY_HEAVY"]
    if manufacturer == "BRITISH AEROSPACE":
        if code in {"BA11", "B461", "B462", "B463"}:
            return CATEGORY["AIRLINER"]
        if code.startswith("H25"):
            return CATEGORY["BUSINESS_JET"]
        return CATEGORY["MILITARY_HEAVY"]
    if manufacturer == "FOKKER":
        return CATEGORY["MILITARY_HEAVY"] if code == "F16" else CATEGORY["AIRLINER"]
    if manufacturer == "ANTONOV":
        return CATEGORY["AIRLINER"] if code in {"A148", "A158"} else CATEGORY["MILITARY_HEAVY"]
    if manufacturer == "ILYUSHIN":
        return CATEGORY["AIRLINER"] if code in {"IL62", "IL86", "IL96"} else CATEGORY["MILITARY_HEAVY"]
    if manufacturer == "SUKHOI":
        return CATEGORY["AIRLINER"] if code == "SU95" else CATEGORY["MILITARY_HEAVY"]
    if manufacturer == "TUPOLEV":
        return CATEGORY["AIRLINER"] if code in {"T134", "T154", "T204", "T334"} else CATEGORY["MILITARY_HEAVY"]
    if manufacturer == "YAKOVLEV":
        return CATEGORY["AIRLINER"] if code in {"YK40", "YK42"} else CATEGORY["MILITARY_HEAVY"]
    if manufacturer == "AEROSPATIALE":
        return CATEGORY["BUSINESS_JET"] if code == "S601" else CATEGORY["MILITARY_HEAVY"]
    if manufacturer == "IAI":
        return CATEGORY["BUSINESS_JET"] if code in {"WW23", "WW24"} else CATEGORY["MILITARY_HEAVY"]
    if manufacturer == "NORTH AMERICAN":
        return CATEGORY["BUSINESS_JET"] if code == "SBR1" else CATEGORY["MILITARY_HEAVY"]
    if manufacturer == "NORTH AMERICAN ROCKWELL":
        return CATEGORY["BUSINESS_JET"]
    if manufacturer == "BAE SYSTEMS":
        return CATEGORY["AIRLINER"]

    if manufacturer in BUSINESS_MANUFACTURERS:
        return CATEGORY["BUSINESS_JET"]
    if manufacturer in AIRLINER_MANUFACTURERS:
        return CATEGORY["AIRLINER"]
    if manufacturer in MILITARY_JET_MANUFACTURERS:
        return CATEGORY["MILITARY_HEAVY"]

    # Unknown is deliberate for experimental and ambiguous jets. The firmware
    # will not mislabel a type merely to avoid the unknown artwork.
    return CATEGORY["UNKNOWN"]


def alias_variants(record: Record) -> set[str]:
    manufacturer = normalize(record.manufacturer)
    model = normalize(record.model)
    code = normalize(record.code)
    variants = {code}
    if model:
        variants.add(model)
    if manufacturer:
        variants.add(manufacturer + code)
        if model:
            variants.add(manufacturer + model)

    # Common ADS-B description strings often append a marketing name or variant.
    # Preserve conservative leading model identifiers as prefix-match candidates.
    tokens = re.findall(r"[A-Za-z0-9]+", record.model)
    if tokens:
        first = normalize(tokens[0])
        if len(first) >= 2:
            variants.add(first)
            if manufacturer:
                variants.add(manufacturer + first)
        if len(tokens) >= 2:
            first_two = normalize(tokens[0] + tokens[1])
            if len(first_two) >= 3:
                variants.add(first_two)
                if manufacturer:
                    variants.add(manufacturer + first_two)
    return {v for v in variants if 3 <= len(v) <= 63}


def load_records_from_bytes(raw: bytes) -> list[Record]:
    digest = hashlib.sha256(raw).hexdigest()
    if digest != SOURCE_SHA256:
        raise ValueError(f"source SHA-256 mismatch: expected {SOURCE_SHA256}, got {digest}")

    rows = list(csv.DictReader(raw.decode("utf-8-sig").splitlines()))
    if not rows:
        raise ValueError("source CSV is empty")
    missing = REQUIRED_COLUMNS - set(rows[0])
    if missing:
        raise ValueError(f"source CSV missing columns: {sorted(missing)}")
    if len(rows) != EXPECTED_ROWS:
        raise ValueError(f"expected {EXPECTED_ROWS} rows, found {len(rows)}")

    seen: set[str] = set()
    records: list[Record] = []
    for row in rows:
        code = row["Designator"].strip().upper()
        pack_designator(code)
        if code in seen:
            raise ValueError(f"duplicate designator {code}")
        seen.add(code)
        records.append(
            Record(
                code=code,
                category=classify(row),
                manufacturer=row["ManufacturerCode"].strip(),
                model=row["ModelFullName"].strip(),
                form=row["AircraftDescription"].strip(),
                engine=row["EngineType"].strip(),
            )
        )
    records.sort(key=lambda item: pack_designator(item.code))
    return records


def build_aliases(records: Iterable[Record]) -> list[tuple[int, int]]:
    categories_by_alias: dict[str, set[int]] = defaultdict(set)
    for record in records:
        for alias in alias_variants(record):
            categories_by_alias[alias].add(record.category)

    aliases: list[tuple[int, int]] = []
    hashes: dict[int, tuple[str, int]] = {}
    for alias, categories in categories_by_alias.items():
        if len(categories) != 1:
            continue
        category = next(iter(categories))
        if category == CATEGORY["UNKNOWN"]:
            continue
        value = fnv1a64(alias)
        previous = hashes.get(value)
        if previous and previous != (alias, category):
            raise ValueError(f"FNV-1a collision: {previous[0]} and {alias}")
        hashes[value] = (alias, category)
        aliases.append((value, category))
    aliases.sort()
    return aliases


def render_header(records: list[Record], aliases: list[tuple[int, int]]) -> str:
    category_counts = defaultdict(int)
    for record in records:
        category_counts[record.category] += 1

    lines = [
        "// Generated by tools/generate_aircraft_database.py. Do not edit manually.",
        "#pragma once",
        "",
        "#include <stddef.h>",
        "#include <stdint.h>",
        "",
        "namespace aircraft {",
        "namespace generated {",
        "",
        f'constexpr char kSourceRevision[] = "{SOURCE_REVISION}";',
        f'constexpr char kSourceSha256[] = "{SOURCE_SHA256}";',
        f'constexpr char kSourceDate[] = "{SOURCE_DATE}";',
        f"constexpr size_t kTypeRecordCount = {len(records)}u;",
        f"constexpr size_t kDescriptionAliasCount = {len(aliases)}u;",
        "",
        "constexpr uint64_t kTypeCodes[kTypeRecordCount] = {",
    ]
    for index in range(0, len(records), 4):
        chunk = records[index:index + 4]
        lines.append("  " + ", ".join(f"UINT64_C(0x{pack_designator(r.code):016X})" for r in chunk) + ",")
    lines.extend(["};", "", "constexpr uint8_t kTypeCategories[kTypeRecordCount] = {"])
    for index in range(0, len(records), 24):
        chunk = records[index:index + 24]
        lines.append("  " + ", ".join(str(r.category) for r in chunk) + ",")
    lines.extend(["};", "", "constexpr uint64_t kDescriptionAliasHashes[kDescriptionAliasCount] = {"])
    for index in range(0, len(aliases), 4):
        chunk = aliases[index:index + 4]
        lines.append("  " + ", ".join(f"UINT64_C(0x{value:016X})" for value, _ in chunk) + ",")
    lines.extend(["};", "", "constexpr uint8_t kDescriptionAliasCategories[kDescriptionAliasCount] = {"])
    for index in range(0, len(aliases), 24):
        chunk = aliases[index:index + 24]
        lines.append("  " + ", ".join(str(category) for _, category in chunk) + ",")
    lines.extend([
        "};",
        "",
        "static_assert(sizeof(kTypeCodes) / sizeof(kTypeCodes[0]) == kTypeRecordCount,",
        '              "Aircraft type-code table size mismatch");',
        "static_assert(sizeof(kTypeCategories) / sizeof(kTypeCategories[0]) == kTypeRecordCount,",
        '              "Aircraft category table size mismatch");',
        "static_assert(sizeof(kDescriptionAliasHashes) / sizeof(kDescriptionAliasHashes[0]) ==",
        "                  kDescriptionAliasCount,",
        '              "Description hash table size mismatch");',
        "static_assert(sizeof(kDescriptionAliasCategories) /",
        "                  sizeof(kDescriptionAliasCategories[0]) == kDescriptionAliasCount,",
        '              "Description category table size mismatch");',
        "",
        "}  // namespace generated",
        "}  // namespace aircraft",
        "",
    ])
    return "\n".join(lines)


def download_source() -> bytes:
    with urllib.request.urlopen(SOURCE_URL, timeout=30) as response:
        raw = response.read()
    digest = hashlib.sha256(raw).hexdigest()
    if digest != SOURCE_SHA256:
        raise ValueError(f"download SHA-256 mismatch: expected {SOURCE_SHA256}, got {digest}")
    return raw


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", type=pathlib.Path, help="local pinned CSV snapshot")
    parser.add_argument("--output", type=pathlib.Path, default=pathlib.Path("include/generated/aircraft_type_database.h"))
    parser.add_argument("--download", action="store_true", help="download the pinned source in memory")
    parser.add_argument("--report", type=pathlib.Path)
    args = parser.parse_args()

    try:
        if args.download == (args.input is not None):
            raise ValueError("choose exactly one of --download or --input")
        raw = download_source() if args.download else args.input.read_bytes()
        records = load_records_from_bytes(raw)
        aliases = build_aliases(records)
        output = render_header(records, aliases)
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(output, encoding="utf-8", newline="\n")

        counts = defaultdict(int)
        for record in records:
            counts[record.category] += 1
        report = {
            "source_url": SOURCE_URL,
            "source_revision": SOURCE_REVISION,
            "source_sha256": SOURCE_SHA256,
            "source_date": SOURCE_DATE,
            "type_records": len(records),
            "description_aliases": len(aliases),
            "category_counts": {name: counts[value] for name, value in CATEGORY.items()},
        }
        if args.report:
            args.report.parent.mkdir(parents=True, exist_ok=True)
            args.report.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        print(json.dumps(report, sort_keys=True))
        return 0
    except (OSError, ValueError, csv.Error) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
