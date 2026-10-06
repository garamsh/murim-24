# Evidence for PR #82

Captures of fix/genealogy-generation at 241591bf1961a1c2621cb2c3587a1fb6689dd71a. This branch is never merged. The region lives 50 days before recording, so that families have formed.

- genealogy_own.png, genealogy_own_4x.png: frame 30, after G through play.command opened the panel on the steered person: their card in the jade frame with the 본인 tag, and the header reading 1대, as a person the world began with.
- genealogy_child.png, genealogy_child_4x.png: frame 90, the panel re-centred by Down and Enter on the child of the first person with a spouse and a child: both parents above, and the header reading 2대.

Commands, from the project root after make build:

    godot --path . --write-movie <dir>/generation.png --quit-after 100 --script capture_generation.gd
    godot --headless --script upscale.gd -- <png>...
