# Synthetic proxy of a compiled 4,096 map: 4 bytes a cell (kind, elevation, biome, region),
# stored per 32-cell area. Measures raw and deflate-compressed size on a sample of areas.
import zlib, math, sys
N=4096; A=32; MAC=8
def h(x,y,s):
    v=(x*374761393 + y*668265263 + s*2147483647) & 0xffffffff
    v=((v^(v>>13))*1274126177)&0xffffffff
    return (v^(v>>16))&0xffff
def vnoise(x,y,scale,s):
    gx,fx=divmod(x,scale); gy,fy=divmod(y,scale)
    a=h(gx,gy,s);b=h(gx+1,gy,s);c=h(gx,gy+1,s);d=h(gx+1,gy+1,s)
    top=a*(scale-fx)+b*fx; bot=c*(scale-fx)+d*fx
    return (top*(scale-fy)+bot*fy)//(scale*scale)   # 0..65535
def fbm(x,y,s):
    return (vnoise(x,y,64,s)*4+vnoise(x,y,16,s+1)*2+vnoise(x,y,4,s+2))//7
regions=[(2300,1750),(1660,1810),(2400,900),(800,2600),(2500,2700),(3600,2450),(500,900),(800,3600),(2000,350),(3800,350)]
# macro layers at 512 x 512, computed lazily and cached
mcache={}
def macro(mx,my):
    k=(mx,my)
    if k in mcache: return mcache[k]
    x,y=mx*MAC,my*MAC
    e=fbm(mx,my,7)
    edge=min(x,y,N-1-x,N-1-y)
    if x>3900: tier=0                      # sea
    elif edge<128: tier=7                  # frame of ranges
    else: tier=1+min(6,max(0,(e-20000)//6000))
    best=None
    for i,(rx,ry) in enumerate(regions):
        dd=(rx-x)**2+(ry-y)**2+ (fbm(mx,my,11+i)-32768)*200
        if best is None or dd<best[0]: best=(dd,i)
    reg=best[1]
    biome=(reg*3+min(2,tier//3))%24
    mcache[k]=(tier,biome,reg); return mcache[k]
# rivers and roads as polylines rasterised by distance
def seg_d2(px,py,ax,ay,bx,by):
    vx,vy=bx-ax,by-ay;wx,wy=px-ax,py-ay
    t=max(0,min(1,(wx*vx+wy*vy)/(vx*vx+vy*vy)))
    dx,dy=px-(ax+t*vx),py-(ay+t*vy);return dx*dx+dy*dy
def meander(pts,s):
    out=[]
    for (ax,ay),(bx,by) in zip(pts,pts[1:]):
        for i in range(16):
            t=i/16;x=ax+(bx-ax)*t;y=ay+(by-ay)*t
            out.append((x+(h(int(x),int(y),s)%120-60),y+(h(int(y),int(x),s)%120-60)))
    out.append(pts[-1]);return out
rivers=[(meander([(1000,600),(1700,500),(1900,1700),(2600,1650),(3900,1300)],1),5),
        (meander([(300,2400),(1230,2875),(2090,2630),(2640,2655),(3455,2320),(3900,2400)],2),8),
        (meander([(500,1900),(1660,1790),(1910,1745)],3),3),
        (meander([(2200,2050),(3150,2050),(3700,2000)],4),3)]
roads=[(meander([(2300,1750),(1660,1810),(1045,2290),(775,2630)],5),1),
       (meander([(2300,1750),(2320,1030),(3020,580),(3780,380)],6),1),
       (meander([(2300,1750),(2640,2655),(3455,2320),(3785,2490)],7),1),
       (meander([(1660,1810),(505,1020),(230,330)],8),1)]
def lines_near(feats,x0,y0):
    out=[]
    for pts,w in feats:
        segs=[(a,b) for a,b in zip(pts,pts[1:]) if min(a[0],b[0])-w-40<x0+A and max(a[0],b[0])+w+40>x0 and min(a[1],b[1])-w-40<y0+A and max(a[1],b[1])+w+40>y0]
        if segs: out.append((segs,w))
    return out
def area_bytes(ax,ay):
    x0,y0=ax*A,ay*A
    rv=lines_near(rivers,x0,y0); rd=lines_near(roads,x0,y0)
    kind=bytearray();elev=bytearray();bio=bytearray();reg=bytearray()
    for y in range(y0,y0+A):
        for x in range(x0,x0+A):
            # dither macro borders: jitter the lookup by up to 3 cells
            jx=x+(h(x,y,99)%7)-3; jy=y+(h(y,x,98)%7)-3
            jx=min(N-1,max(0,jx)); jy=min(N-1,max(0,jy))
            t,b,r=macro(jx//MAC,jy//MAC)
            k=10+b//3
            if t==0: k=1
            elif t==7: k=3
            for segs,w in rv:
                if any(seg_d2(x,y,a[0],a[1],c[0],c[1])<=w*w for a,c in segs): k=2; t=1; break
            if k!=2:
                for segs,w in rd:
                    if any(seg_d2(x,y,a[0],a[1],c[0],c[1])<=w*w for a,c in segs): k=4; break
            # cliff where the tier rises east or south of the cell
            tn,_,_=macro(min(N-1,x+1)//MAC,y//MAC)
            shape= 1 if tn>t else 0
            noisy=h(x,y,77)%10==0
            if noisy and k>=10: b=(b+1)%24; k=10+b//3
            if h(x,y,55)%16==0 and 0<t<7: t+=1
            kind.append(k); elev.append(t|(shape<<3)); bio.append(b); reg.append(r)
    return bytes(kind+elev+bio+reg)
step=int(sys.argv[1]) if len(sys.argv)>1 else 8
raw=comp=0;n=0;mx=0
for ay in range(0,N//A,step):
    for ax in range(0,N//A,step):
        b=area_bytes(ax,ay); c=len(zlib.compress(b,6))
        raw+=len(b);comp+=c;n+=1;mx=max(mx,c)
scale=(N//A)**2/n
print(f"areas sampled {n}; raw/area {raw//n} B; deflate mean/area {comp/n:.0f} B, max {mx} B; whole map raw {raw*scale/2**20:.1f} MiB, deflate {comp*scale/2**20:.2f} MiB")
