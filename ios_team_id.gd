# Writes the App Store Team ID given after `--` into the iOS preset's options in the uncommitted
# .godot/export_credentials.cfg, which Godot reads over export_presets.cfg, keeping every other
# credential there. `make ios` runs it, so the committed preset keeps the field empty.
extends SceneTree

const PRESETS := "res://export_presets.cfg"
const CREDENTIALS := "res://.godot/export_credentials.cfg"
const PRESET := "iOS"
const KEY := "application/app_store_team_id"


func _init() -> void:
	var args := OS.get_cmdline_user_args()
	var team_id: String = args[0] if args.size() == 1 else ""
	var err := merge(PRESETS, CREDENTIALS, team_id)
	if err != OK:
		printerr("ios_team_id: could not set %s on the %s preset in %s: %s" % [KEY, PRESET, CREDENTIALS, error_string(err)])
		quit(1)
		return
	print("ios_team_id: set %s on the %s preset in %s" % [KEY, PRESET, CREDENTIALS])
	quit(0)


## Sets `team_id` on the preset named iOS in `presets_path`, within `credentials_path`, keeping its
## other entries. Refuses an ID that is not ten capital letters or digits, and presets with no iOS one.
static func merge(presets_path: String, credentials_path: String, team_id: String) -> Error:
	if RegEx.create_from_string("^[A-Z0-9]{10}$").search(team_id) == null:
		return ERR_INVALID_PARAMETER
	var presets := ConfigFile.new()
	var err := presets.load(presets_path)
	if err != OK:
		return err
	var options := ""
	for section in presets.get_sections():
		if section.count(".") == 1 and presets.get_value(section, "name", "") == PRESET:
			options = section + ".options"
	if options.is_empty():
		return ERR_DOES_NOT_EXIST
	var credentials := ConfigFile.new()
	err = credentials.load(credentials_path)
	if err != OK and err != ERR_FILE_NOT_FOUND:
		return err
	credentials.set_value(options, KEY, team_id)
	return credentials.save(credentials_path)
