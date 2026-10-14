## make bench's frame benchmark, the M1 battle, with the grade at high and raining: every effect of
## #209 on but the clouds' shadows, which the rain's overcast turns off. Prints the grade's setup
## once the warm-up ends.
extends "res://world_view/frame_bench.gd"


func _initialize() -> void:
	super()
	if _view != null:
		var grade: CanvasLayer = _view.get_node("Grade")
		grade.quality = 2
		grade.raining = true


func _process(delta: float) -> bool:
	if _frame == WARM_UP_FRAMES - 1:
		var p: Dictionary = _view.get_node("Grade").params()
		print("m1 rain setup: quality high, rain %s fog %s clouds %s lamp %s bloom %s lights %s vignette %s" % [
			p.get("rain"), p.get("fog"), p.get("clouds"), p.get("lamp"), p.get("bloom"), p.get("light_count"), p.get("vignette")])
	return super(delta)
