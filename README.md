# Evidence for ADR 0047 (issue #201)

Two Python 3 programs, standard library only, behind the figures ADR 0047 quotes.

- `routes.py` — the draft anchor places of the hand-made map in cells on the 4,096-cell map, with each place's real longitude and latitude. It prints, for each key route, the length of its legs through its gates in cells, the real great-circle length of the same legs in kilometres, and the times on foot, by road, mounted and by boat at the speeds ADR 0047 gives. Run: `python3 routes.py`.
- `proxy.py` — a synthetic stand-in for the compiled map: four bytes a cell (ground kind, elevation, biome, region) from macro layers at one value per 8 x 8 cells, borders dithered by up to 3 cells, four rivers and four roads, stored per 32-cell area. It compresses each sampled area with deflate (`zlib`, level 6) and extrapolates to the 16,384 areas. Run: `python3 proxy.py 4` (every fourth area on each axis, 1,024 areas).
- `proxy_noisy.py` — the same with a tenth of the cells' biome changed and a sixteenth of the cells raised a tier, as an upper case for per-cell detail. Run: `python3 proxy_noisy.py 4`.

Results on the development Mac (Apple M1, Python 3), 2026-10-09:

- `proxy.py 4`: raw 4,096 bytes an area, deflate 176 bytes an area on average, 568 at most; the whole map 64.0 MiB raw, 2.75 MiB compressed.
- `proxy_noisy.py 4`: deflate 332 bytes an area on average, 759 at most; the whole map 5.18 MiB compressed.

The real map's content will differ; the implementing pull request measures the compiled map itself.
