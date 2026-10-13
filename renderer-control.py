from pathlib import Path
import subprocess
p = Path('grade/grade.gdshader')
s = p.read_text()
try:
    p.write_text(s.replace('COLOR = vec4(clamp(g, 0.0, 1.0), 1.0);', 'COLOR = vec4(c, 1.0);'))
    with open('build/issue310/control-renderer.log','w') as log:
        r = subprocess.run(['godot','--path','.','--script','res://grade/grade_check.gd'],stdout=log,stderr=subprocess.STDOUT,timeout=60)
    print('renderer identity control exit', r.returncode)
    assert r.returncode == 1
finally:
    p.write_text(s)
