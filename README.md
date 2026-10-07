# Evidence for PR #121

Captures of feat/sect-panel at 5f11b232211c5212929f08383e482ffc3ab94c49, the PR's head, in a 480 x 270 window, each with a _4x nearest-neighbour copy. This branch is never merged. capture_sect.gd fails if any two images repeat.

- play_sect.png: play.tscn on seed 2026 at tick 0, the player born in village 0, after play.gd took M. The player belongs to no sect yet, so the panel opens on Hwasan, the first sect of the player's Orthodox faction, with no "본인 소속" tag. Hwasan has its two founders, the master the only elder, and 2 of 20 members. No duel tag is drawn, because the Orthodox elect.
- sect_ko.png: the region of seed 2026 with days of five ticks after 60 years. The panel is opened on army 1, whose lead person (31) belongs to the Heavenly Demon, so the title carries the "본인 소속" tag. The sect has 18 of 20 members and 5 elders. The master's card comes first and three other elders follow. The Cult's column carries the duel tag.
- sect_en.png: the same after switching to English, with the shortened faction tabs Orth, Cult and Unor.

The capture logged (each line cut after the panel's details):

    capture_sect: M opens the panel true
    capture_sect: play_sect.png 480x270, sha256 d200a301bf86d438, locale ko; faction 0, sect 0, own sect -1; details { "name": "화산파", "faction": "정파", "master": "서소운", "elders": "1명", "members": 2, "cap": 20, "seat": "held" }; cards [{ "person": 14, "name": "서소운", "master": true, "line": "20세" }]; column { "tabs": ["정파", "마교", "사파"], "leader": "황보도윤", "leader_sect": "", "duel": false, "sects": [{ 
    capture_sect: after 60 years army 1's lead person 31 in sect 1
    capture_sect: sect_ko.png 480x270, sha256 388a191d15689b44, locale ko; faction 1, sect 1, own sect 1; details { "name": "천마신교", "faction": "마교", "master": "조월향", "elders": "5명", "members": 18, "cap": 20, "seat": "held" }; cards [{ "person": 51, "name": "조월향", "master": true, "line": "43세" }, { "person": 31, "name": "송일검", "master": false, "line": "53세" }, { "person": 46, "name": "송수련", "master
    capture_sect: sect_en.png 480x270, sha256 38f5a57082b86f73, locale en; faction 1, sect 1, own sect 1; details { "name": "Heavenly Demon", "faction": "Demonic Cult", "master": "Wolhyang Jo", "elders": "5", "members": 18, "cap": 20, "seat": "held" }; cards [{ "person": 51, "name": "Wolhyang", "master": true, "line": "Age 43" }, { "person": 31, "name": "Ilgeom", "master": false, "line": "Age 53" 

compare_genealogy.gd draws the genealogy panel twice, as main's hud/genealogy.gd (e21d2dd, copied in for the run) and as this branch's, over the same person after 70 days of seed 2026. It logged identical images in both locales:

    compare_genealogy: ko main f3ee178cb82db8e0 branch f3ee178cb82db8e0 identical true
    compare_genealogy: en main 5c915fe5297e6f98 branch 5c915fe5297e6f98 identical true

## File hashes

SHA-256 of each file, first 16 hex digits:

    ba8099d68658e57e  play_sect.png
    6fad6c8dd81d9520  play_sect_4x.png
    03d7a02fe162a4e6  sect_ko.png
    9acad2bf8a2194a7  sect_ko_4x.png
    16a43052ce1e8409  sect_en.png
    0876769d47276a1d  sect_en_4x.png

## Commands

Run from the project root after make build:

    godot --path . --resolution 480x270 --script capture_sect.gd -- <dir>
    godot --path . --resolution 480x270 --script compare_genealogy.gd   # with main's hud/genealogy.gd copied to hud/zz_genealogy_main.gd
