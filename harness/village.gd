extends SceneTree
# The share of the day held, 0.0 midnight by default; AT=0.3 holds the morning, fog and clouds thickest.
# QUALITY 0 low, 1 standard, 2 high (default); HEARTH=1 lists a hearth each frame, so the bloom runs.
var QUALITY := int(OS.get_environment("QUALITY")) if OS.get_environment("QUALITY") != "" else 2
var HEARTH := OS.get_environment("HEARTH") == "1"
var AT := float(OS.get_environment("AT")) if OS.get_environment("AT") != "" else 0.0
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
		play.start(2026)
		play.get_node("Hud").settings.chosen.screen = QUALITY
		play.show_numbers()
		started = true
	play.call("_process", delta)
	var grade: CanvasLayer = play.get_node("Grade")
	# Hold presentation time at summer midnight while the core keeps ticking.
	grade.show_time(AT, 1)
	if OS.get_environment("NOLIGHTS") == "1":
		var c2: Camera2D = play.get_node("Camera")
		var s2 := Vector2(root.size) / c2.zoom
		grade.show_lights(PackedFloat32Array(), Rect2(c2.position - s2 / 2.0, s2))
	if HEARTH:
		var cam: Camera2D = play.get_node("Camera")
		var seen := Vector2(root.size) / cam.zoom
		var shown := Rect2(cam.position - seen / 2.0, seen)
		grade.show_lights(play.get_node("WorldArt").lights(shown), shown, 1)
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
		var params: Dictionary = grade.params()
		if not grade.visible or (AT == 0.0 and not Vector3(params.gain).is_equal_approx(Vector3(0.4692, 0.52, 0.6912))) or (QUALITY > 0 and params.vignette <= 0) or visible_structures == 0 or art.drawn == 0 or play.world.tick_count() == 0:
			printerr("night village: grade, village, bodies or clock setup failed")
			quit(2)
			return false
		var saved := root.get_texture().get_image().save_png("res://build/issue209/r1/%s-village-%s-q%d-h%s-bench-ko.png" % [OS.get_environment("TAG"), AT, QUALITY, HEARTH])
		if saved != OK:
			quit(2)
			return false
		ticks_before = play.world.tick_count()
		print("village at %s quality %d hearth %s setup: bloom %s fog %s clouds %s;" % [AT, QUALITY, HEARTH, grade.params().get("bloom"), grade.params().get("fog"), grade.params().get("clouds")]); print("night village setup: production play scene seed 2026, Korean HUD, summer midnight at high, gain %s vignette %.2f art pixel %.1f lamp %s bloom %s lights %s, %d structures in view, %d bodies drawn, %s" % [params.gain, params.vignette, params.art_pixel, params.get("lamp"), params.get("bloom"), params.get("light_count"), visible_structures, art.drawn, RenderingServer.get_video_adapter_name()])
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
		var night: bool = grade.visible and grade.quality == QUALITY \
				and (AT != 0.0 or Vector3(params.gain).is_equal_approx(Vector3(0.4692, 0.52, 0.6912)) \
				and Vector3(params.lift).is_equal_approx(Vector3(0.012, 0.018, 0.05)) \
				and Vector3(params.shadow).is_equal_approx(Vector3(0.85, 0.95, 1.25)) \
				and Vector3(params.light).is_equal_approx(Vector3(0.95, 1.0, 1.08)) \
				and is_equal_approx(params.saturation, 0.636) and is_equal_approx(params.contrast, 1.05)) \
				and is_equal_approx(params.hue_shift, 1.0) and is_equal_approx(params.vignette, 0.0 if QUALITY == 0 else 0.18) \
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
