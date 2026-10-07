## Plays what the core reports as sound. After each tick it compares what the adapter reads with
## what it read on the tick before: a body whose ready tick moved struck, a living body whose
## health fell was hit, a body that lived and now has none fell, a structure a stage worse caught
## fire, a structure a stage better was mended, and a village newly raided rings its bell. Each is
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
# By person: the tick it was last read alive, its health, its ready tick and where it stood.
var _seen := PackedInt64Array()
var _health := PackedInt64Array()
var _ready_at := PackedInt64Array()
var _where := PackedVector2Array()
# By army, its roster as last read; by structure, its stage; by village, its state and centre.
var _rosters: Array[PackedInt64Array] = []
var _stages := PackedInt64Array()
var _states := PackedInt64Array()
var _centres := PackedVector2Array()


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


## Starts listening to `shown`: what it holds now is read as it stands, and only what changes
## from here on is heard.
func show_world(shown: WorldAdapter) -> void:
	_world = shown
	_tick = 0
	_rosters.clear()
	_stages.clear()
	_states.clear()
	_seen.fill(-2)
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


## Returns the music playing: the battle track while strikes were heard in the last
## BATTLE_HOLD seconds, the explore track otherwise.
func music() -> AudioStream:
	return _music.stream


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


# Reads the bodies, structures and villages, hearing what changed since the last read when
# `listen` is set.
func _read(listen: bool) -> void:
	var cell := PIXELS_PER_CELL / WorldAdapter.CELL_SIZE
	for army in _world.army_count():
		if army == _rosters.size():
			_rosters.append(PackedInt64Array())
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
			_where[person] = at
		# Someone on the last roster not read since has left it: the dead have no health left,
		# while the living have joined another army.
		for person in _rosters[army]:
			if listen and _seen[person] == _tick - 1 and _world.person_health(person) == 0:
				hear(Heard.FALL, _where[person])
		_rosters[army] = roster
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
	var villages := _world.village_count()
	if villages != _centres.size():
		_centres.clear()
		for village: Dictionary in _world.villages():
			_centres.append((Vector2(village.corner) + Vector2(village.area) / 2.0) * PIXELS_PER_CELL)
	for village in mini(villages, _centres.size()):
		var state := _world.village_state(village)
		if village == _states.size():
			_states.append(state)
		elif state != _states[village]:
			if listen and state == WorldAdapter.VILLAGE_RAIDED:
				hear(Heard.BELL, _centres[village])
			_states[village] = state


func _grow(size: int) -> void:
	var from := _seen.size()
	var to := maxi(size, from * 2)
	_seen.resize(to)
	_health.resize(to)
	_ready_at.resize(to)
	_where.resize(to)
	for person in range(from, to):
		_seen[person] = -2
