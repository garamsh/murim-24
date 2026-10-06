# Evidence for PR #84

Captures of feat/economy-caps at 35a1a5cc00e1fa858465e6152eedd7d74795c9da. This branch is never merged. The camera is held on the centre of the region's first village, village 0 of seed 2026, an Orthodox village with the player born in it; the script drives the ticks itself, so the view does not follow the player's body.

- economy_spring.png, economy_spring_4x.png: session start, year 1, spring, day 1 (봄 · 1년). The granary stands east of the square and the storehouse in the south-east corner; the millet and rice fields south of the square are sown. The village holds 26 of a cap of 50, with 6,000 measures stored.
- economy_autumn.png, economy_autumn_4x.png: tick 108,001, the first day of autumn (가을 · 1년). Both fields are ripe; the stock stands at 4,440 after 60 days of feeding 26.
- economy_burnt.png, economy_burnt_4x.png: tick 108,482, 481 ticks after the residents of village 4, of the Demonic Cult, were ordered to the village's centre with their skills: every field of the village is burnt, the torched houses show their damaged frames, and raiders stand at the top. With no intact field the village's cap reads 0.

capture_economy.gd prints the date and the core's state of each of the village's fields, the FIELD_* constants (0 sown, 2 ripe, 4 burnt):

    capture_economy: economy_spring.png at tick 3, date { "year": 1, "season": 0, "day": 1 }, village fields [0, 0], population 26 of cap 50, stock 6000
    capture_economy: economy_autumn.png at tick 108001, date { "year": 1, "season": 2, "day": 1 }, village fields [2, 2], population 26 of cap 50, stock 4440
    capture_economy: ordering army 4 of faction 1 to (9, 108): true
    capture_economy: every field burnt after 481 ticks: true
    capture_economy: economy_burnt.png at tick 108482, date { "year": 1, "season": 2, "day": 1 }, village fields [4, 4], population 26 of cap 0, stock 4440

Commands, from the project root after make build:

    godot --path . --script capture_economy.gd -- <dir>
    godot --headless --script upscale.gd -- <png>...
