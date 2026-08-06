#!/usr/bin/env python3
"""Focused Product 78 page-entry scroll and priority-heading checks."""

from __future__ import annotations

import unittest
from dataclasses import dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
UI_SOURCE = (ROOT / "src" / "ui.cpp").read_text(encoding="utf-8")
RADAR_SOURCE = (ROOT / "src" / "radar_renderer.cpp").read_text(encoding="utf-8")
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


@dataclass
class PageScroll:
    position: int

    def refresh_in_place(self) -> None:
        pass

    def enter_after_rows_ready(self) -> None:
        self.position = 0


class Product78SourceTests(unittest.TestCase):
    def test_product_marker(self) -> None:
        self.assertIn("FIRMWARE_VERSION_CODE = 78", BUILD_INFO)
        self.assertIn('FIRMWARE_VERSION_LABEL = "Product 78"', BUILD_INFO)
        self.assertIn('"7IN-20260805-PRODUCT78-PAGE-TOP-RESET"', BUILD_INFO)

    def test_priority_heading_uses_compact_font(self) -> None:
        creation = UI_SOURCE[UI_SOURCE.index("leftOtherModeLabel = makeLabel") :]
        creation = creation[: creation.index("for (uint8_t i = 0; i < PRIORITY_OTHER_COUNT")]
        self.assertIn("&lv_font_montserrat_14", creation)
        self.assertNotIn("&lv_font_montserrat_16", creation)
        self.assertIn("lv_obj_set_width(leftOtherModeLabel, 112)", creation)
        for state in ("NEAR SELECT", "NEAR TRACK", "POSITION LOST", "NO OTHER"):
            self.assertIn(state, RADAR_SOURCE)

    def test_reset_helper_is_bounded_and_allocation_free(self) -> None:
        body = function_body(UI_SOURCE, "void resetScrollToTop(lv_obj_t* object)")
        self.assertIn("lv_obj_scroll_to_y(object, 0, LV_ANIM_OFF)", body)
        self.assertIn("lv_obj_update_layout(object)", body)
        for forbidden in ("malloc(", "calloc(", "realloc(", "new ", "String("):
            self.assertNotIn(forbidden, body)

    def test_tracks_page_resets_after_rows_are_rendered(self) -> None:
        body = function_body(UI_SOURCE, "void selectPage(uint8_t page) {")
        self.assertIn("if (currentPage == 1)", body)
        self.assertIn("resetScrollToTop(tracksTable)", body)
        self.assertLess(body.index("updatePageContent();"), body.index("resetScrollToTop(tracksTable)"))

    def test_airports_page_resets_after_directory_is_rendered(self) -> None:
        body = function_body(UI_SOURCE, "void selectPage(uint8_t page) {")
        self.assertIn("currentPage == 3 && airportView == AirportView::DIRECTORY", body)
        self.assertIn("resetScrollToTop(airportDirectoryTable)", body)
        self.assertLess(
            body.index("updatePageContent();"),
            body.index("resetScrollToTop(airportDirectoryTable)"),
        )

    def test_tracks_profile_return_resets_after_refresh(self) -> None:
        body = function_body(UI_SOURCE, "void detailBackEvent(lv_event_t*)")
        self.assertIn("updatePageContent();", body)
        self.assertIn("resetScrollToTop(tracksTable)", body)
        self.assertLess(body.index("updatePageContent();"), body.index("resetScrollToTop(tracksTable)"))

    def test_airport_directory_return_resets_after_refresh(self) -> None:
        body = function_body(UI_SOURCE, "void showAirportDirectory()")
        self.assertIn("updateAirportDirectory();", body)
        self.assertIn("resetScrollToTop(airportDirectoryTable)", body)
        self.assertLess(
            body.index("updateAirportDirectory();"),
            body.index("resetScrollToTop(airportDirectoryTable)"),
        )

    def test_in_page_tracks_refresh_still_preserves_scroll(self) -> None:
        body = function_body(UI_SOURCE, "void renderTracksPage()")
        self.assertIn("const lv_coord_t previousScrollY = lv_obj_get_scroll_y(tracksTable)", body)
        self.assertIn("restoreTracksScrollPosition(previousScrollY)", body)
        self.assertNotIn("resetScrollToTop", body)

    def test_in_page_airport_refresh_does_not_force_top(self) -> None:
        body = function_body(UI_SOURCE, "void updateAirportDirectory()")
        self.assertNotIn("resetScrollToTop", body)

    def test_no_scroll_timer_or_new_page_object(self) -> None:
        focused = "\n".join(
            function_body(UI_SOURCE, signature)
            for signature in (
                "void resetScrollToTop(lv_obj_t* object)",
                "void showAirportDirectory()",
                "void detailBackEvent(lv_event_t*)",
                "void selectPage(uint8_t page) {",
            )
        )
        for forbidden in ("lv_timer_create", "lv_obj_create", "lv_table_create", "malloc(", "calloc("):
            self.assertNotIn(forbidden, focused)


class Product78BehaviorModelTests(unittest.TestCase):
    def test_navigation_entry_resets_top(self) -> None:
        page = PageScroll(position=184)
        page.enter_after_rows_ready()
        self.assertEqual(page.position, 0)

    def test_live_refresh_preserves_current_position(self) -> None:
        page = PageScroll(position=136)
        page.refresh_in_place()
        self.assertEqual(page.position, 136)

    def test_reentry_resets_after_scrolling_again(self) -> None:
        page = PageScroll(position=0)
        page.position = 212
        page.enter_after_rows_ready()
        self.assertEqual(page.position, 0)


if __name__ == "__main__":
    unittest.main()
