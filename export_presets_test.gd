# Checks that no release pack holds a script only the tests or a hand run use: every preset in
# export_presets.cfg but the Android bench is exported as a pack, in a Godot of its own, and none
# of the names it holds is a test, a probe, a tool, run_tests.gd or a helper only the tests load.
# Setup: there are release presets, and each pack holds play/play.gd, so its scripts were read.
# Control: the Android bench pack, whose filter leaves nothing out, holds the bench tool it runs,
# so the names a tool is packed under are the ones looked for.
extends RefCounted

const TestSaves := preload("res://extension/test_saves.gd")
const PRESETS := "res://export_presets.cfg"
const BENCH := "Android bench"
const BENCH_TOOL := "world_view/frame_bench_tool.gd"
const GAME := "play/play.gd"
const ENDINGS: Array[String] = ["_test.gd", "_probe.gd", "_tool.gd"]
const TEST_ONLY: Array[String] = ["run_tests.gd", "hud/word_fit.gd", "extension/test_saves.gd"]


func run() -> bool:
	var presets := ConfigFile.new()
	var read := presets.load(PRESETS) == OK
	var releases: Array[String] = []
	for section in presets.get_sections():
		var name: String = presets.get_value(section, "name", "")
		if section.count(".") == 1 and name != BENCH:
			releases.append(name)
	var set_up := read and releases.size() > 0
	var faults: Array[String] = []
	for preset in releases:
		var names := _packed(preset)
		if not names.has(GAME):
			faults.append("%s: %d names, no %s" % [preset, names.size(), GAME])
		for packed in names:
			if TEST_ONLY.has(packed) or ENDINGS.any(func(ending: String) -> bool: return packed.ends_with(ending)):
				faults.append("%s: %s" % [preset, packed])
	var bench := _packed(BENCH)
	var control := bench.has(BENCH_TOOL) and bench.has(GAME)
	var passed := set_up and faults.is_empty() and control
	print("export_presets_test: release presets %s, packed with no test, probe, tool or test helper, faults %s; the %s pack holds %s %s (setup %s): %s" % [
		releases, faults, BENCH, BENCH_TOOL, control, set_up, "pass" if passed else "FAIL"])
	return passed


# Exports `preset` as a zip pack in a Godot of its own and returns the scripts and resources it
# holds by their paths under res://, each once, a script compiled to .gdc or remapped named as its
# .gd, or no names when the export fails.
func _packed(preset: String) -> Array[String]:
	var pack := TestSaves.directory.path_join(preset.replace(" ", "_") + ".zip")
	var output := []
	var code := OS.execute(OS.get_executable_path(), ["--headless", "--path",
		ProjectSettings.globalize_path("res://"), "--export-pack", preset, pack], output, true)
	var names: Array[String] = []
	var zip := ZIPReader.new()
	if code != 0 or zip.open(pack) != OK:
		printerr("export_presets_test: exporting %s exited %d: %s" % [preset, code, "\n".join(output)])
		return names
	for file in zip.get_files():
		var packed := file.trim_suffix(".remap")
		packed = packed.trim_suffix("c") if packed.ends_with(".gdc") else packed
		if not names.has(packed):
			names.append(packed)
	zip.close()
	return names
