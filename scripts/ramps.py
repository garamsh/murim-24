# Port of the PM mockup's buildRamp (style 2) to preview 7-step ramps from each master ramp's middle.
import colorsys, sys
ramps = {
  "ink": ["14101a","2a2433","3d3647","524a5e"],
  "stone": ["3b4250","4a5361","66707d","8a94a0","b4bcc4","dfe3e6"],
  "grass": ["1f3a2b","2c4a32","3b6138","4f7a3f","6b9447","8fae5a"],
  "leaf": ["142a22","1e3328","284536","35593d","4a7044"],
  "earth": ["3a2c24","4a3a2c","6b523a","8c6d4a","b08d60","d1b787"],
  "wood": ["2e1f1a","3b2a22","5c3d2a","7d5535","9c7048"],
  "water": ["1a2f40","1f3a4d","2b5770","3d7a8f","5fa3a8","a6d4cf"],
  "crimson": ["3d0d14","5c1418","9c1f24","d63a2f","f06a3c"],
  "azure": ["152a55","1f3d7a","2f63b8","5b9ae0","a8d0f5"],
  "amber": ["4a2e0c","6b4310","b07418","e8a830","f5d27a"],
  "skin": ["5c3a28","8a5a3c","c48a62","e8b98e"],
  "cloth": ["6b6052","8c7f68","b4a68a","d9cdb0"],
  "robe": ["8a94a0","c8ccd0","f4f1e6"],
  "jade": ["1f5c4a","2f8c6e","4fc49a","a6f0cf"],
  "gold": ["5c4418","8c6a2a","c99a3c","f0d080"],
  "paper": ["a8946c","d9c9a3","efe3c2"],
  "lacquer": ["3d1418","7a2a24","a83a2c"],
  "violet": ["2a1438","46205e","6a3290","9a5cd0","c99cf0"],
}
def hx(h): return tuple(int(h[i:i+2],16)/255 for i in (0,2,4))
def ang(h,t): return ((t-h)%360+540)%360-180
def build(base, n=7, hs=0.95, lo=0.62, hi=0.5, sat=1.06):
    r,g,b = hx(base); h,l,s = colorsys.rgb_to_hls(r,g,b); h*=360
    grey = s < 0.06; out=[]
    for k in range(n):
        u=((k+0.5)/n)*2-1
        L = l*(1+u*lo) if u<0 else l+(1-l)*u*hi
        target = 250 if u<0 else 55
        amt=abs(u)*hs
        H2 = target if grey else h+ang(h,target)*min(0.5,amt*0.45)
        Sx = s*sat*((1+0.3*abs(u)*hs) if u<0 else (1-0.25*u*hs))
        if grey: Sx=0.12*abs(u)*hs
        rr,gg,bb = colorsys.hls_to_rgb((H2%360)/360, min(0.98,max(0.02,L)), min(1,max(0,Sx)))
        out.append('%02x%02x%02x'%tuple(round(c*255) for c in (rr,gg,bb)))
    return out
if __name__=="__main__":
    for name, r in ramps.items():
        base = r[len(r)//2]
        print(name.ljust(8), base, ' '.join(build(base)))
