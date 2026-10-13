from pathlib import Path
import subprocess
p = Path('extension/world_adapter.cpp')
s = p.read_text()
needle = 'world().tick_count() % per_day'
assert needle in s
try:
    p.write_text(s.replace(needle, '0', 1))
    with open('build/issue310/control-date-build.log','w') as log:
        subprocess.run(['.venv/bin/cmake','--build','build/extension'],stdout=log,stderr=subprocess.STDOUT,check=True)
    with open('build/issue310/control-date.log','w') as log:
        result = subprocess.run(['godot','--headless','--path','.','--script','res://run_tests.gd','--','extension/world_adapter_test.gd'],stdout=log,stderr=subprocess.STDOUT,timeout=180)
    print('date control exit', result.returncode)
    assert result.returncode == 1
finally:
    p.write_text(s)
    with open('build/issue310/control-date-restored-build.log','w') as log:
        subprocess.run(['.venv/bin/cmake','--build','build/extension'],stdout=log,stderr=subprocess.STDOUT,check=True)
