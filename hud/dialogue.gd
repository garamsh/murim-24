## The dialogue window, drawn as art/mockup/dialogue.gd lays it out (evidence/pr-180), at the foot of
## the room so the two people talking stay in view above it: the person talked to, its card, name,
## faction, sect and age, and what it says: its greeting over a menu of what it offers, one row for
## each of WorldAdapter.offers, then farewell; or the news it has heard, its own
## WorldAdapter.rumours, newest first, NEWS_LINES at a time; or, from a seller, its wares as
## art/mockup/trade.gd lays them out (evidence/pr-181): each ware's icon, name, effect, how many the
## buyer holds and its price, the buyer's silver, and a line for what was bought or why not. Every
## word comes from hud.csv through
## Godot's translations. What it shows is read from the world through the adapter. It holds no game
## rule.
extends "res://hud/panel.gd"

const Arts := preload("res://hud/arts.gd")
## The menu row that ends the talk, after what the person offers.
const FAREWELL := -1
## The hud.csv key of each offer's row, by WorldAdapter.OFFER_* constant.
const OFFER_KEYS := {WorldAdapter.OFFER_NEWS: "DIALOGUE_NEWS", WorldAdapter.OFFER_TRADE: "DIALOGUE_TRADE"}
## The hud.csv key of each reason a purchase is refused, by WorldAdapter.BUY_* constant.
const BUY_KEYS := {
	WorldAdapter.BUY_DEAD: "BUY_DEAD",
	WorldAdapter.BUY_CANNOT_TALK: "BUY_CANNOT_TALK",
	WorldAdapter.BUY_NOT_SOLD: "BUY_NOT_SOLD",
	WorldAdapter.BUY_NO_SILVER: "BUY_NO_SILVER",
}
## How many lines of news show at once.
const NEWS_LINES := 5
# The layout, in the native 480x270, standing on the room's foot.
const RECT := Rect2(20, 90, 440, 146)
const CARD_RECT := Rect2(30, 116, 84, 90)
const SAY_X := 132
const SAY_Y := 116
const LINE := 14
const WARE_ROW := 20

## The person talked to, or -1 while the window is closed.
var person := -1
## The rows of the menu: what the person offers, as WorldAdapter.OFFER_* constants, then FAREWELL.
var rows: Array[int] = []
## The row chosen.
var chosen := 0
## The news the person tells, each line as it is drawn, newest first, while the news is shown;
## empty while the menu is.
var news: Array[String] = []
## Whether the news is shown in place of the menu.
var showing_news := false
## The first line of news shown.
var first := 0
## Whether the wares are shown in place of the menu, the person they are shown to, the ware
## chosen, and the line under them: what was bought, or why not when `refused`.
var trading := false
var buyer := -1
var ware := 0
var line := ""
var refused := false


## Opens the window on `talked_to` in `world`, at its menu.
func open(world: WorldAdapter, talked_to: int) -> void:
	_world = world
	person = talked_to
	rows.clear()
	for offer in world.offers(talked_to):
		rows.append(offer)
	rows.append(FAREWELL)
	chosen = 0
	_show_menu()
	visible = true
	queue_redraw()


func close() -> void:
	super.close()
	person = -1


## Returns the row chosen: an offer, as a WorldAdapter.OFFER_* constant, or FAREWELL.
func choice() -> int:
	return rows[chosen]


## Moves the choice on the menu, or the news, by `way` rows. Returns whether it moved.
func move(way: int) -> bool:
	var before := first if showing_news else chosen
	if trading:
		var was := ware
		ware = clampi(ware + way, 0, _world.wares().size() - 1)
		queue_redraw()
		return ware != was
	if showing_news:
		first = clampi(first + way, 0, maxi(news.size() - NEWS_LINES, 0))
	else:
		chosen = clampi(chosen + way, 0, rows.size() - 1)
	queue_redraw()
	return (first if showing_news else chosen) != before


## Shows the news the person has heard, as WorldAdapter.rumours gives it, in place of the menu.
func show_news() -> void:
	news.clear()
	for event: Dictionary in _world.rumours(person):
		news.append(rumour_text(_world, event))
	first = 0
	showing_news = true
	queue_redraw()


## Shows the person's wares, as WorldAdapter.wares gives them, to person `to`, in place of the menu.
func show_wares(to: int) -> void:
	trading = true
	buyer = to
	ware = 0
	line = ""
	refused = false
	queue_redraw()


## Shows what came of buying the ware chosen: `refusal`, one of the WorldAdapter.BUY_* constants,
## BUY_NONE for bought.
func show_bought(refusal: int) -> void:
	refused = refusal != WorldAdapter.BUY_NONE
	line = tr(BUY_KEYS[refusal]) if refused else tr("DIALOGUE_BOUGHT") % ware_name(_world.wares()[ware])
	queue_redraw()


## Returns `shown`, a ware as WorldAdapter.wares gives it, named in the current locale.
func ware_name(shown: Dictionary) -> String:
	return tr("WARE_" + String(shown.name).to_upper())


## Returns what `shown`, a ware as WorldAdapter.wares gives it, restores, in the current locale.
func ware_effect(shown: Dictionary) -> String:
	if int(shown.health) > 0:
		return tr("WARE_HEALTH") % int(shown.health)
	return tr("WARE_QI") % Arts.grouped(int(shown.qi))


## Goes back from the news or the wares to the menu. Returns whether either was shown.
func back() -> bool:
	var was := showing_news or trading
	_show_menu()
	queue_redraw()
	return was


## Works the window with `event`: up and down choose what to ask, scroll the news or choose a ware,
## Enter asks what is chosen, the news shown here and farewell ending the talk, or buys the ware
## chosen, and Escape goes back from the news or the wares or ends the talk. Returns "buy",
## "wares", "close", "done", or "" when the key did nothing.
func key(event: InputEventKey) -> String:
	if event.keycode == KEY_ESCAPE:
		return "done" if back() else "close"
	if DIRECTIONS.has(event.keycode):
		return _done(move(DIRECTIONS[event.keycode].y))
	if not ENTER_KEYS.has(event.keycode) or (showing_news and not trading):
		return ""
	if trading:
		return "buy"
	if choice() == WorldAdapter.OFFER_NEWS:
		show_news()
		return "done"
	return "wares" if choice() == WorldAdapter.OFFER_TRADE else "close"


## Works the window with a tap anywhere: it ends the talk. Returns "close".
func tap(_at: Vector2) -> String:
	return "close"


func _show_menu() -> void:
	showing_news = false
	trading = false
	line = ""
	news.clear()
	first = 0


## Returns `event`, a rumour as WorldAdapter.rumours gives it, worded in the current locale.
func rumour_text(world: WorldAdapter, event: Dictionary) -> String:
	var faction := func(index: int) -> String:
		return Affiliation.faction_name(world, index)
	var named := func(index: int) -> String:
		return PersonName.of(world, index, true)
	match event.kind:
		"founded":
			return tr("RUMOUR_FOUNDED") % faction.call(event.faction)
		"raided":
			return tr("RUMOUR_RAIDED") % [faction.call(event.raider), faction.call(event.owner)]
		"ruined":
			return tr("RUMOUR_RUINED") % faction.call(event.owner)
		"sect_fell":
			return tr("RUMOUR_SECT_FELL") % Affiliation.sect_name(world, event.sect)
		"leader_changed":
			var key := "RUMOUR_DUEL" if event.by == "duel" else "RUMOUR_ELECTED"
			return tr(key) % [named.call(event.leader), faction.call(event.faction)]
		"born":
			return tr("RUMOUR_BORN") % named.call(event.person)
		_:
			return tr("RUMOUR_DIED") % named.call(event.person)


# The first ware the person sells, or an empty Dictionary for none.
func _sold() -> Dictionary:
	var wares := _world.wares()
	for n in wares.size():
		if _world.sells(person, n):
			return wares[n]
	return {}


## Returns occupation `occupation` of `world` named in the current locale.
static func occupation_name(world: WorldAdapter, occupation: int) -> String:
	return TranslationServer.translate("OCCUPATION_" + world.occupation_names()[occupation].to_upper())


## Returns the room, in pixels, the window gives a line of news or a menu row.
func line_room() -> float:
	return RECT.end.x - 12 - SAY_X


## Returns the room, in pixels, the card's column gives the person's faction, sect and age.
func card_room() -> float:
	return CARD_RECT.size.x


## Returns the person's particulars under its card: its faction's name, its sect's or none, or for
## a seller its occupation, and its age, in the current locale.
func particulars() -> Array[String]:
	var faction := _world.person_faction(person)
	var sect := _world.person_sect(person)
	var faction_name: String = Affiliation.faction_name(_world, faction) if faction >= 0 else ""
	var sect_name: String = Affiliation.sect_name(_world, sect) if sect >= 0 else tr("DIALOGUE_NO_SECT")
	var sold := _sold()
	if not sold.is_empty():
		sect_name = occupation_name(_world, _world.person_occupation(person))
	return [faction_name, sect_name, tr("DIALOGUE_AGE") % _world.person_age(person)]


func _draw() -> void:
	if person < 0:
		return
	_font = get_theme_default_font() if _font == null else _font
	var keys := tr("DIALOGUE_KEYS_TRADE" if trading else "DIALOGUE_KEYS_NEWS" if showing_news else "DIALOGUE_KEYS_MENU")
	var title := fit(PersonName.of(_world, person), RECT.size.x - 32 - _width(tr("DIALOGUE_CLOSE")))
	_dialog(RECT, title, "" if showing_news or trading else tr("DIALOGUE_CLOSE"), keys)
	# The card: the person standing twice its size on a cell of ground, and its particulars.
	var card := CARD_RECT
	var body := Rect2(card.position + Vector2(26, 2), Vector2(32, 48))
	draw_rect(body, Palette.INK_1)
	draw_rect(Rect2(card.position + Vector2(26, 34), Vector2(32, 16)), Palette.GRASS_3)
	draw_rect(Rect2(card.position + Vector2(26, 34), Vector2(32, 1)), Palette.GRASS_4)
	_portrait(person, body, false)
	var said := particulars()
	var faction := _world.person_faction(person)
	var faction_colour := _faction_colour(faction) if faction >= 0 else Palette.STONE_4
	_text(Vector2(card.position.x, card.position.y + 54), fit(said[0], card.size.x), faction_colour)
	_text(Vector2(card.position.x, card.position.y + 68), fit(said[1], card.size.x), Palette.JADE_4 if not _sold().is_empty() else Palette.STONE_4)
	_text(Vector2(card.position.x, card.position.y + 82), fit(said[2], card.size.x), Palette.STONE_4)
	draw_rect(Rect2(SAY_X - 10, RECT.position.y + 26, 1, RECT.size.y - 52), Palette.INK_4)
	var room := line_room()
	if trading:
		_draw_wares(room)
		return
	if not showing_news:
		_text(Vector2(SAY_X, SAY_Y), fit(tr("DIALOGUE_GREETING"), room), Palette.GOLD_6)
		var items: Array[Dictionary] = []
		for row in rows:
			items.append({"text": fit(tr("DIALOGUE_FAREWELL" if row == FAREWELL else OFFER_KEYS[row]), room - 16)})
		_menu(Vector2(SAY_X, SAY_Y + 22), room, items, chosen)
		return
	_text(Vector2(SAY_X, SAY_Y), tr("DIALOGUE_HEARD"), Palette.GOLD_6)
	if news.is_empty():
		_text(Vector2(SAY_X, SAY_Y + 18), fit(tr("DIALOGUE_NONE"), room), Palette.PAPER_6)
		return
	var count := "%d / %d" % [mini(first + NEWS_LINES, news.size()), news.size()]
	_text(Vector2(RECT.end.x - 12 - _width(count), SAY_Y), count, Palette.STONE_3)
	for n in mini(NEWS_LINES, news.size() - first):
		var y := SAY_Y + 18 + n * LINE
		draw_rect(Rect2(SAY_X, y + 5, 3, 3), Palette.GOLD_3)
		_text(Vector2(SAY_X + 8, y), fit(news[first + n], room - 8), Palette.PAPER_6 if first + n == 0 else Palette.PAPER_4)


# The wares, from the greeting down: the buyer's silver at the right, a row for each ware, and the
# line for what came of the last purchase.
func _draw_wares(room: float) -> void:
	_text(Vector2(SAY_X, SAY_Y), fit(tr("DIALOGUE_TRADE_GREETING"), room - 80), Palette.GOLD_6)
	var right := RECT.end.x - 12
	var silver := _world.person_silver(buyer)
	var purse := tr("SILVER") % silver
	_text(Vector2(right - _width(purse), SAY_Y), purse, Palette.PAPER_6)
	_ware_icon("silver", Vector2(right - _width(purse) - 18, SAY_Y - 2))
	var wares := _world.wares()
	var held := _world.person_held(buyer)
	for n in wares.size():
		var shown: Dictionary = wares[n]
		var y := SAY_Y + 22 + n * WARE_ROW
		if n > 0:
			draw_rect(Rect2(SAY_X, y - 3, room, 1), Palette.INK_4)
		var on := n == ware
		if on:
			draw_rect(Rect2(SAY_X, y - 2, room, WARE_ROW - 2), Palette.INK_2)
			draw_rect(Rect2(SAY_X, y - 2, 1, WARE_ROW - 2), Palette.JADE_4)
		_ware_icon(shown.name, Vector2(SAY_X + 4, y - 1))
		_text(Vector2(SAY_X + 24, y + 1), fit(ware_name(shown), 104), Palette.GOLD_6 if on else Palette.PAPER_6)
		_text(Vector2(SAY_X + 132, y + 1), fit(ware_effect(shown), 70),
				Palette.CRIMSON_5 if int(shown.health) > 0 else Palette.AZURE_5)
		_text(Vector2(SAY_X + 204, y + 1), fit(tr("WARE_HELD") % held[n], 44), Palette.STONE_4)
		var price := tr("SILVER") % int(shown.price)
		_text(Vector2(right - 4 - _width(price), y + 1), price, Palette.GOLD_4 if silver >= int(shown.price) else Palette.STONE_3)
	if not line.is_empty():
		var y := SAY_Y + 22 + wares.size() * WARE_ROW + 6
		_text(Vector2(SAY_X, y), fit(line, room), Palette.CRIMSON_5 if refused else Palette.JADE_4)
