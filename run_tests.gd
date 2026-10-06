# Runs each GDScript test named after `--` and exits non-zero unless every one loaded and passed.
# A test is a script whose `run()` returns true on success; a script that fails to parse, such as
# one naming a class from an extension that did not load, counts as a failure.
extends SceneTree


func _init() -> void:
	var paths := OS.get_cmdline_user_args()
	var passed := 0
	for path in paths:
		var script := load(path) as GDScript
		if script != null and script.can_instantiate() and script.new().run():
			passed += 1
		else:
			printerr("%s: FAIL" % path)
	print("run_tests: %d of %d GDScript tests passed" % [passed, paths.size()])
	quit(0 if paths.size() > 0 and passed == paths.size() else 1)
