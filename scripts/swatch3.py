# swatch3.py <dir> -- swatch3.png: each ramp as now (cap 30, lights 0.8), then the same mixed toward
# a cool shadow colour and a warm light colour at the step's lightness, at two strengths.
import sys, colorsys
sys.argv=[None, sys.argv[1], sys.argv[1]+'/swatch3.png']
exec(open(sys.argv[1]+'/swatch.py').read().split("variants = ")[0])
def tinted(base, shade, light, sk, lk):
    out=[]
    for k,c in enumerate(build(base, hi=0.8, cap=30)):
        u=((k+0.5)/7)*2-1
        r,g,b=[v/255 for v in c]; h,l,s=colorsys.rgb_to_hls(r,g,b)
        t=[int(shade[i:i+2],16)/255 for i in (0,2,4)] if u<0 else [int(light[i:i+2],16)/255 for i in (0,2,4)]
        a=abs(u)*(sk if u<0 else lk)
        m=[r+(t[0]-r)*a, g+(t[1]-g)*a, b+(t[2]-b)*a]
        h2,l2,s2=colorsys.rgb_to_hls(*m)
        r2,g2,b2=colorsys.hls_to_rgb(h2,l,s2)
        out.append(tuple(round(max(0,min(1,v))*255) for v in (r2,g2,b2)))
    return out
B=28; names=list(ramps); cols=2; per=(len(names)+1)//2
w=cols*(7*B+16); h=per*4*B
px=[(40,40,40)]*(w*h)
for ni,name in enumerate(names):
    col,row0=ni//per,(ni%per)*4
    r=ramps[name]; base=r[len(r)//2]
    rows=[build(base,hi=0.8,cap=30), tinted(base,'3a2a7a','ffe7a0',0.35,0.25), tinted(base,'3a2a7a','ffe7a0',0.55,0.35)]
    for ri,row in enumerate(rows):
        for k,c in enumerate(row):
            for y in range(B-2):
                for x in range(B-2): px[((row0+ri)*B+y)*w+col*(7*B+16)+k*B+x]=c
png(sys.argv[2], w, h, px)
