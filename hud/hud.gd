## The heads-up display over the play scene, in the tone of docs/art-direction.md §Effects and
## HUD: the player's portrait, faction, health and inner energy with their values; a map of the
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

## Labels that show a translated key or a value, by what they show, so a test can read them.
var labels := {}
## The icon in each technique slot, in key order.
var slot_icons: Array[TextureRect] = []

var _theme := Theme.new()
var _portrait := TextureRect.new()
var _hp := ProgressBar.new()
var _qi := ProgressBar.new()
var _map := TextureRect.new()
var _map_texture: ImageTexture
var _dot := ColorRect.new()
var _slots: Array[Panel] = []
var _icons := {}
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
	_hp.value = 100.0 * strength / full
	labels.hp_value.text = "%d/%d" % [strength, full]
	var lead := world.body_positions(steered)[0] / WorldAdapter.CELL_SIZE
	var corner := (Vector2i(lead) - MAP_SIZE / 2).clamp(Vector2i.ZERO,
			(_world_size - MAP_SIZE).max(Vector2i.ZERO))
	(_map.texture as AtlasTexture).region = Rect2(corner, MAP_SIZE)
	_dot.position = _map.position + (lead - Vector2(corner)).floor() - Vector2.ONE
	for slot in _slots.size():
		var style: StyleBoxFlat = _slots[slot].get_theme_stylebox("panel")
		style.border_color = JADE if skills.has(slot) else INK


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
	_bar(player, _hp, Vector2(42, 26), HP)
	_bar(player, _qi, Vector2(42, 42), QI)
	# Inner energy is not in the core yet: its bar stands full and its value is a dash.
	_qi.value = 100.0
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
