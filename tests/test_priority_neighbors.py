#!/usr/bin/env python3
"""Focused Product 76 selected/tracked neighbor checks."""

from __future__ import annotations

import math
import re
import unittest
from dataclasses import dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "src" / "radar_renderer.cpp"
BUILD = ROOT / "include" / "build_info.h"


@dataclass(frozen=True)
class Target:
    hex: str
    distance: float
    bearing: float


def local_position(target: Target) -> tuple[float, float]:
    radians = math.radians(target.bearing)
    return math.cos(radians) * target.distance, math.sin(radians) * target.distance


def nearest(primary: Target, targets: list[Target], capacity: int = 3) -> list[tuple[str, float, float]]:
    primary_north, primary_east = local_position(primary)
    candidates: list[tuple[float, str, float]] = []
    for target in targets:
        if not target.hex or target.hex == primary.hex:
            continue
        north, east = local_position(target)
        delta_north = north - primary_north
        delta_east = east - primary_east
        separation_squared = delta_north * delta_north + delta_east * delta_east
        bearing = (math.degrees(math.atan2(delta_east, delta_north)) + 360.0) % 360.0
        candidates.append((separation_squared, target.hex, bearing))
    candidates.sort(key=lambda item: (item[0], item[1]))
    return [
        (hex_code, math.sqrt(separation_squared), bearing)
        for separation_squared, hex_code, bearing in candidates[:capacity]
    ]


class Product76PriorityNeighborTests(unittest.TestCase):
    def test_source_uses_bounded_fixed_top_three(self) -> None:
        source = SOURCE.read_text(encoding="utf-8")
        self.assertIn("PriorityNeighbor neighbors[PRIORITY_OTHER_COUNT]{}", source)
        self.assertIn("findPriorityNeighbors(", source)
        self.assertIn("neighborCapacity", source)
        self.assertNotIn("std::vector", source)
        self.assertNotIn("std::sort", source)

    def test_source_uses_relative_not_home_distance(self) -> None:
        source = SOURCE.read_text(encoding="utf-8")
        start = source.index(
            "PriorityNeighbor neighbors[PRIORITY_OTHER_COUNT]{};"
        )
        end = source.index(
            "} else if (snapshot.manualTracking", start
        )
        text = source[start:end]
        self.assertIn("sqrtf(neighbor.separationSquaredMiles)", text)
        self.assertIn("neighbor.relativeBearingDegrees", text)
        self.assertNotIn("target.distanceMiles", text)
        self.assertNotIn("target.bearing", text)

    def test_source_handles_selected_tracked_and_lost_position(self) -> None:
        source = SOURCE.read_text(encoding="utf-8")
        self.assertIn('"NEAR TRACK"', source)
        self.assertIn('"NEAR SELECT"', source)
        self.assertIn('"POSITION LOST"', source)
        self.assertIn("priorityAircraft && primaryTarget", source)
        self.assertIn("strcmp(candidate.hex, primary.hex) == 0", source)

    def test_build_identity_is_product_76(self) -> None:
        build = BUILD.read_text(encoding="utf-8")
        self.assertIn("FIRMWARE_VERSION_CODE = 76", build)
        self.assertIn('FIRMWARE_VERSION_LABEL = "Product 76"', build)
        self.assertIn("7IN-20260804-PRODUCT76-PRIORITY-NEIGHBORS", build)

    def test_relative_geometry_and_ordering(self) -> None:
        primary = Target("AAAAAA", 10.0, 0.0)
        targets = [
            primary,
            Target("BBBBBB", 11.0, 0.0),   # 1 mile north
            Target("CCCCCC", 10.0, 90.0),  # about 14.14 miles southeast
            Target("DDDDDD", 9.0, 0.0),    # 1 mile south
            Target("EEEEEE", 12.0, 0.0),   # 2 miles north
        ]
        result = nearest(primary, targets)
        self.assertEqual([item[0] for item in result], ["BBBBBB", "DDDDDD", "EEEEEE"])
        self.assertAlmostEqual(result[0][1], 1.0, places=6)
        self.assertAlmostEqual(result[0][2], 0.0, places=6)
        self.assertAlmostEqual(result[1][1], 1.0, places=6)
        self.assertAlmostEqual(result[1][2], 180.0, places=6)

    def test_equal_distance_tie_breaks_by_stable_hex(self) -> None:
        primary = Target("PRIMARY", 0.0, 0.0)
        targets = [
            primary,
            Target("BBBBBB", 1.0, 90.0),
            Target("AAAAAA", 1.0, 270.0),
        ]
        result = nearest(primary, targets)
        self.assertEqual([item[0] for item in result], ["AAAAAA", "BBBBBB"])

    def test_bounded_result_with_two_hundred_targets(self) -> None:
        primary = Target("000000", 20.0, 45.0)
        targets = [primary]
        for index in range(1, 200):
            targets.append(
                Target(
                    f"{index:06X}",
                    5.0 + (index % 70),
                    float((index * 37) % 360),
                )
            )
        result = nearest(primary, targets)
        self.assertEqual(len(result), 3)
        self.assertEqual(
            result,
            sorted(result, key=lambda item: (item[1] * item[1], item[0])),
        )

    def test_home_nearest_is_not_assumed_to_be_priority_nearest(self) -> None:
        primary = Target("PRIMARY", 60.0, 90.0)
        home_near = Target("HOME01", 1.0, 0.0)
        primary_near = Target("CLOSE1", 61.0, 90.0)
        result = nearest(primary, [home_near, primary, primary_near])
        self.assertEqual(result[0][0], "CLOSE1")
        self.assertAlmostEqual(result[0][1], 1.0, places=5)


if __name__ == "__main__":
    unittest.main()
