# Speech-bubble font

`glyphs.json` is a 9 px bitmap font for Tomo's speech bubble: ASCII plus
every kana and kanji his lines use (about 180 glyphs). Each entry is
`{char: [advance, [11 row bitstrings]]}`, with `"1"` for an inked pixel.
`tools/sprites/build.py` turns it into the `sprite_glyphs` table in
`sprites_gen.c`, and `components/scene` draws the bubble with it.

Characters missing from the font draw as `?`. If you add a line in
`components/pet/pet.c` with a new character, add its glyph here and rerun
`python tools/sprites/build.py`.

## Where it comes from and licence

The glyphs are cut from the 9 px bitmap strike of **WenQuanYi Zen Hei Sharp**
(文泉驿正黑点阵), by the WenQuanYi project. That font is licensed under the
GNU GPL v2 (or later) with a font embedding exception. This subset is a
derivative of it and stays under the same licence. The rest of this repo
is not relicensed by it.
