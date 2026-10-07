# Evidence for M5: one play flow in the exported macOS app

Stills of the exported `build/export/macos/Murim24.app`, built by `make export` from origin/main at c738d28, played through one flow in Korean: title, new game, a few days, pause and save, title, continue, genealogy, sect application, death and rebirth, the new body. Each still is the native 480 x 270 frame of the root viewport, with a `_4x` nearest-neighbour copy. Every file's SHA-256 is the one the driver logged when it saved that frame (`run.log`), and no two of the 24 files share a hash. This branch is never merged.

## The build

`make export` ran in a clean worktree of c738d28. It exited 0 and ended with:

    built build/macos/libmurim.macos.template_release.universal.dylib and build/windows/libmurim.windows.template_release.x86_64.dll, and exported build/export/macos/Murim24.app and build/export/windows/Murim24.exe with the macOS and Windows presets in export_presets.cfg

The bundle that was played, with override.cfg removed again after the run:

    4bef2cb32bc17a7f88f6d6e5d95842ba10bef81f87137635632b6061317f14a3  Contents/MacOS/Murim24
    a9d1671c1426aff433cfa6c709597621c003715c857e7eded79e06fcd8795ba7  Contents/Resources/Murim24.pck
    f66b41e3d4189c8cd92577dd095de00588c32349170b83165d4d3ed6e026fd23  Contents/Frameworks/libmurim.macos.template_release.universal.dylib

## How it was driven

A release export template ignores `--script`. With `Murim24 --script drive.gd -- <dir>` the driver's `_initialize` never ran, and play.tscn opened on its own. So `drive_play_flow.gd` is an autoload `Node`, added by an `override.cfg` placed beside the executable, which a release build still reads (`override.cfg` here is the file used, with the scratch path shortened). The game boots as it ships: play.tscn is the main scene, and the driver acts on the scene the tree is playing.

Every key goes through `get_viewport().push_input()`, so it reaches play.gd's `_unhandled_input` as a key from the window does. The one other input is the march in step 11, a left-click event handed to `play.command()`, the method `_unhandled_input` passes a click to, aimed at a cell outside the view. The world runs on its own frames at 4x, the fastest speed on the settings panel. The driver picks Korean and 4x on that panel, keeps them in a file of its own, and removes that file at the end. It refuses to run if slot 0 already holds a save, and it removes the save it makes. Slot 0 was empty before and after the run.

Commands, with `APP=build/export/macos/Murim24.app`:

    make export
    printf '[autoload]\n\nDrivePlayFlow="*%s"\n' /abs/path/drive_play_flow.gd > $APP/Contents/MacOS/override.cfg
    $APP/Contents/MacOS/Murim24 -- <out dir> > run.log 2>&1   # exited 0
    rm $APP/Contents/MacOS/override.cfg

## Steps

Seed 2026 was typed on the new-game screen. The player is born into village 0, of the Orthodox.

1. `01_title`: the title as the app opens. Continue is greyed with 저장 없음; `load_refusal(0)` is 0, `LOAD_MISSING`.
2. `02_new_game_seed`: Enter on 새 게임, then the keys 2, 0, 2, 6. The screen holds seed 2026.
3. `03_world_days`: Enter, then the world runs until scene tick 4,500, day 3 of spring in year 1. The HUD shows 서일검 of the Orthodox at 검 654, and the rumour log shows 이백랑 태어나다, the one rumour that reached the player.
4. `04_pause`: Escape. The pause panel shows the controls guide and 시드 2026.
5. `05_saved`: Down and Enter on 저장하기. The line under the menu reads 저장했다. (`PAUSE_SAVED`), and `load_refusal(0)` is now -1, loadable.
6. `06_title_again`: Down, Down and Enter on 타이틀로. The scene reloads, `world` is null, and nothing is unavailable.
7. `07_continue_chosen`: Down. 이어하기 is chosen and can be taken.
8. `08_resumed`: Enter on 이어하기, then 120 ticks of the resumed world. Seed 2026, army 10, person 20: the same as when saved.
9. `09_genealogy`: G. The genealogy panel is centred on person 20, 서일검 of the Seo family, 25, with no parents, spouse or children known.
10. `10_sect_applied`: G closes it, M opens the sect panel on 화산파 of the Orthodox, and Enter applies. The verdict reads 본산에서 너무 멀리 산다. (`APPLICATION_TOO_FAR`), and the person stays out of every sect.
11. `11_rebirth`: M closes the panel. A click orders person 20 onto cell (31, 49), the nearest army of the Demonic Cult. It dies at scene tick 1,801, and the rebirth screen sums up 서일검 (서씨 가문, 향년 25세, 문파 없음, 검 654, 자녀 0명) beside the new life, person 22 (오씨 가문, 마교, 0세).
12. `12_new_body`: Enter on 계속. The world plays on with person 22, of the Cult, in army 10. The rumour log shows the new birth and the old death.

## Core hash before save and after load

    before save (scene tick 4,500, read just before Enter on 저장하기):  3479360604247063717
    after load  (read on the frame of Enter on 이어하기, before any tick): 3479360604247063717
    WorldAdapter.load(0).hash(), the slot as written:                    3479360604247063717

Seed, steered army (10) and person (20) are also the same before the save and after the load.

## Observations

- No defect showed in the game's flow.
- Without the 120-tick wait, `08_resumed` came out byte-identical to `03_world_days` on an earlier run (19 ticks after the load). Nothing in view had moved yet. The wait exists only so that the stills' hashes differ.
- The same tick can draw two different frames: an earlier run under the editor binary drew `03_world_days` with the hash this run's `08_resumed` has. Animation phase follows frames, not ticks, so a still's hash shows which frame was saved and does not identify the world state.
- `make export` printed `cannot connect to daemon at tcp:5037: Connection refused` twice: the Android export plugin looking for adb. The export still succeeded.

## Log

`run.log` is the app's whole output for the run, scratch paths shortened. The driver lines:

    drive_play_flow: frame 11, 01_title.png 480x270 sha256 55d687299f5c11fe2b8905f77c04752df1d1f68e6209e2ed0e449a83ca473ae5, 01_title_4x.png sha256 87500a9dbce348c0d4a58b7e7483e91eccfbbcb027b1c633cb629ddb8192711c, locale ko; screen @Control@80, unavailable ["continue"], slot 0 refusal 0
    drive_play_flow: frame 22, 02_new_game_seed.png 480x270 sha256 0c8d516411773b3ed73e6bc889af10a68fac0733fe30036ebcd066bf5ecfa419, 02_new_game_seed_4x.png sha256 4c1cdd8956543ba7d3755a427d508b8746b34f810b7560859dd31cd3e6c4ce48, locale ko; screen @Control@81, typed seed 2026
    drive_play_flow: frame 8244, 03_world_days.png 480x270 sha256 4431b7b858ce596036960763918b33dfcbf8f12ca45c1082e6867f08b5d3f726, 03_world_days_4x.png sha256 70a91e798678d52d4625602686444e7c19fa5e2eb13afba749f66f463958cef9, locale ko; seed 2026, ticks 4500, date { "year": 1, "season": 0, "day": 3 }, person 20, attainment 검 654, rumours 1: 이백랑 태어나다 /  /
    drive_play_flow: frame 8255, 04_pause.png 480x270 sha256 e9ec54a4363e7a98eba395d3b679311023c045387362c38e94106d2b3502bef3, 04_pause_4x.png sha256 9b2312abeef5559eac3c3aeffff8413ef3afcbc4048d7b99c906002358a2c745, locale ko; screen @Control@82, chosen resume, seed shown 2026, ticks 4500
    drive_play_flow: frame 8266, 05_saved.png 480x270 sha256 9342fbd904a5b89c0616851e2e0d35bbc44ce493926586ad90f26d5f6f45157f, 05_saved_4x.png sha256 529195abfc09c22face06f0758389ec17415b59fd6bc38667194f62ffd703cb0, locale ko; chosen save, notice PAUSE_SAVED, core hash before save 3479360604247063717, ticks 4500, person 20, slot 0 refusal -1
    drive_play_flow: frame 8277, 06_title_again.png 480x270 sha256 de320db1103e6309c64bc574ce49f6234126c05144310aa652ee45b91fc498c0, 06_title_again_4x.png sha256 a8efb8d02b9e42a4ef882523cb93b6cc586fdbe1529b6cef40c31f04572f97b9, locale ko; screen @Control@1014, world <null>, chosen new, unavailable []
    drive_play_flow: frame 8288, 07_continue_chosen.png 480x270 sha256 3aa7c7bfc4169b4e6ab33e2a7e03feaf407be6d2aaaa1c21acc6a079bbbef1bd, 07_continue_chosen_4x.png sha256 08d16c2733badf6b173379a63b35cbb8c4fe62f735f0269ed88299dadf6278ef, locale ko; chosen continue, unavailable []
    drive_play_flow: frame 8615, 08_resumed.png 480x270 sha256 85733a029d67e180a1ec529df09c14091905cdc3a8c3d93aaf06c0364ac3866c, 08_resumed_4x.png sha256 f2b1010e4e3526c6ec1864519cfb8c18b48082bf8ad26604d65403b4ea06ada7, locale ko; seed 2026, core hash after load 3479360604247063717 at scene tick 0 (before save 3479360604247063717, equal true; slot 0 holds 3479360604247063717), steered 10 (saved 10), person 20 (saved 20), scene ticks now 120
    drive_play_flow: frame 8626, 09_genealogy.png 480x270 sha256 97e5f14c13e214de1a7072c9fa1eaf953298a4c6a10fb2e92d1cce9f5235ae5f, 09_genealogy_4x.png sha256 913026119369bb1386eca30a911472f50f135c4c7f608c29b33a948b5b6de098, locale ko; visible true, centre 20, own 20, cards 1
    drive_play_flow: frame 8637, 10_sect_applied.png 480x270 sha256 95292ede21aa3582cd2db4b2bf92225a162b9ff0c7339448a82ae5ab86e98930, 10_sect_applied_4x.png sha256 05c77de2bb819f64ccb865e1776212c67c687dfb459b66410e7eb06d320f7120, locale ko; visible true, faction 0, sect 0, verdict APPLICATION_TOO_FAR, person's sect now -1
    drive_play_flow: frame 8637, body 20 of faction 0 marches on cell (31, 49) of faction 1 at tick 143; order accepted true
    drive_play_flow: frame 11432, 11_rebirth.png 480x270 sha256 21771121c077d2d46b7072ad9927f2c0be66e9ba613ca7a57020a94009d1005c, 11_rebirth_4x.png sha256 a2e6d19b92d7c739db72437787fbd99b49cde2815a9eb3bdabf6ee4cba53fda3, locale ko; body 20 died at tick 1801; past 20 next 22: [...]
    drive_play_flow: frame 11443, 12_new_body.png 480x270 sha256 0b902d984ee89ed529d5c66fb66284bcd2a0438865469a9957ae43ac800ff856, 12_new_body_4x.png sha256 a0750dbb0259351af001486a5f81e01784af03d354cad69667fe9d0802eef8b2, locale ko; screen <none>, steered 10, person 22 (was 20), ticks 1816
    drive_play_flow: 24 files, none repeated true

The rebirth line is cut at `[...]` here. `run.log` holds it whole, with the rows step 11 lists.
