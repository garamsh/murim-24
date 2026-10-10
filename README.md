# Evidence for the 2.5D port's bodies (#200, PR B, #235)

Captures at 1920 x 1080 in Korean, before (main 031cd31) and after (branch `feat/bodies-25d` at cc9230a, stacked on #226), on the development Mac (Apple M1, Godot 4.7.2).

- `before_behind_house_3x_ko.png`, `after_behind_house_3x_ko.png`, and their `_crop` at 3x: seed 2026's player walked to the cell just north of the house nearest it (`world.order` to that cell, then 196 ticks). After, it is behind the roof, drawn as its azure outline over it; its head shows above the ridge.
- `before_m1_3x.png`, `after_m1_3x.png`, and their `_crop` at 3x: the M1 battle `make bench` renders. Soldiers behind a tree are outlined over it, and none are outlined over the soldiers in front of them. Each body has its shadow, and those walking away show their backs.
- `before_player_3x_ko.png`, `after_player_3x_ko.png`: the player's village at the play scene's 3x.
