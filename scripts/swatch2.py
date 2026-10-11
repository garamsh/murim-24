# swatch2.py <dir with ramps.py and swatch.py> -- draws swatch2.png there: for each master ramp,
# three rows of 28-pixel steps, the former ramp, the mockup's builder from its middle step, and the
# builder with lights rising 0.8 of the way and the hue turn capped at 30 degrees times |u|.
import sys; sys.argv=[None, sys.argv[1], sys.argv[1]+'/swatch2.png']
exec(open(sys.argv[1]+'/swatch.py').read().split("variants = ")[0])
variants=[dict(), dict(hi=0.8, cap=30)]
B=28; names=list(ramps); cols=2; per=(len(names)+1)//2
w=cols*(7*B+16); h=per*4*B
px=[(40,40,40)]*(w*h)
for ni,name in enumerate(names):
    col, row0 = ni//per, (ni%per)*4
    r=ramps[name]; base=r[len(r)//2]
    rows=[[tuple(int(c[i:i+2],16) for i in (0,2,4)) for c in r]]+[build(base,**v) for v in variants]
    for ri,row in enumerate(rows):
        for k,c in enumerate(row):
            for y in range(B-2):
                for x in range(B-2): px[((row0+ri)*B+y)*w+col*(7*B+16)+k*B+x]=c
png(sys.argv[2], w, h, px)
