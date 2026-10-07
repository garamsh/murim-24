# Checks that `make ios`'s Team ID lands on the iOS preset's options in the credentials file and
# keeps the Android preset's secret there. Controls: an ID that is not ten capital letters or digits,
# and presets with no iOS one, are refused, while the same calls with a valid ID and an iOS preset pass.
extends RefCounted

const IosTeamId := preload("res://ios_team_id.gd")
const TEAM_ID := "ABCDE12345"
const SECRET_KEY := "keystore/debug_password"
const SECRET := "android"
const KEY := "application/app_store_team_id"


func run() -> bool:
	var dir := OS.get_temp_dir().path_join("ios_team_id_test_%d" % OS.get_process_id())
	DirAccess.make_dir_recursive_absolute(dir)
	var both := dir.path_join("both.cfg")
	var android_only := dir.path_join("android_only.cfg")
	var credentials := dir.path_join("export_credentials.cfg")
	_presets(both, ["Android", "iOS"])
	_presets(android_only, ["Android"])
	var stored := ConfigFile.new()
	stored.set_value("preset.0.options", SECRET_KEY, SECRET)
	stored.save(credentials)
	var set_up: bool = _read(credentials, "preset.0.options", SECRET_KEY) == SECRET

	var lowercase := IosTeamId.merge(both, credentials, TEAM_ID.to_lower())
	var short := IosTeamId.merge(both, credentials, TEAM_ID.left(9))
	var refused_untouched := _read(credentials, "preset.1.options", KEY) == null
	var no_ios := IosTeamId.merge(android_only, credentials, TEAM_ID)
	var accepted := IosTeamId.merge(both, credentials, TEAM_ID)
	var team_id: Variant = _read(credentials, "preset.1.options", KEY)
	var secret: Variant = _read(credentials, "preset.0.options", SECRET_KEY)

	for file in [both, android_only, credentials]:
		DirAccess.remove_absolute(file)
	DirAccess.remove_absolute(dir)
	var right: bool = set_up and lowercase == ERR_INVALID_PARAMETER and short == ERR_INVALID_PARAMETER \
			and refused_untouched and no_ios == ERR_DOES_NOT_EXIST and accepted == OK \
			and team_id == TEAM_ID and secret == SECRET
	print("ios_team_id_test: set up %s; lowercase %s, nine characters %s, no iOS preset %s, left untouched %s; valid %s wrote %s and kept the Android secret %s: %s" % [
		set_up, error_string(lowercase), error_string(short), error_string(no_ios), refused_untouched,
		error_string(accepted), team_id, secret, "pass" if right else "FAIL"])
	return right


func _presets(path: String, platforms: Array[String]) -> void:
	var presets := ConfigFile.new()
	for index in platforms.size():
		presets.set_value("preset.%d" % index, "name", platforms[index])
		presets.set_value("preset.%d.options" % index, "application/bundle_identifier", "com.example.murim24")
	presets.save(path)


func _read(path: String, section: String, key: String) -> Variant:
	var file := ConfigFile.new()
	file.load(path)
	return file.get_value(section, key) if file.has_section_key(section, key) else null
