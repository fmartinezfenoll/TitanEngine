#pragma once

// A small hand-picked subset of Google Material Symbols (Outlined), used as
// glyphs in DebugUI's icon font (see DebugUI::GetIconFont()). Source:
// https://github.com/google/material-design-icons (Apache License 2.0,
// vendored at project/resources/fonts/materialsymbols/).
//
// Each macro is the UTF-8 encoding of the icon's codepoint (looked up in
// MaterialSymbolsOutlined.codepoints), meant to be drawn as text while
// DebugUI::GetIconFont() is the active font.

#define ICON_FOLDER   "\xEE\x8B\x87" // U+E2C7 folder
#define ICON_IMAGE    "\xEE\x8F\xB4" // U+E3F4 image
#define ICON_AUDIO    "\xEE\xAE\x82" // U+EB82 audio_file (reserved for a future AssetKind::Audio)
#define ICON_MODEL    "\xEE\xBF\x89" // U+EFC9 view_in_ar
#define ICON_SCENE    "\xEE\xA3\x9A" // U+E8DA theaters
#define ICON_CODE     "\xEE\xA1\xAF" // U+E86F code
#define ICON_PALETTE  "\xEE\x90\x8A" // U+E40A palette
#define ICON_FILE     "\xEE\xA1\xB3" // U+E873 description

// Every codepoint above, used to build the icon font's glyph range (see
// DebugUI::ApplyTheme()) -- must stay in sync with the macros above.
#define MATERIAL_ICONS_CODEPOINTS \
    0xE2C7, 0xE3F4, 0xEB82, 0xEFC9, 0xE8DA, 0xE86F, 0xE40A, 0xE873
