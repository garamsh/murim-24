# Evidence for PR #134

Captures of feat/frame at baebcb8, the PR's head, in a 480 x 270 window, each with a _4x nearest-neighbour copy. This branch is never merged.

capture_screens.gd drives play.tscn as a player does, through play.gd's navigate(). It fails if any two images repeat. The language is chosen on the settings panel, kept in a file of the script's own, which it removes at the end.

- title_ko.png, title_en.png: the scene as it opens. The backdrop is art/title/backdrop.png. Continue is greyed with 준비 중 / Soon and cannot be chosen.
- new_game_ko.png, new_game_en.png: after Enter on a new game and the digits 4817203365 typed.
- pause_ko.png, pause_en.png: after Enter started that world and Escape paused it. The seed is 4817203365. The guide is play.gd's controls(), drawn from its key constants: W A S D, the click, 1-9 (one skill), 0 (all skills), C, G, M, F2 and Escape.
- settings_ko.png, settings_en.png: after Down and Enter on the pause panel's settings. The English one has English lit on the language row.

The capture logged:

    capture_screens: title_ko.png 480x270, sha256 308209087602e77c, locale ko; screen res://hud/title.gd
    capture_screens: new_game_ko.png 480x270, sha256 f0e51edb3f35d52e, locale ko; typed 4817203365
    capture_screens: pause_ko.png 480x270, sha256 884127bfec3bd324, locale ko; seed 4817203365, ticks 4, guide ["[\"W\", \"A\", \"S\", \"D\"] 이동", "[\"CONTROLS_CLICK\"] 그곳으로 이동", "[\"1-9\"] 기술 하나", "[\"0\"] 모든 기술", "[\"C\"] 수장에게 도전", "[\"G\"] 족보", "[\"M\"] 문파", "[\"F2\"] 언어", "[\"Escape\"] 일시정지"]
    capture_screens: settings_ko.png 480x270, sha256 1fbac65e5161b3da, locale ko; chosen { "speed": 0, "window": 0, "resolution": 2, "language": 0 }, speed 1x
    capture_screens: title_en.png 480x270, sha256 0d32b614c16996b3, locale en; screen res://hud/title.gd
    capture_screens: new_game_en.png 480x270, sha256 2b6d2180bdd72b3b, locale en; typed 4817203365
    capture_screens: pause_en.png 480x270, sha256 068d94c870f9480c, locale en; seed 4817203365, ticks 4, guide ["[\"W\", \"A\", \"S\", \"D\"] Move", "[\"CONTROLS_CLICK\"] Go there", "[\"1-9\"] One skill", "[\"0\"] All skills", "[\"C\"] Challenge leader", "[\"G\"] Genealogy", "[\"M\"] Sects", "[\"F2\"] Language", "[\"Escape\"] Pause"]
    capture_screens: settings_en.png 480x270, sha256 b57d2769eb0d8af2, locale en; chosen { "speed": 0, "window": 0, "resolution": 2, "language": 1 }, speed 1x
    capture_screens: 8 images, none repeated true

compare_panels.gd draws the genealogy and sect panels over seed 2026 at tick 0, in each locale. It ran once with main's hud/panel.gd and hud/sect.gd (7b12870) copied in, and once with this branch's, to show that moving _chrome onto _dialog changed no pixel. Main, then branch:

    compare_panels: start en_KR f3ee178cb82db8e0	compare_panels: start en_KR f3ee178cb82db8e0
    compare_panels: genealogy ko 575ba8c7d4471a59	compare_panels: genealogy ko 575ba8c7d4471a59
    compare_panels: sect ko b9ec7cd18679be00	compare_panels: sect ko b9ec7cd18679be00
    compare_panels: genealogy en 2151d007a1439ca5	compare_panels: genealogy en 2151d007a1439ca5
    compare_panels: sect en 5fab2c97b6fdc29c	compare_panels: sect en 5fab2c97b6fdc29c

## File hashes

SHA-256 of each file, first 16 hex digits:

    c2e83fd4c3f9726c  new_game_en_4x.png
    ba6883a7675cc12d  new_game_en.png
    54cbe07bf287b15a  new_game_ko_4x.png
    82adc5ada3dbf74d  new_game_ko.png
    7d249625e41c1780  pause_en_4x.png
    2de3643329cd4be3  pause_en.png
    26effd525475b7db  pause_ko_4x.png
    b8207d1cc206066b  pause_ko.png
    2fe3315b5f8e2c44  settings_en_4x.png
    ff2acb1b60ec7488  settings_en.png
    8ca7b7740ab52cd6  settings_ko_4x.png
    2147252fe6e0542f  settings_ko.png
    3b3555f95c34a6be  title_en_4x.png
    e5b9a35026595056  title_en.png
    069e25beb7c27e06  title_ko_4x.png
    2a68e793156ccc89  title_ko.png
    dba24d0bf3d61a20  capture_screens.gd
    128effc932162252  compare_panels.gd
