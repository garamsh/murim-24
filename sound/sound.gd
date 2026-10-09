## Plays what the core reports as sound. After each tick it reads the core's events recorded since
## the last: a death is a fall, and a raid sent against a village rings its bell. For what the core
## records no event of, it compares what the adapter reads with what it read on the tick before: a
## body whose ready tick moved struck, a living body whose health fell was hit, a structure a stage
## worse caught fire, and a structure a stage better was mended. Each is
## heard where it happened, within `hearing`, through a fixed pool of voices on the Effects bus:
## at most one of each kind starts a tick, and once every voice sounds, the rest are dropped. The
## music on the Music bus is the battle track while strikes are heard, and the explore track
## otherwise. The UI's clicks and panels sound through a voice of their own. It holds no game rule.
extends Node2D

enum Heard { STRIKE, HIT, FALL, FIRE, MEND, BELL }

## Each kind's sound, by Heard.
const STREAMS: Array[AudioStream] = [
	preload("res://sound/strike.wav"), preload("res://sound/hit.wav"), preload("res://sound/fall.wav"),
	preload("res://sound/fire.wav"), preload("res://sound/mend.wav"), preload("res://sound/bell.wav")]
const UI_STREAMS := {"click": preload("res://sound/click.wav"), "open": preload("res://sound/open.wav")}
const EXPLORE := preload("res://sound/explore.wav")
const BATTLE := preload("res://sound/battle.wav")
## How many effects may sound at once.
const VOICES := 12
## How long the battle track plays on after the last strike heard, in seconds.
const BATTLE_HOLD := 8.0
const PIXELS_PER_CELL := 16.0
# How far past a voice's position it fades out, in pixels.
const VOICE_DISTANCE := 480.0
# How long await_mix waits for the audio server to start a mix, in microseconds, before it gives up.
const MIX_WAIT_USEC := 1000000

## The part of the world sounds are heard from, in pixels; set by the scene each frame.
var hearing := Rect2()
## How many events of each kind were reported, and how many of them started a voice, by Heard.
var heard := PackedInt64Array()
var played := PackedInt64Array()
## Events heard within `hearing` that found no free voice.
var dropped := 0

var _world: WorldAdapter
var _tick := 0
# Seconds since the scene started, advanced each frame; a voice sounds until its stream ends.
var _now := 0.0
var _voices: Array[AudioStreamPlayer2D] = []
var _until := PackedFloat64Array()
var _started := PackedInt64Array()
var _battle_until := -1.0
var _music: AudioStreamPlayer
var _ui: AudioStreamPlayer
# By person: the tick it was last read alive, its health and its ready tick.
var _seen := PackedInt64Array()
var _health := PackedInt64Array()
var _ready_at := PackedInt64Array()
# By structure, its stage.
var _stages := PackedInt64Array()
# The sequence of the next event to read.
var _next_event := 0


func _init() -> void:
	heard.resize(STREAMS.size())
	played.resize(STREAMS.size())
	_started.resize(STREAMS.size())
	_until.resize(VOICES)
	_until.fill(-1.0)
	for i in VOICES:
		var voice := AudioStreamPlayer2D.new()
		voice.bus = &"Effects"
		voice.max_distance = VOICE_DISTANCE
		_voices.append(voice)
		add_child(voice)
	_ui = AudioStreamPlayer.new()
	_ui.bus = &"Effects"
	add_child(_ui)
	_music = AudioStreamPlayer.new()
	_music.bus = &"Music"
	_music.stream = EXPLORE
	add_child(_music)


func _ready() -> void:
	_music.play()


# Stops every player and waits out the audio server's next mix, which releases what they played:
# a sound it still holds when the engine shuts down is reported leaked. The players, leaving the
# tree first, have paused their sounds, and a mix finishing a pause overwrites a stop made during
# it, so they are stopped under the lock the driver mixes in.
func _exit_tree() -> void:
	AudioServer.lock()
	for voice in _voices:
		voice.stop()
	_ui.stop()
	_music.stop()
	AudioServer.unlock()
	await_mix()


## Returns once the audio server has finished a mix started after the call, or after MIX_WAIT_USEC
## when none starts.
static func await_mix() -> void:
	var called := Time.get_ticks_usec()
	while _last_mix_start() <= called and Time.get_ticks_usec() - called <= MIX_WAIT_USEC:
		OS.delay_usec(1000)
	# The driver holds the lock while it mixes.
	AudioServer.lock()
	AudioServer.unlock()


# Returns when the audio server's last mix started, in Time.get_ticks_usec() microseconds; the clock
# is read first, so a mix starting meanwhile reads as no later than it began.
static func _last_mix_start() -> int:
	var now := Time.get_ticks_usec()
	return now - int(AudioServer.get_time_since_last_mix() * 1e6)


## Starts listening to `shown`: what it holds now is read as it stands, and only what changes
## from here on is heard.
func show_world(shown: WorldAdapter) -> void:
	_world = shown
	_tick = 0
	_stages.clear()
	_seen.fill(-2)
	_next_event = 0
	_read(false)


## Hears what the tick just run changed.
func note_tick() -> void:
	_tick += 1
	_started.fill(0)
	_read(true)


## Hears an event of kind `kind`, one of Heard, at `at` in pixels: starts a voice for it when it
## lies within `hearing`, no other of its kind started this tick and a voice is free. Returns
## whether a voice started.
func hear(kind: Heard, at: Vector2) -> bool:
	heard[kind] += 1
	if not hearing.has_point(at):
		return false
	if kind == Heard.STRIKE:
		_battle_until = _now + BATTLE_HOLD
	if _started[kind] > 0:
		return false
	var voice := -1
	for i in VOICES:
		if _until[i] <= _now:
			voice = i
			break
	if voice < 0:
		dropped += 1
		return false
	_started[kind] += 1
	played[kind] += 1
	var player := _voices[voice]
	player.stream = STREAMS[kind]
	player.position = at
	_until[voice] = _now + STREAMS[kind].get_length()
	if player.is_inside_tree():
		player.play()
	return true


## Returns how many voices are sounding.
func sounding() -> int:
	var count := 0
	for until in _until:
		if until > _now:
			count += 1
	return count


## Returns the stream voice `voice` sounds or last sounded.
func voice_stream(voice: int) -> AudioStream:
	return _voices[voice].stream


## Plays the UI's sound `name`, "click" or "open".
func ui(name: String) -> void:
	_ui.stream = UI_STREAMS[name]
	if _ui.is_inside_tree():
		_ui.play()


func _process(delta: float) -> void:
	advance(delta)


## Moves the sound's clock on by `delta` seconds, freeing voices whose sound has ended, and
## switches the music when the battle starts or ends.
func advance(delta: float) -> void:
	_now += delta
	var track := BATTLE if _now < _battle_until else EXPLORE
	if _music.stream != track:
		_music.stream = track
		if _music.is_inside_tree():
			_music.play()


# Reads the events, bodies and structures, hearing what happened since the last read when
# `listen` is set.
func _read(listen: bool) -> void:
	for event: Dictionary in _world.events(_next_event):
		_next_event = event.sequence + 1
		var at := (Vector2(event.cell) + Vector2.ONE / 2.0) * PIXELS_PER_CELL
		if listen and event.kind == WorldAdapter.EVENT_DIED:
			hear(Heard.FALL, at)
		elif listen and event.kind == WorldAdapter.EVENT_RAIDED:
			hear(Heard.BELL, at)
	var cell := PIXELS_PER_CELL / WorldAdapter.CELL_SIZE
	for army in _world.army_count():
		var roster := _world.roster(army)
		var healths := _world.body_strengths(army)
		var ready := _world.body_ready(army)
		var positions := _world.body_positions(army)
		for body in roster.size():
			var person := roster[body]
			if person >= _seen.size():
				_grow(person + 1)
			var at := positions[body] * cell
			if listen and _seen[person] == _tick - 1:
				if ready[body] != _ready_at[person]:
					hear(Heard.STRIKE, at)
				if healths[body] < _health[person]:
					hear(Heard.HIT, at)
			_seen[person] = _tick
			_health[person] = healths[body]
			_ready_at[person] = ready[body]
	for structure in _world.structure_count():
		var stage := _world.structure_stage(structure)
		if structure == _stages.size():
			_stages.append(stage)
		elif stage != _stages[structure]:
			var at := (Vector2(_world.structure_cell(structure)) \
					+ Vector2(_world.structure_footprint(structure)) / 2.0) * PIXELS_PER_CELL
			if listen:
				hear(Heard.FIRE if stage > _stages[structure] else Heard.MEND, at)
			_stages[structure] = stage


func _grow(size: int) -> void:
	var from := _seen.size()
	var to := maxi(size, from * 2)
	_seen.resize(to)
	_health.resize(to)
	_ready_at.resize(to)
	for person in range(from, to):
		_seen[person] = -2
