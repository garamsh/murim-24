import zlib, struct, colorsys, sys
sys.path.insert(0, sys.argv[1]); from ramps import ramps, hx, ang
def png(path, w, h, px):
    raw=b''.join(b'\x00'+bytes(sum((px[y*w+x] for x in range(w)),())) for y in range(h))
    def chunk(t,d): return struct.pack('>I',len(d))+t+d+struct.pack('>I',zlib.crc32(t+d)&0xffffffff)
    open(path,'wb').write(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',w,h,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(raw))+chunk(b'IEND',b''))
def build(base, n=7, hs=0.95, lo=0.62, hi=0.5, sat=1.06, cap=None, shue=250, lhue=55):
    r,g,b = hx(base); h,l,s = colorsys.rgb_to_hls(r,g,b); h*=360
    grey = s < 0.06; out=[]
    for k in range(n):
        u=((k+0.5)/n)*2-1
        L = l*(1+u*lo) if u<0 else l+(1-l)*u*hi
        target = shue if u<0 else lhue
        amt=abs(u)*hs
        t = ang(h,target)*min(0.5,amt*0.45)
        if cap is not None: t = max(-cap*abs(u), min(cap*abs(u), t))
        H2 = target if grey else h+t
        Sx = s*sat*((1+0.3*abs(u)*hs) if u<0 else (1-0.25*u*hs))
        if grey: Sx=0.12*abs(u)*hs
        rr,gg,bb = colorsys.hls_to_rgb((H2%360)/360, min(0.98,max(0.02,L)), min(1,max(0,Sx)))
        out.append(tuple(round(c*255) for c in (rr,gg,bb)))
    return out
variants = [dict(), dict(hi=0.8, cap=30)]
B=14; rows=[]
for name, r in ramps.items():
    base=r[len(r)//2]
    rows.append([hx2 for hx2 in [tuple(int(c[i:i+2],16) for i in (0,2,4)) for c in r]])
    for v in variants: rows.append(build(base, **v))
    rows.append(None)
w=7*B; h=len(rows)*B
px=[(40,40,40)]*(w*h)
for ri,row in enumerate(rows):
    if row is None: continue
    for k,c in enumerate(row):
        for y in range(B):
            for x in range(B): px[(ri*B+y)*w+k*B+x]=c
png(sys.argv[2], w, h, px)
