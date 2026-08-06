from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
RENDERER = (ROOT / "src" / "radar_renderer.cpp").read_text(encoding="utf-8")
BUILD_INFO = (ROOT / "include" / "build_info.h").read_text(encoding="utf-8")


def test_product_79_marker_is_present():
    assert "FIRMWARE_VERSION_CODE = 79" in BUILD_INFO
    assert 'FIRMWARE_VERSION_LABEL = "Product 79"' in BUILD_INFO
    assert '7IN-20260806-PRODUCT79-RANGE-SYMBOLS' in BUILD_INFO


def test_range_symbol_sizes_are_fixed_and_bounded():
    assert "CONTACT_20_MILE_SIZE = 25" in RENDERER
    assert "CONTACT_40_MILE_SIZE = 17" in RENDERER
    assert "CONTACT_80_MILE_SIZE = 11" in RENDERER
    assert "destinationY * RADAR_CONTACT_BITMAP_H / destinationSize" in RENDERER
    assert "destinationX * RADAR_CONTACT_BITMAP_W / destinationSize" in RENDERER


def test_40_mile_heading_and_80_mile_fixed_orientation():
    assert re.search(
        r"renderedHeadingIndex\s*=\s*\n\s*rangeMiles <= 40\.1f "
        r"\? screen\.headingIndex : 0;",
        RENDERER,
    )


def test_no_dot_substitution_or_render_loop_allocation():
    draw_start = RENDERER.index("void drawContacts(")
    draw_end = RENDERER.index("void drawContactLabels(", draw_start)
    draw_contacts = RENDERER[draw_start:draw_end]
    assert "fillCircle(screen.x, screen.y" not in draw_contacts
    assert "malloc(" not in draw_contacts
    assert "calloc(" not in draw_contacts
    assert "new " not in draw_contacts
    assert "String" not in draw_contacts


def test_state_colors_and_stable_hit_testing_remain():
    assert "screen.tracked\n            ? red" in RENDERER
    assert "screen.selected\n                   ? amber" in RENDERER
    assert "strncpy(result.hex, hit.hex" in RENDERER
    assert "priorityFor" in RENDERER
