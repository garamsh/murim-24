from pathlib import Path
import subprocess
root = Path.cwd()
report = root / 'build/issue310'
cases = [
('material', 'grade/grade.tscn', 'resource_local_to_scene = true', 'resource_local_to_scene = false', ['grade/grade_test.gd']),
('season', 'grade/grade.gd', 'grade.gain *= _vector(of_season.gain)', 'grade.gain *= Vector3.ONE', ['grade/grade_test.gd']),
('blend', 'grade/grade.gd', '_vector(a[field]).lerp(_vector(b[field]), share)', '_vector(a[field])', ['grade/grade_test.gd']),
('quality', 'grade/grade.gd', '0.0 if quality == Quality.LOW else VIGNETTE', 'VIGNETTE', ['grade/grade_test.gd']),
('visibility', 'grade/grade.gd', 'visible = true', 'visible = false', ['grade/grade_test.gd']),
('settings', 'hud/settings.gd', 'return chosen.screen', 'return 1', ['hud/settings_test.gd']),
('clock', 'play/play.gd', 'float(date.tick) / date.ticks_per_day', '0.0', ['play/play_test.gd']),
('autosave', 'play/play.gd', 'return [date.year, date.season, date.day]', 'return [date.year, date.season, date.day, date.tick]', ['play/play_test.gd']),
]
for name, file, before, after, tests in cases:
    path = root / file
    original = path.read_text()
    assert before in original, (name, file)
    try:
        path.write_text(original.replace(before, after, 1))
        result = subprocess.run(['godot','--headless','--path','.','--script','res://run_tests.gd','--',*tests], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=180)
        (report / f'control-{name}.log').write_bytes(result.stdout)
        print(f'{name}: exit {result.returncode}', flush=True)
        assert result.returncode == 1, (name, result.returncode)
    finally:
        path.write_text(original)
