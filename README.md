# Evidence for PR #143

Captures of play.tscn at the native 480 x 270 over the region of seed 2026, recorded by capture_play_loop.gd at feat/play-loop-ui 6f69b77, in Korean (`_ko`) and English (`_en`). This branch is never merged.

- admitted_ko.png, admitted_en.png: born into village 1, of the Demonic Cult, the player opens the sect panel with M and applies with Enter; the Heavenly Demon admits it, the verdict under the cards reads admitted, and the title carries the player's tag.
- refused_ko.png, refused_en.png: born into village 2, of the Unorthodox, the same keys; the Black Fang refuses it, the verdict reads that it has not proven itself (`APPLICATION_UNPROVEN`), and it stays out.
- rumours_ko.png, rumours_en.png: born into village 0, the same region with days of five ticks lived ten years, so that rumours have come; the rumour log shows the latest three that reach the player, newest first, and the player panel its art.
- rebirth_ko.png, rebirth_en.png: born into village 1, the Cult body ordered on the nearest Orthodox village's residents dies there after 1,618 ticks; on that tick its soul is born again and the rebirth screen sums up the life ended beside the life begun.

The capture's log lines:

    capture_play_loop: born in village 1, verdict 0, sect now 1
    capture_play_loop: born in village 1, verdict 0, sect now 1
    capture_play_loop: born in village 2, verdict 6, sect now -1
    capture_play_loop: born in village 2, verdict 6, sect now -1
    capture_play_loop: 6 rumours reach the player; shown ["황보소연 태어나다", "이도윤 태어나다", "백우진 태어나다"]
    capture_play_loop: 6 rumours reach the player; shown ["Soyeon born", "Doyun born", "Ujin born"]
    capture_play_loop: body 20 died after 1618 ticks; the rebirth screen shows 20 and 21: [{ "label": "REBIRTH_FAMILY", "value": "서씨 가문" }, { "label": "REBIRTH_YEARS", "value": "25세" }, { "label": "REBIRTH_SECT", "value": "없음" }, { "label": "REBIRTH_ATTAINMENT", "value": "검 80" }, { "label": "REBIRTH_CHILDREN", "value": "0명" }] and [{ "label": "REBIRTH_FAMILY", "value": "송씨 가문" }, { "label": "REBIRTH_FACTION", "value": "마교" }, { "label": "REBIRTH_AGE", "value": "0세" }]
    capture_play_loop: body 20 died after 1618 ticks; the rebirth screen shows 20 and 21: [{ "label": "REBIRTH_FAMILY", "value": "Seo family" }, { "label": "REBIRTH_YEARS", "value": "25 years" }, { "label": "REBIRTH_SECT", "value": "None" }, { "label": "REBIRTH_ATTAINMENT", "value": "Sword 80" }, { "label": "REBIRTH_CHILDREN", "value": "0" }] and [{ "label": "REBIRTH_FAMILY", "value": "Song family" }, { "label": "REBIRTH_FACTION", "value": "Demonic Cult" }, { "label": "REBIRTH_AGE", "value": "Age 0" }]

Command, from the project root after make build:

    godot --path . --resolution 480x270 --script capture_play_loop.gd -- <dir>
