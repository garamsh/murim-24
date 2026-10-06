# Writes a 4x nearest-neighbour copy of each PNG named after `--`, beside it as <name>_4x.png,
# so a capture can be laid beside art/mockup/target_screen_4x.png.
# Run: godot --headless --script <this file> -- <png>...
extends SceneTree


func _init() -> void:
	for path in OS.get_cmdline_user_args():
		var image := Image.load_from_file(path)
		image.resize(image.get_width() * 4, image.get_height() * 4, Image.INTERPOLATE_NEAREST)
		image.save_png(path.get_basename() + "_4x.png")
	quit()
