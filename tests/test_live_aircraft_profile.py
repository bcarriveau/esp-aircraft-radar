#!/usr/bin/env python3
"""Focused Product 77 live Aircraft Profile regression checks."""

from __future__ import annotations

import re
import unittest
from dataclasses import dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
UI_SOURCE = (ROOT / "src" / "ui.cpp").read_text(encoding="utf-8")
BUILD_INFO = (ROOT / "include" / "build_info.h").read_text(encoding="utf-8")


def function_body(source: str, signature: str) -> str:
    start = source.find(signature)
    if start < 0:
        raise AssertionError(f"Missing function: {signature}")
    brace = source.find("{", start)
    if brace < 0:
        raise AssertionError(f"Missing function body: {signature}")
    depth = 0
    in_string = False
    in_char = False
    escaped = False
    line_comment = False
    block_comment = False
    index = brace
    while index < len(source):
        char = source[index]
        nxt = source[index + 1] if index + 1 < len(source) else ""
        if line_comment:
            if char == "\n":
                line_comment = False
        elif block_comment:
            if char == "*" and nxt == "/":
                block_comment = False
                index += 1
        elif in_string:
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == '"':
                in_string = False
        elif in_char:
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == "'":
                in_char = False
        elif char == "/" and nxt == "/":
            line_comment = True
            index += 1
        elif char == "/" and nxt == "*":
            block_comment = True
            index += 1
        elif char == '"':
            in_string = True
        elif char == "'":
            in_char = True
        elif char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                return source[brace : index + 1]
        index += 1
    raise AssertionError(f"Unterminated function: {signature}")


@dataclass(frozen=True)
class Target:
    hex: str
    altitude: int


def resolve_profile(
    open_hex: str,
    targets: list[Target],
    manual_tracking: bool = False,
    tracked_hex: str = "",
) -> tuple[Target | None, bool]:
    """Behavioral model of Product 77 stable-ICAO profile resolution."""
    for target in targets:
        if target.hex == open_hex:
            return target, False
    tracked_missing = manual_tracking and tracked_hex == open_hex
    return None, tracked_missing


class Product77SourceTests(unittest.TestCase):
    def test_product_marker(self) -> None:
        self.assertIn("FIRMWARE_VERSION_CODE = 77", BUILD_INFO)
        self.assertIn('FIRMWARE_VERSION_LABEL = "Product 77"', BUILD_INFO)
        self.assertIn(
            '"7IN-20260805-PRODUCT77-LIVE-AIRCRAFT-PROFILE"', BUILD_INFO
        )

    def test_profile_refresh_is_version_gated(self) -> None:
        body = function_body(UI_SOURCE, "void refreshOpenTargetDetails()")
        self.assertIn("app_state::targetVersion()", body)
        self.assertIn("app_state::rangeGeneration()", body)
        self.assertIn("app_state::trackingVersion()", body)
        self.assertIn("app_state::copySnapshot(uiTargets, snapshot)", body)
        self.assertEqual(body.count("app_state::copySnapshot"), 1)
        self.assertIn("detailTargetVersion == currentTargetVersion", body)
        self.assertIn("detailRangeGeneration == currentRangeGeneration", body)
        self.assertIn("detailTrackingVersion == currentTrackingVersion", body)

    def test_profile_identity_is_stable_icao(self) -> None:
        body = function_body(UI_SOURCE, "void refreshOpenTargetDetails()")
        self.assertIn("strcmp(uiTargets[index].hex, detailTarget.hex)", body)
        self.assertNotIn("detailTargetIndex", UI_SOURCE)
        self.assertNotRegex(body, r"detailTarget\s*=\s*uiTargets\[0\]")

    def test_refresh_is_bounded_and_allocation_free(self) -> None:
        body = function_body(UI_SOURCE, "void refreshOpenTargetDetails()")
        self.assertIn("index < snapshot.count", body)
        for forbidden in (
            "malloc(",
            "calloc(",
            "realloc(",
            "new ",
            "String(",
            "std::vector",
            "std::sort",
        ):
            self.assertNotIn(forbidden, body)

    def test_current_and_last_known_states_are_explicit(self) -> None:
        body = function_body(UI_SOURCE, "void renderTargetDetails(")
        self.assertIn('"CURRENT UPDATE"', body)
        self.assertIn('"NOT IN CURRENT UPDATE\\nLAST KNOWN VALUES"', body)
        self.assertIn('"TRACK SIGNAL LOST\\nLAST KNOWN VALUES"', body)
        self.assertIn('"NOT AVAILABLE"', body)
        self.assertIn("LV_STATE_DISABLED", body)

    def test_tracked_missing_profile_can_still_stop_tracking(self) -> None:
        render_body = function_body(UI_SOURCE, "void renderTargetDetails(")
        action_body = function_body(UI_SOURCE, "void detailTrackEvent(")
        self.assertIn("trackedMissing || app_state::isManuallyTracked(target)", render_body)
        self.assertIn("if (!detailTargetCurrent && !tracked) return", action_body)
        self.assertIn("stopManualTracking()", action_body)

    def test_profile_state_resets_cleanly(self) -> None:
        reset_body = function_body(UI_SOURCE, "void resetTargetDetailsState()")
        self.assertIn("detailTargetValid = false", reset_body)
        self.assertIn("detailTargetCurrent = false", reset_body)
        self.assertEqual(reset_body.count("UINT32_MAX"), 3)
        self.assertGreaterEqual(UI_SOURCE.count("resetTargetDetailsState();"), 3)

    def test_main_loop_services_open_profile(self) -> None:
        body = function_body(UI_SOURCE, "void update(uint32_t now)")
        self.assertIn("refreshOpenTargetDetails();", body)
        self.assertLess(
            body.index("refreshOpenTargetDetails();"),
            body.index("if (currentPage == 0 && !detailTargetValid)"),
        )

    def test_existing_profile_objects_are_updated_not_rebuilt(self) -> None:
        refresh = function_body(UI_SOURCE, "void refreshOpenTargetDetails()")
        render = function_body(UI_SOURCE, "void renderTargetDetails(")
        for constructor in (
            "lv_label_create",
            "lv_btn_create",
            "lv_obj_create",
            "lv_canvas_create",
        ):
            self.assertNotIn(constructor, refresh)
            self.assertNotIn(constructor, render)


class Product77BehaviorModelTests(unittest.TestCase):
    def test_reordered_snapshot_resolves_same_hex(self) -> None:
        first = Target("ABC123", 12000)
        second = Target("DEF456", 22000)
        resolved, tracked_missing = resolve_profile(
            "ABC123", [second, first]
        )
        self.assertEqual(resolved, first)
        self.assertFalse(tracked_missing)

    def test_current_snapshot_replaces_last_known_values(self) -> None:
        old = Target("ABC123", 12000)
        new = Target("ABC123", 13500)
        resolved, tracked_missing = resolve_profile(old.hex, [new])
        self.assertEqual(resolved.altitude, 13500)
        self.assertFalse(tracked_missing)

    def test_selected_missing_is_not_misreported_as_current(self) -> None:
        resolved, tracked_missing = resolve_profile("ABC123", [])
        self.assertIsNone(resolved)
        self.assertFalse(tracked_missing)

    def test_tracked_missing_retains_grace_state(self) -> None:
        resolved, tracked_missing = resolve_profile(
            "ABC123", [], manual_tracking=True, tracked_hex="ABC123"
        )
        self.assertIsNone(resolved)
        self.assertTrue(tracked_missing)

    def test_bounded_capacity_scan(self) -> None:
        targets = [Target(f"{index:06X}", index) for index in range(200)]
        resolved, tracked_missing = resolve_profile("0000C7", targets)
        self.assertEqual(resolved.altitude, 199)
        self.assertFalse(tracked_missing)


if __name__ == "__main__":
    unittest.main()
