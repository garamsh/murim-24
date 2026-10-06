## The heads-up display over the play scene, in the tone of docs/art-direction.md §Effects and
## HUD: the player's portrait, faction, health and inner energy with their values; its faction's
## leader, a duel for the lead while one is fought, and the verdict on a challenge; a map of the
## region around the player with the season and year; a rumour log; and the technique bar, an
## icon and its number key for each of the player's skills. Every word comes from
## hud.csv through Godot's translations, Korean first; F2 switches between Korean and English.
## It reads the world and holds no game rule.
extends CanvasLayer

const LOCALES := ["ko", "en"]
const FONT := "res://hud/fonts/Galmuri9.ttf"
const FONT_SIZE := 10
const FACTIONS := ["orthodox", "demonic", "unorthodox"]
const ART := "res://art/"
const MAX_SLOTS := 9
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
# How long a notice about a challenge stays on screen.
const NOTICE_MS := 3000
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

## Labels that show a translated key or a value, by what they show, so a test can read them.
var labels := {}
## The panel shown while a duel for the player's faction's lead is fought, and its two bars:
## `challenger` and `leader`.
var duel_panel: Panel
var duel_bars := {}
## The icon in each technique slot, in key order.
var slot_icons: Array[TextureRect] = []

var _theme := Theme.new()
var _portrait := TextureRect.new()
## The health and inner-energy bars, so a test can read them.
var hp_bar := ProgressBar.new()
var qi_bar := ProgressBar.new()
var _map := TextureRect.new()
var _map_texture: ImageTexture
var _dot := ColorRect.new()
var _slots: Array[Panel] = []
var _icons := {}
# The duel being shown: its challenger and each duellist's health when it began.
var _duel := {}
var _notice_until := 0
var _skill_bar: Panel
var _world_size := Vector2i.ZERO


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


func _unhandled_key_input(event: InputEvent) -> void:
	var key := event as InputEventKey
	if key != null and key.pressed and key.keycode == KEY_F2:
		switch_locale()


## Switches the HUD to the next locale in LOCALES, and returns it.
func switch_locale() -> String:
	var next: String = LOCALES[(LOCALES.find(TranslationServer.get_locale().left(2)) + 1) % LOCALES.size()]
	TranslationServer.set_locale(next)
	return next


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
	_map_texture = ImageTexture.create_from_image(_map_image(world))
	var view := AtlasTexture.new()
	view.atlas = _map_texture
	_map.texture = view
	var skills := world.skills(steered)
	var shown := mini(skills.size(), MAX_SLOTS)
	for slot in _slots.size():
		_slots[slot].visible = slot < shown
		if slot < shown:
			# A skill whose radius reaches past its target bursts; any other strikes one body.
			var icon := AtlasTexture.new()
			icon.atlas = _icons.texture
			var tag: String = "burst" if int(skills[slot].radius) > 0 else "strike"
			icon.region = Rect2(_icons.tags[tag] * 16, 0, 16, 16)
			slot_icons[slot].texture = icon
	_skill_bar.size.x = shown * 22 + 6
	_skill_bar.position.x = 240 - _skill_bar.size.x / 2.0


## Updates the bars, the map around the player and the technique bar: `skills` are the skill
## indices the player's last order chose.
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
	for slot in _slots.size():
		var style: StyleBoxFlat = _slots[slot].get_theme_stylebox("panel")
		style.border_color = JADE if skills.has(slot) else INK


## Shows the core's verdict on the player's challenge, one of the WorldAdapter.CHALLENGE_*
## constants, for NOTICE_MS: that the duel began, or why it was refused.
func show_challenge(refusal: int) -> void:
	_set_up()
	_notice(CHALLENGE_KEYS.get(refusal, "CHALLENGE_NONE"))


# Shows the player's faction's leader, and while a duel for its lead is fought, both duellists'
# health against what each had when it began; when it ends, whether the lead changed hands.
func _refresh_leadership(world: WorldAdapter, steered: int) -> void:
	var faction := world.army_faction(steered)
	var leader := world.leader(faction)
	var own := world.roster(steered)
	labels.leader_value.text = _who(leader, own) if leader >= 0 else tr("HUD_LEADER_NONE")
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
	labels.duel_challenger.text = _who(fought.challenger, own)
	labels.duel_leader.text = _who(fought.leader, own)
	duel_bars.challenger.value = 100.0 * world.person_health(fought.challenger) / _duel.from
	duel_bars.leader.value = 100.0 * world.person_health(fought.leader) / _duel.leader_from


# The player, if `person` is the player's own, or the person by number.
func _who(person: int, own: PackedInt64Array) -> String:
	return tr("HUD_LEADER_YOU") if own.has(person) else tr("HUD_PERSON") % person


func _notice(key: String) -> void:
	labels.notice.text = key
	labels.notice.visible = true
	_notice_until = Time.get_ticks_msec() + NOTICE_MS


func _process(_delta: float) -> void:
	if labels.has("notice") and labels.notice.visible and Time.get_ticks_msec() > _notice_until:
		labels.notice.visible = false


func _build() -> void:
	var player := _panel(Rect2(4, 4, 160, 50))
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
	labels.hp_value = _value(player, Vector2(42, 14), 112)
	labels.qi = _label(player, "HUD_QI", Vector2(42, 30), QI)
	labels.qi_value = _value(player, Vector2(42, 30), 112)
	_bar(player, hp_bar, Vector2(42, 26), HP)
	_bar(player, qi_bar, Vector2(42, 42), QI)
	# Inner energy is not in the core yet: its bar stands empty and its value is a dash.
	qi_bar.value = 0.0
	labels.qi_value.text = "—"

	var map := _panel(Rect2(392, 4, 84, 62))
	_map.position = Vector2(4, 4)
	_map.size = Vector2(MAP_SIZE)
	_map.stretch_mode = TextureRect.STRETCH_SCALE
	map.add_child(_map)
	_dot.color = JADE
	_dot.size = Vector2(2, 2)
	map.add_child(_dot)
	labels.season = _label(map, "HUD_SEASON_YEAR", Vector2(5, 48), PAPER)

	var rumour := _panel(Rect2(4, 230, 164, 36))
	labels.rumour = _label(rumour, "HUD_RUMOR", Vector2(5, 2), GOLD_LIGHT)
	labels.rumour_text = _label(rumour, "HUD_RUMOR_NONE", Vector2(5, 16), PAPER)

	_skill_bar = _panel(Rect2(0, 240, 0, 26))
	var bar := _skill_bar
	for slot in MAX_SLOTS:
		var cell := Panel.new()
		var style := StyleBoxFlat.new()
		style.bg_color = INK_DARK
		style.set_border_width_all(1)
		style.border_color = INK
		cell.add_theme_stylebox_override("panel", style)
		cell.position = Vector2(4 + slot * 22, 3)
		cell.size = Vector2(20, 20)
		bar.add_child(cell)
		var icon := TextureRect.new()
		icon.position = Vector2(2, 2)
		icon.size = Vector2(16, 16)
		cell.add_child(icon)
		slot_icons.append(icon)
		# The number key, in the slot's top-left corner over the icon.
		var number := Label.new()
		number.text = str(slot + 1)
		number.auto_translate_mode = Node.AUTO_TRANSLATE_MODE_DISABLED
		number.position = Vector2(1, -3)
		number.add_theme_color_override("font_color", GOLD_LIGHT)
		number.add_theme_color_override("font_outline_color", INK_DARK)
		number.add_theme_constant_override("outline_size", 2)
		cell.add_child(number)
		_slots.append(cell)

	labels.locale = _label(self, "HUD_LOCALE", Vector2(400, 68), STONE)

	var leadership := _panel(Rect2(4, 58, 160, 18))
	labels.leader = _label(leadership, "HUD_LEADER", Vector2(5, 2), GOLD_LIGHT)
	labels.leader_value = _value(leadership, Vector2(36, 2), 60)
	labels.leader_value.horizontal_alignment = HORIZONTAL_ALIGNMENT_LEFT
	labels.challenge_key = _label(leadership, "HUD_CHALLENGE_KEY", Vector2(100, 2), STONE)

	duel_panel = _panel(Rect2(176, 4, 128, 44))
	duel_panel.visible = false
	labels.duel = _label(duel_panel, "HUD_DUEL", Vector2(5, 3), HP)
	labels.duel_challenger = _value(duel_panel, Vector2(5, 15), 52)
	labels.duel_challenger.horizontal_alignment = HORIZONTAL_ALIGNMENT_LEFT
	labels.duel_leader = _value(duel_panel, Vector2(5, 28), 52)
	labels.duel_leader.horizontal_alignment = HORIZONTAL_ALIGNMENT_LEFT
	duel_bars.challenger = ProgressBar.new()
	duel_bars.leader = ProgressBar.new()
	_bar(duel_panel, duel_bars.challenger, Vector2(60, 20), HP)
	_bar(duel_panel, duel_bars.leader, Vector2(60, 33), HP)
	duel_bars.challenger.size.x = 62
	duel_bars.leader.size.x = 62

	labels.notice = _label(self, "", Vector2(90, 80), PAPER)
	labels.notice.size = Vector2(300, 12)
	labels.notice.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	labels.notice.add_theme_color_override("font_outline_color", INK_DARK)
	labels.notice.add_theme_constant_override("outline_size", 3)
	labels.notice.visible = false


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


func _bar(parent: Control, bar: ProgressBar, at: Vector2, colour: Color) -> void:
	var fill := StyleBoxFlat.new()
	fill.bg_color = colour
	var back := StyleBoxFlat.new()
	back.bg_color = INK_DARK
	bar.add_theme_stylebox_override("fill", fill)
	bar.add_theme_stylebox_override("background", back)
	bar.show_percentage = false
	bar.position = at
	bar.size = Vector2(112, 4)
	parent.add_child(bar)


# One pixel per cell: the terrain kind's colour, its owner's accent along territory borders,
# and each village's square in its owner's accent.
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
			if owners.size() == terrain.size() and owners.size() > 0 and x + 1 < _world_size.x \
					and owners[i] != owners[i + 1]:
				colour = ACCENTS[sprites[owners[i]]]
			if owners.size() == terrain.size() and owners.size() > 0 and y + 1 < _world_size.y \
					and owners[i] != owners[i + _world_size.x]:
				colour = ACCENTS[sprites[owners[i]]]
			image.set_pixel(x, y, colour)
	for village: Dictionary in world.villages():
		var corner: Vector2i = village.corner
		var area: Vector2i = village.area
		image.fill_rect(Rect2i(corner, area), ACCENTS[sprites[village.faction]].lightened(0.3))
	return image
