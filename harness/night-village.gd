## Throwaway (#364, not committed): #323's night village bench, the production play scene of seed 2026
## at summer midnight at high, the player born in village 7, which starts with a temple and an inn on
## this branch; it reports the hearths on screen, so bloom's share is visible.
extends SceneTree
const WARM_UP := 120
const TIMED := 1200
const BUDGET_MS := 16.7
var play: Node2D
var frame := 0
var last_usec := 0
var ticks_before := 0
var times := PackedFloat64Array()
var visible_structures := 0
var started := false
var waiting := 0
func _initialize() -> void:
	if DisplayServer.get_name() == "headless":
		printerr("night village: a display is required")
		quit(2)
		return
	TranslationServer.set_locale("ko")
	root.size = Vector2i(1920, 1080)
	DisplayServer.window_set_vsync_mode(DisplayServer.VSYNC_DISABLED)
	DisplayServer.window_set_flag(DisplayServer.WINDOW_FLAG_ALWAYS_ON_TOP, true)
	play = load("res://play/play.tscn").instantiate()
	root.add_child(play)
func _process(delta: float) -> bool:
	if not started:
		waiting += 1
		if not play.is_node_ready():
			if waiting > 60:
				printerr("night village: production scene never became ready")
				quit(2)
			return false
		play.set_process(false)
		play.birth_village = 6
		play.start(2026)
		play.get_node("Hud").settings.chosen.screen = 2
		play.show_numbers()
		started = true
	play.call("_process", delta)
	var grade: CanvasLayer = play.get_node("Grade")
	if OS.get_environment("NOBLOOM") == "1":
		# The same lights with no hearth, so bloom does not run: bloom's own cost is the difference.
		var art: Node2D = play.get_node("WorldArt")
		var camera: Camera2D = play.get_node("Camera")
		var screen := Vector2(root.size) / camera.zoom
		var seen := Rect2(camera.position - screen / 2.0, screen)
		grade.show_lights(art.lights(seen), seen, 0, art.lights_version, art.torch(seen))
	# Hold presentation time at summer midnight while the core keeps ticking.
	grade.show_time(0.0, 1)
	frame += 1
	var now := Time.get_ticks_usec()
	if frame == WARM_UP:
		var art: Node2D = play.get_node("WorldArt")
		var camera: Camera2D = play.get_node("Camera")
		var screen := Vector2(root.size) / camera.zoom
		var view := Rect2(camera.position - screen / 2.0, screen)
		for index in play.world.structure_count():
			if view.has_point(art.to_screen(Vector2(play.world.structure_cell(index)) * WorldAdapter.CELL_SIZE)):
				visible_structures += 1
		var kinds := {}
		for index in play.world.structure_count():
			if view.has_point(art.to_screen(Vector2(play.world.structure_cell(index)) * WorldAdapter.CELL_SIZE)):
				var kind: String = play.world.structure_kind(index)
				kinds[kind] = kinds.get(kind, 0) + 1
		print("night village in view: %s, hearths %d" % [kinds, art.hearths])
		var params: Dictionary = grade.params()
		if not grade.visible or not Vector3(params.gain).is_equal_approx(Vector3(0.4692, 0.52, 0.6912)) or params.vignette <= 0 or visible_structures == 0 or art.drawn == 0 or play.world.tick_count() == 0:
			printerr("night village: grade, village, bodies or clock setup failed")
			quit(2)
			return false
		var saved := root.get_texture().get_image().save_png("res://build/issue364b/%s-night-village-bench-ko.png" % OS.get_environment("TAG"))
		if saved != OK:
			quit(2)
			return false
		ticks_before = play.world.tick_count()
		print("night village setup: production play scene seed 2026, Korean HUD, summer midnight at high, gain %s vignette %.2f art pixel %.1f lamp %s bloom %s lights %s, %d structures in view, %d bodies drawn, %s" % [params.gain, params.vignette, params.art_pixel, params.get("lamp"), params.get("bloom"), params.get("light_count"), visible_structures, art.drawn, RenderingServer.get_video_adapter_name()])
	elif frame > WARM_UP:
		times.append((now - last_usec) / 1000.0)
	last_usec = Time.get_ticks_usec()
	if times.size() == TIMED:
		var sorted := times.duplicate()
		sorted.sort()
		var p99 := sorted[ceili(sorted.size() * 0.99) - 1]
		var timed_ticks: int = play.world.tick_count() - ticks_before if play.world != null else 0
		var params: Dictionary = grade.params()
		var live: bool = play.is_inside_tree() and play.world != null and play.get_node("Hud").screen() == null
		if live:
			live = play.steered >= 0 and play.steered == play.world.player_army() and not play.world.roster(play.steered).is_empty()
		var night: bool = grade.visible and grade.quality == 2 \
				and Vector3(params.gain).is_equal_approx(Vector3(0.4692, 0.52, 0.6912)) \
				and Vector3(params.lift).is_equal_approx(Vector3(0.012, 0.018, 0.05)) \
				and Vector3(params.shadow).is_equal_approx(Vector3(0.85, 0.95, 1.25)) \
				and Vector3(params.light).is_equal_approx(Vector3(0.95, 1.0, 1.08)) \
				and is_equal_approx(params.saturation, 0.636) and is_equal_approx(params.contrast, 1.05) \
				and is_equal_approx(params.hue_shift, 1.0) and is_equal_approx(params.vignette, 0.18) \
				and is_equal_approx(params.art_pixel, play.get_node("Camera").zoom.x)
		var bodies: int = play.get_node("WorldArt").drawn
		print("night village end: %d timed ticks, live unpaused player %s, high midnight grade %s, %d bodies drawn" % [timed_ticks, live, night, bodies])
		if timed_ticks <= 0 or not live or not night or bodies <= 0:
			printerr("night village: final live gameplay/effect check failed")
			quit(2)
			return false
		print("night village: %d warm-up then %d timed frames, %d core ticks during timed frames, window %s, vsync off" % [WARM_UP, TIMED, play.world.tick_count() - ticks_before, root.size])
		print("night village frame time: p50 %.3f ms p99 %.3f ms max %.3f ms" % [sorted[sorted.size() / 2], p99, sorted[-1]])
		print("%s: night village p99 %.3f ms against %.1f ms" % ["PASS" if p99 <= BUDGET_MS else "FAIL", p99, BUDGET_MS])
		quit(0 if p99 <= BUDGET_MS else 1)
	if frame > WARM_UP + TIMED + 10:
		quit(2)
	return false
