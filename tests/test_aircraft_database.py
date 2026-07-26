#!/usr/bin/env python3
"""Focused aircraft classifier database and explicit API checks.

Runs without PlatformIO. Optional --csv verifies deterministic regeneration from
an exact source snapshot. Optional --host-compile compiles the real classifier,
exercises target-aware APIs, and rejects the removed implicit array dispatch.
"""
from __future__ import annotations

import argparse
import hashlib
import importlib.util
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
HEADER = ROOT / "include/generated/aircraft_type_database.h"
GENERATOR = ROOT / "tools/generate_aircraft_database.py"
PUBLIC_HEADER = ROOT / "include/aircraft_data.h"

EXPECTED = {
    "C190": 4,
    "C170": 4,
    "C17": 2,
    "E190": 0,
    "B190": 3,
    "PC12": 3,
    "PC24": 1,
    "C208": 3,
    "C25A": 1,
    "H64": 5,
    "V22": 2,
    "GLID": 6,
    "GYRO": 6,
    "UHEL": 5,
    "P8": 2,
    "BT7": 2,
}


def pack_designator(code: str) -> int:
    normalized = "".join(ch for ch in code.upper() if ch.isascii() and ch.isalnum())
    if not 1 <= len(normalized) <= 8:
        return 0
    value = 0
    for ch in normalized:
        value = (value << 8) | ord(ch)
    return value << (8 * (8 - len(normalized)))


def parse_array(text: str, name: str, pattern: str) -> list[int]:
    match = re.search(rf"constexpr [^\n]+ {name}\[[^\]]+\] = \{{(.*?)\n\}};", text, re.S)
    if not match:
        raise AssertionError(f"missing generated array {name}")
    return [int(value, 16 if value.startswith("0x") else 10)
            for value in re.findall(pattern, match.group(1))]


def public_header_checks() -> None:
    text = PUBLIC_HEADER.read_text(encoding="utf-8")
    forbidden = (
        "targetFromTypeCodeMember",
        "IsTypeCodeInput",
        "offsetof(Target, typeCode)",
        "reinterpret_cast<const Target*>",
        "Category categoryForType(",
        "AircraftBitmapId bitmapForType(",
    )
    for fragment in forbidden:
        assert fragment not in text, fragment
    assert "Category categoryForTarget(const Target& target);" in text
    assert "AircraftBitmapId bitmapForTarget(const Target& target);" in text
    assert "const char* kindName(const Target& target);" in text


def generated_checks() -> None:
    text = HEADER.read_text(encoding="utf-8")
    codes = parse_array(text, "kTypeCodes", r"UINT64_C\((0x[0-9A-F]+)\)")
    categories = parse_array(text, "kTypeCategories", r"\b([0-6])\b")
    hashes = parse_array(text, "kDescriptionAliasHashes", r"UINT64_C\((0x[0-9A-F]+)\)")
    alias_categories = parse_array(text, "kDescriptionAliasCategories", r"\b([0-6])\b")

    assert len(codes) == 2697
    assert len(codes) == len(categories)
    assert len(hashes) == 11716
    assert len(hashes) == len(alias_categories)
    assert codes == sorted(codes)
    assert len(codes) == len(set(codes))
    assert hashes == sorted(hashes)
    assert len(hashes) == len(set(hashes))

    table = dict(zip(codes, categories))
    for code, expected in EXPECTED.items():
        actual = table.get(pack_designator(code), 6)
        assert actual == expected, (code, expected, actual)


def load_generator():
    spec = importlib.util.spec_from_file_location("aircraft_db_generator", GENERATOR)
    if spec is None or spec.loader is None:
        raise RuntimeError("unable to load generator")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def regeneration_check(csv_path: pathlib.Path) -> None:
    generator = load_generator()
    raw = csv_path.read_bytes()
    records = generator.load_records_from_bytes(raw)
    aliases = generator.build_aliases(records)
    regenerated = generator.render_header(records, aliases).encode("utf-8")
    checked_in = HEADER.read_bytes()
    assert regenerated == checked_in
    assert hashlib.sha256(checked_in).hexdigest() == \
        "5cb411c1923209972da74477cbc52ec88f9468dd9b6301916b490a0f938c5737"


def host_compile_check() -> None:
    compiler = shutil.which("g++") or shutil.which("c++")
    if not compiler:
        raise RuntimeError("no C++17 compiler found")

    with tempfile.TemporaryDirectory(prefix="aircraft-classifier-") as temp_name:
        temp = pathlib.Path(temp_name)
        include = temp / "include"
        generated = include / "generated"
        generated.mkdir(parents=True)
        shutil.copy2(ROOT / "include/aircraft_data.h", include / "aircraft_data.h")
        shutil.copy2(HEADER, generated / HEADER.name)
        (include / "aircraft_bitmap_types.h").write_text(
            "#pragma once\n#include <stdint.h>\n"
            "enum class AircraftBitmapId : uint8_t { AIRLINER, BUSINESS_JET, "
            "TURBOPROP, PISTON, HELICOPTER, UNKNOWN };\n",
            encoding="utf-8",
        )
        (include / "Arduino.h").write_text(
            "#pragma once\n#include <cmath>\n#include <cstddef>\n#include <cstdint>\n"
            "#include <cstdio>\n#include <cstring>\n",
            encoding="utf-8",
        )
        test_cpp = temp / "test.cpp"
        test_cpp.write_text(
            '#include "aircraft_data.h"\n#include <cassert>\n#include <cstring>\n'
            "int main(){using aircraft::Category;"
            "using AircraftBitmapId = ::AircraftBitmapId;"
            'assert(aircraft::categoryForTypeCode("C190")==Category::PISTON);'
            'assert(aircraft::categoryForTypeCode("C17")==Category::MILITARY_HEAVY);'
            'assert(aircraft::categoryForTypeCode("E190")==Category::AIRLINER);'
            'char unrelated[9]="C190";'
            'assert(aircraft::categoryForTypeCode(unrelated)==Category::PISTON);'
            'assert(aircraft::bitmapForTypeCode(unrelated)==AircraftBitmapId::PISTON);'
            'assert(std::strcmp(aircraft::kindNameForTypeCode(unrelated),"PISTON")==0);'
            'struct DescriptionCase{const char* text;Category expected;};'
            'const DescriptionCase cases[]={{"DAHER TBM 960",Category::TURBOPROP},'
            '{"SINGLE ENGINE PISTON LAND",Category::PISTON},'
            '{"MOONEY M20J",Category::PISTON},'
            '{"PILATUS PC-24",Category::BUSINESS_JET},'
            '{"PILATUS PC-12",Category::TURBOPROP},'
            '{"CESSNA CITATION CJ4",Category::BUSINESS_JET},'
            '{"CESSNA 172 SKYHAWK",Category::PISTON},'
            '{"PIPER PA-23 APACHE",Category::PISTON},'
            '{"AH-64 APACHE",Category::HELICOPTER},'
            '{"AIRBUS HELICOPTER H125",Category::HELICOPTER},'
            '{"AIRBUS A320",Category::AIRLINER},'
            '{"BOMBARDIER GLOBAL 7500",Category::BUSINESS_JET},'
            '{"BOMBARDIER CRJ 900",Category::AIRLINER},'
            '{"UNKNOWN MODEL",Category::UNKNOWN},'
            '{"JET AIRCRAFT",Category::UNKNOWN}};'
            'for(const auto& item:cases)'
            'assert(aircraft::categoryForDescription(item.text)==item.expected);'
            "aircraft::Target t{};std::strcpy(t.typeCode,\"UNKNOWN\");"
            "std::strcpy(t.description,\"CESSNA 190\");"
            "assert(aircraft::categoryForTarget(t)==Category::PISTON);"
            "assert(aircraft::bitmapForTarget(t)==AircraftBitmapId::PISTON);"
            "assert(std::strcmp(aircraft::kindName(t),\"PISTON\")==0);return 0;}\n",
            encoding="utf-8",
        )
        output = temp / "test"
        subprocess.run(
            [compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror", "-pedantic",
             f"-I{include}", str(ROOT / "src/aircraft_data.cpp"), str(test_cpp),
             "-o", str(output)],
            check=True,
        )
        subprocess.run([str(output)], check=True)

        legacy_cpp = temp / "legacy_implicit_api.cpp"
        legacy_cpp.write_text(
            '#include "aircraft_data.h"\n'
            'int main(){char unrelated[9]="C190";'
            'auto category=aircraft::categoryForType(unrelated);'
            'auto bitmap=aircraft::bitmapForType(unrelated);'
            'auto name=aircraft::kindName(unrelated);'
            '(void)category;(void)bitmap;(void)name;}\n',
            encoding="utf-8",
        )
        legacy = subprocess.run(
            [compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror",
             "-pedantic", f"-I{include}", "-c", str(legacy_cpp),
             "-o", str(temp / "legacy_implicit_api.o")],
            capture_output=True,
            text=True,
        )
        assert legacy.returncode != 0, "legacy implicit classifier API still compiles"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--csv", type=pathlib.Path,
                        help="verify deterministic regeneration from this CSV")
    parser.add_argument("--host-compile", action="store_true")
    args = parser.parse_args()

    public_header_checks()
    generated_checks()
    if args.csv:
        regeneration_check(args.csv)
    if args.host_compile:
        host_compile_check()
    print("Aircraft classifier database and explicit API tests passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
