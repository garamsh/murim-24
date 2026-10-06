# hud/fonts

The fonts the HUD draws its text in, each from an outside project, with its licence beside it.

- `Galmuri9.ttf` — Galmuri v2.40.4 (github.com/quiple/galmuri, release asset `Galmuri-v2.40.4.zip`, SHA-256 `c8b3d9861a62ae73c8b1178091401cd79994812437ef386413f6dd54856e60e7`), SIL Open Font License 1.1 (`LICENSE.txt`).
- `Galmuri9.bdf` — the same release's bitmap form of Galmuri9, which `art/mockup/bdf.lua` reads to draw Hangul into the mockups with Aseprite alone; the game does not load it. Galmuri9.ttf at font size 10, the HUD's size, draws the same pixels: the two agree row for row.
