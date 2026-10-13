from pathlib import Path
import subprocess
p = Path('grade/grade.gdshader')
s = p.read_text()
try:
    p.write_text(s.replace('(cell + 0.5) * art_pixel * SCREEN_PIXEL_SIZE * 2.0 - 1.0', 'SCREEN_UV * 2.0 - 1.0'))
    with open('build/issue310/control-pixel-grid.log','w') as log:
        r = subprocess.run(['godot','--path','.','--script','build/issue310/pixel-grid.gd'],stdout=log,stderr=subprocess.STDOUT,timeout=60)
    print('pixel grid control exit', r.returncode)
    assert r.returncode == 1
finally:
    p.write_text(s)
