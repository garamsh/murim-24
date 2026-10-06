# Evidence for PR #80

Captures of feat/genealogy-panel at a8b2f029b69998b4883d633f745c9a05b0a67b2f, for comparison with art/mockup/genealogy_panel_4x.png. This branch is never merged. The region lives 50 days before recording, so that families have formed.

- genealogy_open.png, genealogy_open_4x.png: frame 30, the panel open on the first person with a spouse and a child: the couple, their child, the details column.
- genealogy_recentred.png, genealogy_recentred_4x.png: frame 60, after the selection moved down and Enter centred the tree on the child: the child with both parents above.
- genealogy_english.png, genealogy_english_4x.png: frame 100, the same panel after the locale switched to English.

Commands, from the project root after make build:

    godot --path . --write-movie <dir>/genealogy.png --quit-after 120 --script capture_genealogy.gd
    godot --headless --script upscale.gd -- <png>...
