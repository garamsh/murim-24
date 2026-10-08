## The heads-up display over the play scene, in the tone of docs/art-direction.md §Effects and
## HUD: the player's portrait, faction, health, and its qi against its store and its realm as
## art/mockup/mobile_hud_arts.png shows them; the
## genealogy panel, which G opens; the sect panel, which M opens; the arts panel, which K opens,
## and the loadout it fills; the title, new-game, pause and
## settings screens and the rebirth screen; its faction's leader, a duel for the lead while one is
## fought, and the verdict on a challenge, why a cast was refused, whether an autosave was
## written, or, from the core's progress, a skill the player learned and how, or a realm it gained;
## a map of the region
## around the player with the season and year; the latest RUMOUR_LINES rumours that reach the
## player; and the technique bar, the loadout's five slots, each with its key, Q W E R T, and the
## icon of the skill on it. On a touch screen, or where the settings force it, the touch controls
## stand in for the technique bar and the keys, their arc holding the loadout, and the panels move
## as art/mockup/mobile_hud_idle.png lays them out.
## On a PC it is drawn at its own whole-pixel scale, finer than the world's, as ViewScale sets
## it, its panels anchored to the corners of the room it is given and a settings button under the
## map; with the touch controls it keeps the native 480x270 at the world's scale.
## Every word comes from hud.csv through Godot's translations, Korean first, or in the language
## the settings keep. It reads the world and holds no game rule.
extends CanvasLayer

## Sent when the settings button is clicked.
signal settings_asked

const LOCALES := ["ko", "en"]
const FONT := "res://hud/fonts/Galmuri9.ttf"
const FONT_SIZE := 10
const FACTIONS := ["orthodox", "demonic", "unorthodox"]
const ART := "res://art/"
const PersonName := preload("res://hud/person_name.gd")
const Arts := preload("res://hud/arts.gd")
const Loadout := preload("res://hud/loadout.gd")
const ViewScale := preload("res://view_scale.gd")
## The room the panels are placed in below, the native size; a larger room moves each panel by the
## room it gains, as far as its corner of the room.
const NATIVE := Vector2(480, 270)
## How many of the latest rumours the rumour log shows.
const RUMOUR_LINES := 3
# docs/art-direction.md §Palette.
const INK := Color("2a2433")
const INK_DARK := Color("14101a")
const GOLD := Color("8c6a2a")
const GOLD_LIGHT := Color("c99a3c")
const PAPER := Color("efe3c2")
const STONE := Color("8a94a0")
const JADE := Color("4fc49a")
const HP := Color("d63a2f")
const QI := Color("2f63b8")
const ACCENTS: Array[Color] = [Color("2f63b8"), Color("d63a2f"), Color("e8a830")]
# Minimap colours by terrain kind name.
const KIND_COLOURS := {
	"plain": Color("3b6138"), "forest": Color("1e3328"), "hills": Color("6b9447"),
	"water": Color("2b5770"), "mountain": Color("8a94a0"),
}
const MAP_SIZE := Vector2i(76, 44)
## A technique slot: its step along the bar, its size, the column its key is centred in, left of
## its icon, and where the icon stands.
const SLOT_STEP := 32
const SLOT_SIZE := Vector2(30, 20)
const SLOT_KEY := Rect2(1, 3, 10, 12)
const SLOT_ICON_AT := Vector2(12, 2)
# How long a notice about a challenge stays on screen.
const NOTICE_MS := 3000
# Where the panels the touch layout moves stand, in the keyboard layout and in the touch layout,
# as art/mockup/touch.lua places them.
const MAP_AT := {false: Vector2(392, 4), true: Vector2(346, 4)}
const RUMOUR_RECT := {false: Rect2(4, 184, 220, 52), true: Rect2(176, 4, 166, 42)}
const RUMOUR_WIDTH := {false: 209, true: 156}
const DUEL_AT := {false: Vector2(176, 4), true: Vector2(176, 50)}
const NOTICE_AT := {false: Vector2(90, 92), true: Vector2(140, 100)}
# The settings button, under the map; the touch layout has none, its pause button standing in.
const SETTINGS_RECT := Rect2(424, 70, 52, 16)
# The translation key of each challenge verdict, by WorldAdapter.CHALLENGE_* constant.
const CHALLENGE_KEYS := {
	WorldAdapter.CHALLENGE_NONE: "CHALLENGE_NONE",
	WorldAdapter.CHALLENGE_NO_SUCCESSION: "CHALLENGE_NO_SUCCESSION",
	WorldAdapter.CHALLENGE_NO_LEADER: "CHALLENGE_NO_LEADER",
	WorldAdapter.CHALLENGE_NO_CHALLENGER: "CHALLENGE_NO_CHALLENGER",
	WorldAdapter.CHALLENGE_NOT_A_MEMBER: "CHALLENGE_NOT_A_MEMBER",
	WorldAdapter.CHALLENGE_IS_LEADER: "CHALLENGE_IS_LEADER",
	WorldAdapter.CHALLENGE_DUEL_IN_PROGRESS: "CHALLENGE_DUEL_IN_PROGRESS",
}
# The translation key of each reason a cast is refused, by WorldAdapter.CAST_* constant.
const CAST_KEYS := {
	WorldAdapter.CAST_DEAD: "CAST_DEAD",
	WorldAdapter.CAST_NO_SKILL: "CAST_NO_SKILL",
	WorldAdapter.CAST_ON_COOLDOWN: "CAST_ON_COOLDOWN",
	WorldAdapter.CAST_NO_QI: "CAST_NO_QI",
	WorldAdapter.CAST_NO_TARGET: "CAST_NO_TARGET",
	WorldAdapter.CAST_OUT_OF_RANGE: "CAST_OUT_OF_RANGE",
}

## Labels that show a translated key or a value, by what they show, so a test can read them.
var labels := {}
## The genealogy panel, open over the HUD while the player reads it.
var genealogy: Control
## The sect panel, open over the HUD while the player reads it.
var sect: Control
## The arts panel, open over the HUD while the player reads it or fills the loadout.
var arts: Control
## The skills on the technique bar's slots and the touch arc's buttons.
var loadout := Loadout.new()
## The title screen, the new-game panel over it, the pause panel over the game, and the settings
## panel over either.
var title_screen: Control
var new_game: Control
var pause: Control
var settings: Control
var rebirth: Control
## The panel shown while a duel for the player's faction's lead is fought, and its two bars:
## `challenger` and `leader`.
var duel_panel: Panel
var duel_bars := {}
## The icon in each technique slot, in key order.
var slot_icons: Array[TextureRect] = []
## The skill on each technique slot, as WorldAdapter.person_skills gives it, or an empty
## Dictionary, so a test can read them.
var slot_skills: Array[Dictionary] = []
## The touch controls, shown in place of the technique bar while the touch layout is on.
var touch: Control
## How many of the rumour log's lines the layout shows.
var rumour_lines := RUMOUR_LINES
## The room the HUD is laid out in, in its own pixels.
var room := NATIVE
## The settings button, shown in the keyboard layout.
var settings_button: Panel

var _theme := Theme.new()
var _portrait := TextureRect.new()
## The health and inner-energy bars, so a test can read them.
var hp_bar := ProgressBar.new()
var qi_bar := ProgressBar.new()
var _map := TextureRect.new()
## The map of the whole region, one pixel per cell, so a test can read it.
var map_texture: ImageTexture
var _dot := ColorRect.new()
var _slots: Array[Panel] = []
var _icons := {}
# The duel being shown: its challenger and each duellist's health when it began.
var _duel := {}
var _notice_until := 0
var _skill_bar: Panel
var _world_size := Vector2i.ZERO
var _map_panel: Panel
var _rumour_panel: Panel
# The steered body's ready tick when it last moved, and the ticks it then had to wait.
var _ready_at := -1
var _cool_span := 1
# The world whose events the notices follow, the next of its events to read, and the notices of
# skills learned and realms gained waiting to be shown.
var _progress_of: WorldAdapter
var _next_step := 0
var _waiting: Array[String] = []


func _ready() -> void:
	_set_up()


# Builds the HUD once, on ready or on the first show_player, whichever comes first: a test
# readies only the scene it instantiates.
func _set_up() -> void:
	if not _slots.is_empty():
		return
	TranslationServer.set_locale(LOCALES[0])
	var font: FontFile = load(FONT)
	font.antialiasing = TextServer.FONT_ANTIALIASING_NONE
	font.hinting = TextServer.HINTING_NONE
	font.subpixel_positioning = TextServer.SUBPIXEL_POSITIONING_DISABLED
	_theme.default_font = font
	_theme.default_font_size = FONT_SIZE
	_theme.set_color("font_color", "Label", PAPER)
	_build()


## Builds the HUD if it is not yet built, and opens the title screen, with continue taken or not
## as `saved` says whether there is a save to continue.
func open_title(saved: bool) -> void:
	_set_up()
	title_screen.open(saved)


## Returns the screen open on top, the one that takes the keys: the rebirth screen, the settings,
## the new-game panel, the pause panel or the title, or null while none is open.
func screen() -> Control:
	for open: Control in [rebirth, settings, new_game, pause, title_screen]:
		if open.visible:
			return open
	return null


## Shows `world`'s region on the map and army `steered`'s faction on the portrait.
func show_player(world: WorldAdapter, steered: int) -> void:
	_set_up()
	var icons: Dictionary = JSON.parse_string(FileAccess.get_file_as_string(ART + "icons/skill_icons.json"))
	_icons = {"texture": load(ART + "icons/skill_icons.png"), "tags": {}}
	for tag: Dictionary in icons.meta.frameTags:
		_icons.tags[tag.name] = int(tag.from)
	var names := world.faction_names()
	var faction := world.army_faction(steered)
	var sprite: String = names[faction].to_lower() if faction < names.size() else FACTIONS[faction % FACTIONS.size()]
	labels.faction.text = "FACTION_" + sprite.to_upper()
	var portrait := AtlasTexture.new()
	portrait.atlas = load(ART + "bodies/" + sprite + ".png")
	# The idle frame's head and shoulders.
	portrait.region = Rect2(0, 6, 16, 16)
	_portrait.texture = portrait
	show_map(world)
	if world != _progress_of:
		_progress_of = world
		var kept := world.progress(0)
		_next_step = int(kept.back().sequence) + 1 if not kept.is_empty() else 0
		_waiting.clear()
	var roster := world.roster(steered)
	if not roster.is_empty():
		loadout.follow(world, roster[0])
	show_loadout(world)


## Shows the loadout's skills on the technique bar's slots and the touch arc's buttons, as
## `world` gives the person the loadout follows: each skill's icon, and on the arc its range and
## radius; an empty slot shows none.
func show_loadout(world: WorldAdapter) -> void:
	_set_up()
	var skills := world.person_skills(loadout.person) if loadout.person >= 0 else []
	var reach: Array[Dictionary] = []
	var pixels := 16.0 / WorldAdapter.CELL_SIZE
	slot_skills.clear()
	for slot in Loadout.SLOTS:
		var index := loadout.skill(world, slot)
		var skill: Dictionary = skills[index] if index >= 0 else {}
		slot_skills.append(skill)
		if skill.is_empty():
			slot_icons[slot].texture = null
			reach.append({})
			continue
		# A skill whose radius reaches past its target bursts; any other strikes one body.
		var tag: String = "burst" if int(skill.radius) > 0 else "strike"
		var icon := AtlasTexture.new()
		icon.atlas = _icons.texture
		icon.region = Rect2(_icons.tags[tag] * 16, 0, 16, 16)
		slot_icons[slot].texture = icon
		reach.append({"icon": tag, "range": int(skill.range) * pixels, "radius": int(skill.radius) * pixels})
	touch.show_skills(reach)


## Draws `world`'s region on the map as it stands: its terrain, the territory the core derives
## now and its villages.
func show_map(world: WorldAdapter) -> void:
	map_texture = ImageTexture.create_from_image(_map_image(world))
	(_map.texture as AtlasTexture).atlas = map_texture


## Updates the bars, the map around the player, the technique bar and the arts panel while it is
## open, and announces the skills the player learned and the realms it gained since the last
## refresh: `skills` are the skill indices the player's last order chose.
func refresh(world: WorldAdapter, steered: int, skills: PackedInt64Array) -> void:
	var strength := 0
	for body_strength in world.body_strengths(steered):
		strength += body_strength
	var full := maxi(world.army_strength(steered), 1)
	hp_bar.value = 100.0 * strength / full
	# A percentage of the starting strength: strength itself runs to units no player reads.
	labels.hp_value.text = "%d%%" % floori(hp_bar.value)
	var positions := world.body_positions(steered)
	_dot.visible = not positions.is_empty()
	if not positions.is_empty():
		var lead := positions[0] / WorldAdapter.CELL_SIZE
		var corner := (Vector2i(lead) - MAP_SIZE / 2).clamp(Vector2i.ZERO,
				(_world_size - MAP_SIZE).max(Vector2i.ZERO))
		(_map.texture as AtlasTexture).region = Rect2(corner, MAP_SIZE)
		_dot.position = _map.position + (lead - Vector2(corner)).floor() - Vector2.ONE
	_refresh_leadership(world, steered)
	var roster := world.roster(steered)
	if not roster.is_empty():
		var person := roster[0]
		var capacity := world.person_qi_capacity(person)
		qi_bar.value = 100.0 * world.person_qi(person) / maxi(capacity, 1)
		labels.qi_value.text = "%s / %s" % [Arts.grouped(world.person_qi(person)), Arts.grouped(capacity)]
		labels.realm_value.text = Arts.realm_name(world, world.person_realm(person))
		show_rumours(world, person)
		_announce_lessons(world, person)
	labels.season.text = season_text(world.date())
	for slot in _slots.size():
		var style: StyleBoxFlat = _slots[slot].get_theme_stylebox("panel")
		style.border_color = JADE if skills.has(loadout.skill(world, slot)) else INK
	if arts.visible:
		arts.reread()
	if touch.visible:
		_refresh_cooling(world, steered)


# Shows on the touch controls how long the steered body has left before it may strike again, as
# the core's ready tick and tick count give it: the share of its wait still to run, and the whole
# seconds left.
func _refresh_cooling(world: WorldAdapter, steered: int) -> void:
	var ready := world.body_ready(steered)
	if ready.is_empty():
		touch.cooling = 0.0
		touch.queue_redraw()
		return
	# A body may cast again once its ready tick is no later than the next tick.
	var left := maxi(int(ready[0]) - world.tick_count() - 1, 0)
	if ready[0] != _ready_at:
		_ready_at = ready[0]
		_cool_span = maxi(left, 1)
	var cooling := float(left) / _cool_span
	var seconds := ceili(float(left) / WorldAdapter.TICKS_PER_SECOND)
	if cooling != touch.cooling or seconds != touch.cooling_seconds:
		touch.cooling = cooling
		touch.cooling_seconds = seconds
		touch.queue_redraw()


## Lays the HUD out for touch when `on`, as art/mockup/mobile_hud_idle.png lays it out: the touch
## controls shown in place of the technique bar, the rumour log of two lines beside the status
## panel, the map left of the pause button, and no word naming a key; and for the keyboard when
## not. A mouse then stands in for a finger where the screen takes no touch, so the touch layout
## can be tried at a desk.
func use_touch(on: bool) -> void:
	_set_up()
	touch.visible = on
	if not on:
		touch.release_all()
	Input.emulate_touch_from_mouse = on and not DisplayServer.is_touchscreen_available()
	_skill_bar.visible = not on
	settings_button.visible = not on
	labels.challenge_key.visible = not on
	_rumour_panel.size = RUMOUR_RECT[on].size
	rumour_lines = 2 if on else RUMOUR_LINES
	for line in RUMOUR_LINES:
		var label: Label = labels["rumour_%d" % line]
		label.size.x = RUMOUR_WIDTH[on]
		label.visible = line < rumour_lines
	_place()


## Sizes the HUD for a window of `window` pixels, with the touch controls shown or not as `touch`
## says, as ViewScale scales it and gives it room.
func fit(window: Vector2i, touch: bool) -> void:
	_set_up()
	var scale := ViewScale.hud_scale(window, touch)
	var placed := ViewScale.hud_room(window, touch)
	transform = Transform2D(0.0, Vector2(scale, scale), 0.0, Vector2(placed.position))
	room = Vector2(placed.size)
	_place()


## Returns `event` with its position in the HUD's pixels, from the window's.
func to_hud(event: InputEvent) -> InputEvent:
	return event.xformed_by(transform.affine_inverse())


## Returns where `at`, a point in the HUD's pixels, lies in the window's.
func to_window(at: Vector2) -> Vector2:
	return transform * at


## Returns where `at`, a point in the window's pixels, lies in the HUD's.
func from_window(at: Vector2) -> Vector2:
	return transform.affine_inverse() * at


# Asks for the settings when the settings button is clicked.
func _on_settings_button(event: InputEvent) -> void:
	var click := event as InputEventMouseButton
	if click != null and click.pressed and click.button_index == MOUSE_BUTTON_LEFT:
		settings_asked.emit()


# Places the panels of the layout shown in the room: each keeps its place against the corner it
# stands by, the panels and screens over the HUD in the room's middle.
func _place() -> void:
	var on := touch.visible
	var gained := room - NATIVE
	_map_panel.position = MAP_AT[on] + Vector2(gained.x, 0)
	settings_button.position = SETTINGS_RECT.position + Vector2(gained.x, 0)
	_rumour_panel.position = RUMOUR_RECT[on].position + Vector2(0, gained.y)
	_skill_bar.position = Vector2(240 - _skill_bar.size.x / 2.0, 240) + Vector2(gained.x / 2.0, gained.y)
	duel_panel.position = DUEL_AT[on] + Vector2(gained.x / 2.0, 0)
	labels.notice.position = NOTICE_AT[on] + gained / 2.0
	for over: Control in [arts, touch, genealogy, sect, title_screen, new_game, pause, settings, rebirth]:
		over.position = (gained / 2.0).floor()


# Queues a notice for each skill `person` learned and each realm it gained in `world`'s progress
# since the last read: how it learned the skill, and its name, or the realm's name.
func _announce_lessons(world: WorldAdapter, person: int) -> void:
	var lessons := {
		WorldAdapter.LEARNED_BY_COMPREHENSION: "NOTICE_COMPREHENDED",
		WorldAdapter.LEARNED_BY_MANUAL: "NOTICE_READ",
		WorldAdapter.LEARNED_BY_TEACHING: "NOTICE_TAUGHT",
	}
	for step: Dictionary in world.progress(_next_step):
		_next_step = int(step.sequence) + 1
		if step.person != person:
			continue
		if step.kind == WorldAdapter.PROGRESS_LEARNED:
			var taught: Dictionary = world.arts()[step.art].skills[step.skill]
			_waiting.append(tr(lessons[int(step.by)]) % Arts.named(taught))
		else:
			_waiting.append(tr("NOTICE_REALM") % Arts.realm_name(world, step.rank))


## Returns the notices of skills learned and realms gained still waiting to be shown.
func waiting_notices() -> Array[String]:
	return _waiting.duplicate()


## Shows the latest RUMOUR_LINES rumours that reach `person`, newest first, or that none has come.
func show_rumours(world: WorldAdapter, person: int) -> void:
	var heard := world.rumours(person)
	for line in RUMOUR_LINES:
		var label: Label = labels["rumour_%d" % line]
		label.text = rumour_text(world, heard[line]) if line < heard.size() else ""
	if heard.is_empty():
		labels.rumour_0.text = tr("HUD_RUMOR_NONE")


## Returns `event`, a rumour as WorldAdapter.rumours gives it, worded in the current locale.
func rumour_text(world: WorldAdapter, event: Dictionary) -> String:
	var faction := func(index: int) -> String:
		return tr("FACTION_" + world.faction_names()[index].to_upper())
	var person := func(index: int) -> String:
		return PersonName.of(world, index, true)
	match event.kind:
		"founded":
			return tr("RUMOUR_FOUNDED") % faction.call(event.faction)
		"raided":
			return tr("RUMOUR_RAIDED") % [faction.call(event.raider), faction.call(event.owner)]
		"ruined":
			return tr("RUMOUR_RUINED") % faction.call(event.owner)
		"sect_fell":
			return tr("RUMOUR_SECT_FELL") % tr("SECT_" + world.sect_name(event.sect).to_upper().replace(" ", "_"))
		"leader_changed":
			var key := "RUMOUR_DUEL" if event.by == "duel" else "RUMOUR_ELECTED"
			return tr(key) % [person.call(event.leader), faction.call(event.faction)]
		"born":
			return tr("RUMOUR_BORN") % person.call(event.person)
		_:
			return tr("RUMOUR_DIED") % person.call(event.person)


## Shows the core's verdict on the player's challenge, one of the WorldAdapter.CHALLENGE_*
## constants, for NOTICE_MS: that the duel began, or why it was refused.
func show_challenge(refusal: int) -> void:
	_set_up()
	_notice(CHALLENGE_KEYS.get(refusal, "CHALLENGE_NONE"))


## Shows for NOTICE_MS why the core refused the player's cast, one of the WorldAdapter.CAST_*
## constants other than CAST_NONE.
func show_cast(refusal: int) -> void:
	_set_up()
	_notice(CAST_KEYS[refusal])


## Shows for NOTICE_MS whether the autosave was written, as `written` says.
func show_autosave(written: bool) -> void:
	_set_up()
	_notice("HUD_AUTOSAVED" if written else "HUD_AUTOSAVE_FAILED")


# Shows the player's faction's leader, and while a duel for its lead is fought, both duellists'
# health against what each had when it began; when it ends, whether the lead changed hands.
func _refresh_leadership(world: WorldAdapter, steered: int) -> void:
	var faction := world.army_faction(steered)
	var leader := world.leader(faction)
	var own := world.roster(steered)
	labels.leader_value.text = _who(world, leader, own) if leader >= 0 else tr("HUD_LEADER_NONE")
	var fought := world.duel(faction)
	duel_panel.visible = not fought.is_empty()
	if fought.is_empty():
		if not _duel.is_empty():
			_notice("HUD_DUEL_NEW_LEADER" if leader == _duel.challenger else "HUD_DUEL_LEADER_HELD")
			_duel = {}
		return
	if _duel.is_empty() or _duel.challenger != fought.challenger:
		_duel = {
			"challenger": fought.challenger,
			"from": maxi(world.person_health(fought.challenger), 1),
			"leader_from": maxi(world.person_health(fought.leader), 1),
		}
	labels.duel_challenger.text = _who(world, fought.challenger, own)
	labels.duel_leader.text = _who(world, fought.leader, own)
	duel_bars.challenger.value = 100.0 * world.person_health(fought.challenger) / _duel.from
	duel_bars.leader.value = 100.0 * world.person_health(fought.leader) / _duel.leader_from


# The player, if `person` is the player's own, or the person by name: the given name alone in
# English, as on the genealogy cards.
func _who(world: WorldAdapter, person: int, own: PackedInt64Array) -> String:
	return tr("HUD_LEADER_YOU") if own.has(person) else PersonName.of(world, person, true)


func _notice(key: String) -> void:
	labels.notice.text = key
	labels.notice.visible = true
	_notice_until = Time.get_ticks_msec() + NOTICE_MS


func _process(_delta: float) -> void:
	if labels.has("notice") and labels.notice.visible and Time.get_ticks_msec() > _notice_until:
		labels.notice.visible = false
	if labels.has("notice") and not labels.notice.visible and not _waiting.is_empty():
		_notice(_waiting.pop_front())


## The season and year of `date`, as the core's calendar gives them, in the current locale.
func season_text(date: Dictionary) -> String:
	return tr("HUD_SEASON_YEAR") % [tr("SEASON_%d" % date.season), date.year]


func _build() -> void:
	var player := _panel(Rect2(4, 4, 168, 62))
	_portrait.position = Vector2(4, 8)
	_portrait.size = Vector2(32, 32)
	_portrait.stretch_mode = TextureRect.STRETCH_SCALE
	player.add_child(_portrait)
	# The faction follows the title, whichever language makes the title longer.
	var title := HBoxContainer.new()
	title.position = Vector2(42, 3)
	title.add_theme_constant_override("separation", 6)
	player.add_child(title)
	labels.player = _label(title, "HUD_PLAYER", Vector2.ZERO, PAPER)
	labels.faction = _label(title, "", Vector2.ZERO, STONE)
	# Each value on its label's line, right-aligned, and its bar under the line.
	labels.hp = _label(player, "HUD_HP", Vector2(42, 14), HP)
	labels.hp_value = _value(player, Vector2(42, 14), 120)
	labels.qi = _label(player, "HUD_QI", Vector2(42, 30), QI)
	labels.qi_value = _value(player, Vector2(42, 30), 120)
	labels.realm = _label(player, "HUD_REALM", Vector2(42, 47), GOLD_LIGHT)
	labels.realm_value = _value(player, Vector2(42, 47), 120)
	labels.realm_value.text = "—"
	_bar(player, hp_bar, Vector2(42, 26), HP)
	_bar(player, qi_bar, Vector2(42, 42), QI)
	qi_bar.value = 0.0
	labels.qi_value.text = "—"

	var map := _panel(Rect2(MAP_AT[false], Vector2(84, 62)))
	settings_button = _panel(SETTINGS_RECT)
	var word := _label(settings_button, "SETTINGS_TITLE", Vector2.ZERO, PAPER)
	word.size = SETTINGS_RECT.size - Vector2(0, 2)
	word.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	word.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
	settings_button.gui_input.connect(_on_settings_button)
	_map_panel = map
	_map.position = Vector2(4, 4)
	_map.size = Vector2(MAP_SIZE)
	_map.stretch_mode = TextureRect.STRETCH_SCALE
	_map.texture = AtlasTexture.new()
	map.add_child(_map)
	_dot.color = JADE
	_dot.size = Vector2(2, 2)
	map.add_child(_dot)
	labels.season = _trimmed(map, Vector2(5, 48), 74)

	# Above the technique bar's row, so the widest bar never meets it.
	var rumour := _panel(RUMOUR_RECT[false])
	_rumour_panel = rumour
	labels.rumour = _label(rumour, "HUD_RUMOR", Vector2(5, 2), GOLD_LIGHT)
	for line in RUMOUR_LINES:
		labels["rumour_%d" % line] = _trimmed(rumour, Vector2(5, 14 + line * 12), RUMOUR_WIDTH[false])
	labels.rumour_0.text = tr("HUD_RUMOR_NONE")

	var bar_width := Loadout.SLOTS * SLOT_STEP + 6
	_skill_bar = _panel(Rect2(240 - bar_width / 2.0, 240, bar_width, 26))
	var bar := _skill_bar
	for slot in Loadout.SLOTS:
		var cell := Panel.new()
		var style := StyleBoxFlat.new()
		style.bg_color = INK_DARK
		style.set_border_width_all(1)
		style.border_color = INK
		cell.add_theme_stylebox_override("panel", style)
		cell.position = Vector2(4 + slot * SLOT_STEP, 3)
		cell.size = SLOT_SIZE
		bar.add_child(cell)
		var icon := TextureRect.new()
		icon.position = SLOT_ICON_AT
		icon.size = Vector2(16, 16)
		cell.add_child(icon)
		slot_icons.append(icon)
		# The slot's key, in a column of its own left of the icon, so it covers none of it.
		var number := Label.new()
		number.theme = _theme
		number.text = Arts.SLOT_KEYS[slot]
		number.auto_translate_mode = Node.AUTO_TRANSLATE_MODE_DISABLED
		number.position = SLOT_KEY.position
		number.size = SLOT_KEY.size
		number.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
		number.add_theme_color_override("font_color", GOLD_LIGHT)
		number.add_theme_color_override("font_outline_color", INK_DARK)
		number.add_theme_constant_override("outline_size", 2)
		cell.add_child(number)
		_slots.append(cell)

	# The genealogy panel, closed until G opens it; added after every other panel, below, so it
	# draws over them all.
	genealogy = preload("res://hud/genealogy.gd").new()
	genealogy.use_font(_theme.default_font)
	genealogy.visible = false
	sect = preload("res://hud/sect.gd").new()
	sect.use_font(_theme.default_font)
	sect.visible = false
	arts = Arts.new()
	arts.use_font(_theme.default_font)
	arts.visible = false

	var leadership := _panel(Rect2(4, 70, 168, 18))
	labels.leader = _label(leadership, "HUD_LEADER", Vector2(5, 2), GOLD_LIGHT)
	labels.leader_value = _trimmed(leadership, Vector2(42, 2), 56)
	labels.challenge_key = _label(leadership, "HUD_CHALLENGE_KEY", Vector2(100, 2), STONE)

	duel_panel = _panel(Rect2(DUEL_AT[false], Vector2(128, 44)))
	duel_panel.visible = false
	labels.duel = _label(duel_panel, "HUD_DUEL", Vector2(5, 3), HP)
	labels.duel_challenger = _trimmed(duel_panel, Vector2(5, 15), 52)
	labels.duel_leader = _trimmed(duel_panel, Vector2(5, 28), 52)
	duel_bars.challenger = ProgressBar.new()
	duel_bars.leader = ProgressBar.new()
	_bar(duel_panel, duel_bars.challenger, Vector2(60, 20), HP)
	_bar(duel_panel, duel_bars.leader, Vector2(60, 33), HP)
	duel_bars.challenger.size.x = 62
	duel_bars.leader.size.x = 62

	labels.notice = _label(self, "", NOTICE_AT[false], PAPER)
	labels.notice.size = Vector2(300, 12)
	labels.notice.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	labels.notice.add_theme_color_override("font_outline_color", INK_DARK)
	labels.notice.add_theme_constant_override("outline_size", 3)
	labels.notice.visible = false
	# The touch controls, hidden until the touch layout is on, under the panels and screens but over
	# the arts panel, beside which their arc stays live.
	touch = preload("res://hud/touch.gd").new()
	touch.use_font(_theme.default_font)
	touch.visible = false
	add_child(arts)
	add_child(touch)
	add_child(genealogy)
	add_child(sect)
	# The screens, closed until the play scene opens one, over every panel and each over the last.
	title_screen = preload("res://hud/title.gd").new()
	new_game = preload("res://hud/new_game.gd").new()
	pause = preload("res://hud/pause.gd").new()
	settings = preload("res://hud/settings.gd").new()
	rebirth = preload("res://hud/rebirth.gd").new()
	settings.locales = LOCALES
	for screen_panel: Control in [title_screen, new_game, pause, settings, rebirth]:
		screen_panel.use_font(_theme.default_font)
		screen_panel.visible = false
		add_child(screen_panel)
	settings.applied.connect(func() -> void: use_touch(settings.touch_shown()))

func _panel(rect: Rect2) -> Panel:
	var panel := Panel.new()
	panel.theme = _theme
	panel.position = rect.position
	panel.size = rect.size
	var style := StyleBoxFlat.new()
	style.bg_color = INK
	style.set_border_width_all(1)
	style.border_color = GOLD
	style.border_width_top = 2
	panel.add_theme_stylebox_override("panel", style)
	add_child(panel)
	return panel


func _label(parent: Node, key: String, at: Vector2, colour: Color) -> Label:
	var label := Label.new()
	label.theme = _theme
	label.text = key
	label.position = at
	label.add_theme_color_override("font_color", colour)
	parent.add_child(label)
	return label


# A value label `width` pixels wide from `at`, its text against the right edge.
func _value(parent: Control, at: Vector2, width: float) -> Label:
	var label := Label.new()
	label.theme = _theme
	label.auto_translate_mode = Node.AUTO_TRANSLATE_MODE_DISABLED
	label.position = at
	label.size = Vector2(width, 12)
	label.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	label.add_theme_color_override("font_color", PAPER)
	parent.add_child(label)
	return label


# A label `width` pixels wide from `at`, its text from the left edge and cut short with an
# ellipsis past the right: a person's name, or a date in a year too late for the map panel.
func _trimmed(parent: Control, at: Vector2, width: float) -> Label:
	var label := _value(parent, at, width)
	label.horizontal_alignment = HORIZONTAL_ALIGNMENT_LEFT
	label.text_overrun_behavior = TextServer.OVERRUN_TRIM_ELLIPSIS
	return label


func _bar(parent: Control, bar: ProgressBar, at: Vector2, colour: Color) -> void:
	var fill := StyleBoxFlat.new()
	fill.bg_color = colour
	var back := StyleBoxFlat.new()
	back.bg_color = INK_DARK
	bar.add_theme_stylebox_override("fill", fill)
	bar.add_theme_stylebox_override("background", back)
	bar.show_percentage = false
	bar.position = at
	bar.size = Vector2(120, 4)
	parent.add_child(bar)


# One pixel per cell: the terrain kind's colour, the accent of the faction holding it along
# territory borders, or of its neighbour's where it lies in no territory, and each village's
# square in its owner's accent.
func _map_image(world: WorldAdapter) -> Image:
	var rows := world.map_rows()
	_world_size = Vector2i(rows[0].length() if rows.size() > 0 else 0, rows.size())
	var image := Image.create_empty(maxi(_world_size.x, 1), maxi(_world_size.y, 1), false, Image.FORMAT_RGBA8)
	var names := world.terrain_names()
	var terrain := world.terrain()
	var owners := world.territory()
	var sprites := []
	for name in world.faction_names():
		sprites.append(maxi(FACTIONS.find(name.to_lower()), 0))
	for y in _world_size.y:
		for x in _world_size.x:
			var i := y * _world_size.x + x
			var colour: Color
			if terrain.size() == _world_size.x * _world_size.y:
				colour = KIND_COLOURS.get(names[terrain[i]], KIND_COLOURS.plain)
			else:
				colour = KIND_COLOURS.plain if rows[y][x] == "." else KIND_COLOURS.mountain
			if owners.size() == terrain.size() and owners.size() > 0:
				var right := i + 1 if x + 1 < _world_size.x else i
				var below := i + _world_size.x if y + 1 < _world_size.y else i
				for next in [right, below]:
					if owners[next] != owners[i]:
						colour = ACCENTS[sprites[owners[i] if owners[i] >= 0 else owners[next]]]
			image.set_pixel(x, y, colour)
	for village: Dictionary in world.villages():
		var corner: Vector2i = village.corner
		var area: Vector2i = village.area
		image.fill_rect(Rect2i(corner, area), ACCENTS[sprites[village.faction]].lightened(0.3))
	return image
