## Synthesises every sound under sound/ from the parameters below and writes each as a 16-bit mono
## WAV beside this script: the effects the sound scene plays on what the core reports, and the
## explore and battle music. A seeded generator draws every random value, so a run writes the same
## files. Run: godot --headless --path . --script res://sound/generate.gd
extends SceneTree

const RATE := 22050
const PEAK := 0.8
const DIR := "res://sound/"
# Effects: each a list of layers mixed together. A layer is a `noise` burst through a low-pass
# swept from `from` to `to` Hz, a `tone` gliding from `from` to `to` Hz, or a `bell` of inharmonic
# partials on `from` Hz; it starts at `at` seconds, rises over `attack` and decays over `decay`.
const EFFECTS := {
	"strike": {"seed": 11, "length": 0.22, "layers": [
		{"kind": "noise", "at": 0.0, "attack": 0.06, "decay": 0.14, "from": 900.0, "to": 5200.0, "gain": 1.0}]},
	"hit": {"seed": 12, "length": 0.2, "layers": [
		{"kind": "tone", "at": 0.0, "attack": 0.002, "decay": 0.16, "from": 150.0, "to": 55.0, "gain": 1.0},
		{"kind": "noise", "at": 0.0, "attack": 0.001, "decay": 0.04, "from": 3000.0, "to": 800.0, "gain": 0.6}]},
	"fall": {"seed": 13, "length": 0.5, "layers": [
		{"kind": "noise", "at": 0.0, "attack": 0.01, "decay": 0.12, "from": 700.0, "to": 200.0, "gain": 0.8},
		{"kind": "tone", "at": 0.0, "attack": 0.005, "decay": 0.2, "from": 90.0, "to": 45.0, "gain": 1.0},
		{"kind": "noise", "at": 0.16, "attack": 0.01, "decay": 0.1, "from": 500.0, "to": 150.0, "gain": 0.5},
		{"kind": "tone", "at": 0.16, "attack": 0.005, "decay": 0.16, "from": 70.0, "to": 40.0, "gain": 0.6}]},
	"fire": {"seed": 14, "length": 0.9, "layers": [
		{"kind": "noise", "at": 0.0, "attack": 0.25, "decay": 0.6, "from": 400.0, "to": 1600.0, "gain": 0.7},
		{"kind": "crackle", "at": 0.05, "attack": 0.0, "decay": 0.8, "from": 6000.0, "to": 2500.0, "gain": 0.9}]},
	"mend": {"seed": 15, "length": 0.45, "layers": [
		{"kind": "tone", "at": 0.0, "attack": 0.001, "decay": 0.07, "from": 620.0, "to": 600.0, "gain": 0.9},
		{"kind": "noise", "at": 0.0, "attack": 0.001, "decay": 0.02, "from": 4000.0, "to": 2000.0, "gain": 0.6},
		{"kind": "tone", "at": 0.2, "attack": 0.001, "decay": 0.07, "from": 660.0, "to": 640.0, "gain": 0.8},
		{"kind": "noise", "at": 0.2, "attack": 0.001, "decay": 0.02, "from": 4000.0, "to": 2000.0, "gain": 0.5}]},
	"bell": {"seed": 16, "length": 2.6, "layers": [
		{"kind": "bell", "at": 0.0, "attack": 0.003, "decay": 1.6, "from": 392.0, "to": 392.0, "gain": 1.0},
		{"kind": "bell", "at": 0.8, "attack": 0.003, "decay": 1.6, "from": 392.0, "to": 392.0, "gain": 0.8}]},
	"click": {"seed": 17, "length": 0.04, "layers": [
		{"kind": "tone", "at": 0.0, "attack": 0.001, "decay": 0.025, "from": 1800.0, "to": 1500.0, "gain": 0.6}]},
	"open": {"seed": 18, "length": 0.35, "layers": [
		{"kind": "noise", "at": 0.0, "attack": 0.05, "decay": 0.12, "from": 1500.0, "to": 4000.0, "gain": 0.4},
		{"kind": "pluck", "at": 0.04, "attack": 0.0, "decay": 0.3, "from": 880.0, "to": 880.0, "gain": 0.7}]},
}
# A bell's partials, as ratios of its fundamental, and their gains.
const BELL_PARTIALS := [[0.5, 0.5], [1.0, 1.0], [1.19, 0.6], [1.56, 0.4], [2.0, 0.35], [2.66, 0.25]]
# Music: a plucked melody drawn on a pentatonic scale, a drone, drums and a gong, looping
# seamlessly over `bars` bars of four beats. Scale degrees are semitones above `root` Hz.
const MUSIC := {
	"explore": {"seed": 21, "tempo": 72, "bars": 8, "root": 196.0, "scale": [0, 2, 4, 7, 9],
		"notes_per_beat": 1, "rest": 0.3, "drone": 0.25, "drums": false, "gong_every": 4},
	"battle": {"seed": 22, "tempo": 132, "bars": 16, "root": 164.81, "scale": [0, 3, 5, 7, 10],
		"notes_per_beat": 2, "rest": 0.2, "drone": 0.15, "drums": true, "gong_every": 8},
}

var _random := RandomNumberGenerator.new()


func _init() -> void:
	for name: String in EFFECTS:
		_write(name, _effect(EFFECTS[name]), false)
	for name: String in MUSIC:
		_write(name, _music(MUSIC[name]), true)
	quit()


func _effect(spec: Dictionary) -> PackedFloat32Array:
	_random.seed = spec.seed
	var samples := PackedFloat32Array()
	samples.resize(int(spec.length * RATE))
	for layer: Dictionary in spec.layers:
		_layer(samples, layer, false)
	return samples


# Mixes `layer` into `samples`, wrapping past the end when `wrap` is set so a loop stays seamless.
func _layer(samples: PackedFloat32Array, layer: Dictionary, wrap: bool) -> void:
	var start := int(layer.at * RATE)
	var length := int((layer.attack + layer.decay) * RATE)
	if layer.kind == "pluck":
		_pluck(samples, start, layer.from, layer.decay, layer.gain, wrap)
		return
	var phase := 0.0
	var low := 0.0
	for i in length:
		var t := float(i) / RATE
		var progress := float(i) / length
		var envelope: float = t / layer.attack if t < layer.attack else exp(-4.0 * (t - layer.attack) / layer.decay)
		var frequency: float = lerpf(layer.from, layer.to, progress)
		var value := 0.0
		match layer.kind:
			"noise":
				# A one-pole low-pass over white noise.
				var alpha := 1.0 - exp(-TAU * frequency / RATE)
				low += alpha * (_random.randf_range(-1.0, 1.0) - low)
				value = low * 2.0
			"crackle":
				value = _random.randf_range(-1.0, 1.0) if _random.randf() < 0.004 else 0.0
				var alpha := 1.0 - exp(-TAU * frequency / RATE)
				low += alpha * (value - low)
				value = low * 3.0
			"tone":
				phase += TAU * frequency / RATE
				value = sin(phase)
			"bell":
				for partial: Array in BELL_PARTIALS:
					value += sin(TAU * frequency * partial[0] * t) * partial[1] * exp(-t * partial[0] / layer.decay)
				value *= 0.4
		var at := start + i
		if wrap:
			at %= samples.size()
		elif at >= samples.size():
			break
		samples[at] += value * envelope * layer.gain


# A plucked string by Karplus-Strong: a burst of noise circulating through an averaging delay line.
func _pluck(samples: PackedFloat32Array, start: int, frequency: float, decay: float, gain: float,
		wrap: bool) -> void:
	var period := maxi(int(RATE / frequency), 2)
	var line := PackedFloat32Array()
	line.resize(period)
	for i in period:
		line[i] = _random.randf_range(-1.0, 1.0)
	var length := int(decay * 4.0 * RATE)
	for i in length:
		var k := i % period
		var value := line[k]
		line[k] = 0.5 * (value + line[(k + 1) % period]) * 0.996
		var at := start + i
		if wrap:
			at %= samples.size()
		elif at >= samples.size():
			break
		samples[at] += value * gain


func _music(spec: Dictionary) -> PackedFloat32Array:
	_random.seed = spec.seed
	var beat: float = 60.0 / spec.tempo
	var beats: int = spec.bars * 4
	var samples := PackedFloat32Array()
	samples.resize(int(beats * beat * RATE))
	var scale: Array = spec.scale
	# The melody walks the scale over two octaves, a step or two at a time, resting now and then;
	# each four bars end on the root.
	var degree := scale.size()
	var step: float = beat / spec.notes_per_beat
	for n in beats * spec.notes_per_beat:
		var bar_end: bool = (n + 1) % (16 * spec.notes_per_beat) == 0
		if not bar_end and _random.randf() < spec.rest:
			continue
		degree = clampi(degree + _random.randi_range(-2, 2), 0, scale.size() * 2 - 1)
		if bar_end:
			degree = scale.size()
		var semitones: int = scale[degree % scale.size()] + 12 * (degree / scale.size())
		var frequency: float = spec.root * pow(2.0, semitones / 12.0)
		_pluck(samples, int(n * step * RATE), frequency, 0.5, 0.5, true)
	if spec.drone > 0.0:
		for i in samples.size():
			var t := float(i) / RATE
			var swell := 0.6 + 0.4 * sin(TAU * t / (beat * 8.0))
			samples[i] += (sin(TAU * spec.root * 0.5 * t) + 0.5 * sin(TAU * spec.root * 0.75 * t)) \
					* spec.drone * swell
	for b in beats:
		if b % (spec.gong_every * 4) == 0:
			_layer(samples, {"kind": "bell", "at": b * beat, "attack": 0.01, "decay": 3.0,
				"from": spec.root * 0.5, "to": spec.root * 0.5, "gain": 0.5}, true)
		if spec.drums:
			# A low drum on each beat, louder on the first of the bar, and a high one off the beat.
			_layer(samples, {"kind": "tone", "at": b * beat, "attack": 0.003, "decay": 0.25,
				"from": 120.0, "to": 50.0, "gain": 0.9 if b % 4 == 0 else 0.6}, true)
			_layer(samples, {"kind": "noise", "at": (b + 0.5) * beat, "attack": 0.002, "decay": 0.05,
				"from": 2500.0, "to": 900.0, "gain": 0.35}, true)
	return samples


# Normalises `samples` to PEAK and writes them as DIR/<name>.wav, looping when `loop` is set.
func _write(name: String, samples: PackedFloat32Array, loop: bool) -> void:
	var peak := 0.0
	for value in samples:
		peak = maxf(peak, absf(value))
	var data := PackedByteArray()
	data.resize(samples.size() * 2)
	for i in samples.size():
		data.encode_s16(i * 2, int(clampf(samples[i] * PEAK / peak, -1.0, 1.0) * 32767.0))
	var wav := AudioStreamWAV.new()
	wav.format = AudioStreamWAV.FORMAT_16_BITS
	wav.mix_rate = RATE
	wav.stereo = false
	wav.data = data
	if loop:
		wav.loop_mode = AudioStreamWAV.LOOP_FORWARD
		wav.loop_end = samples.size()
	var error := wav.save_to_wav(ProjectSettings.globalize_path(DIR + name + ".wav"))
	print("%s.wav: %.2f s, %s" % [name, float(samples.size()) / RATE, error_string(error)])
