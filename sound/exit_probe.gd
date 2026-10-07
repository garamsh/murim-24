## Starts every kind's sound, frees what plays it once the audio server has mixed it, then quits on
## the same step, so the exit log shows whatever the server still holds. `sound_test.gd` runs it in
## a Godot of its own: after `--`, "scene" plays them through sound.tscn and "bare" through a
## player each with nothing else.
extends SceneTree

const Sound := preload("res://sound/sound.gd")
const SCENE := "res://sound/sound.tscn"

var _holder: Node2D


# A 2D player hands its sound to the audio server on the physics step after it plays, so the
# sounds start on the first step and are freed on the second.
func _physics_process(_delta: float) -> bool:
	if _holder == null:
		_start()
		return false
	Sound.await_mix()
	var playing := 0
	for player: Node in _holder.get_children():
		if player.playing:
			playing += 1
	print("exit_probe: %d playing" % playing)
	_holder.free()
	return true


func _start() -> void:
	if OS.get_cmdline_user_args() == PackedStringArray(["scene"]):
		_holder = load(SCENE).instantiate()
		root.add_child(_holder)
		_holder.hearing = Rect2(-1e6, -1e6, 2e6, 2e6)
		for kind in Sound.STREAMS.size():
			_holder.hear(kind, Vector2.ZERO)
	else:
		_holder = Node2D.new()
		root.add_child(_holder)
		for stream in Sound.STREAMS:
			var player := AudioStreamPlayer2D.new()
			player.stream = stream
			_holder.add_child(player)
			player.play()
