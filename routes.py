import math
# (map x, map y) in cells on the 4,096 map, north up; (lon, lat) real
P = {
 'Luoyang':   ((2300,1750),(112.45,34.62)),
 'Hangu':     ((2020,1745),(110.92,34.63)),
 'Tong':      ((1910,1750),(110.30,34.60)),
 'Changan':   ((1660,1810),(108.94,34.34)),
 'Huashan':   ((1870,1780),(110.08,34.48)),
 'Kaifeng':   ((2640,1710),(114.31,34.80)),
 'Songshan':  ((2390,1775),(112.95,34.50)),
 'Mengjin':   ((2300,1700),(112.44,34.83)),
 'Jingxing':  ((2600,990),(114.10,38.05)),
 'Taiyuan':   ((2320,1030),(112.55,37.87)),
 'Yanjing':   ((3020,580),(116.40,39.90)),
 'Shanhai':   ((3560,560),(119.77,40.00)),
 'Liaoyang':  ((3780,380),(123.17,41.27)),
 'Nanyang':   ((2310,2110),(112.53,33.00)),
 'Xiangyang': ((2245,2320),(112.14,32.04)),
 'Wudang':    ((2035,2240),(111.00,32.40)),
 'Wuchang':   ((2640,2655),(114.30,30.55)),
 'Jianmen':   ((1045,2290),(105.55,32.20)),
 'Hanzhong':  ((1270,2040),(107.03,33.07)),
 'Chengdu':   ((775,2630),(104.07,30.67)),
 'Chongqing': ((1230,2875),(106.55,29.56)),
 'Kuimen':    ((1760,2560),(109.50,31.00)),
 'Yichang':   ((2090,2630),(111.30,30.70)),
 'Nanjing':   ((3455,2320),(118.80,32.06)),
 'Yangzhou':  ((3565,2245),(119.40,32.40)),
 'Suzhou':    ((3785,2490),(120.60,31.30)),
 'Hangzhou':  ((3700,2720),(120.15,30.27)),
 'Huaiford':  ((3150,2050),(117.40,32.95)),
 'Kongtong':  ((1215,1550),(106.50,35.55)),
 'Liangzhou': ((505,1020),(102.60,37.93)),
 'Jiayu':     ((330,760),(98.20,39.80)),
 'Tianshan':  ((230,330),(88.00,43.80)),
 'Dali':      ((420,3560),(100.23,25.60)),
 'Meiling':   ((2700,3480),(114.30,25.30)),
 'Guangzhou': ((2560,3830),(113.26,23.13)),
 'Changsha':  ((2410,3175),(112.94,28.23)),
}
def d(a,b):
    (x1,y1),_=P[a];(x2,y2),_=P[b];return math.hypot(x2-x1,y2-y1)
def km(a,b):
    _,(lo1,la1)=P[a];_,(lo2,la2)=P[b]
    p=math.radians;dl=p(lo2-lo1);dp=p(la2-la1)
    h=math.sin(dp/2)**2+math.cos(p(la1))*math.cos(p(la2))*math.sin(dl/2)**2
    return 6371*2*math.asin(math.sqrt(h))
routes=[
 ('Luoyang–Chang\'an, by Hangu and Tong', ['Luoyang','Hangu','Tong','Changan'],'road'),
 ('Luoyang–Kaifeng', ['Luoyang','Kaifeng'],'road'),
 ('Luoyang–Song Shan (Shaolin)', ['Luoyang','Songshan'],'road'),
 ('Chang\'an–Hua Shan', ['Changan','Huashan'],'road'),
 ('Chang\'an–Chengdu, by Jianmen', ['Changan','Hanzhong','Jianmen','Chengdu'],'road'),
 ('Luoyang–Taiyuan, by Mengjin and a Taihang pass', ['Luoyang','Mengjin','Taiyuan'],'road'),
 ('Taiyuan–Yanjing, by Jingxing', ['Taiyuan','Jingxing','Yanjing'],'road'),
 ('Yanjing–Liaoyang, by Shanhai', ['Yanjing','Shanhai','Liaoyang'],'road'),
 ('Luoyang–Wuchang, by Nanyang', ['Luoyang','Nanyang','Xiangyang','Wuchang'],'road'),
 ('Xiangyang–Wudang', ['Xiangyang','Wudang'],'road'),
 ('Chongqing–Wuchang, by the Three Gorges', ['Chongqing','Kuimen','Yichang','Wuchang'],'river'),
 ('Wuchang–Nanjing, down the Yangtze', ['Wuchang','Nanjing'],'river'),
 ('Kaifeng–Yangzhou, by the Huai fords', ['Kaifeng','Huaiford','Yangzhou'],'road'),
 ('Nanjing–Suzhou–Hangzhou', ['Nanjing','Suzhou','Hangzhou'],'road'),
 ('Chang\'an–Tianshan, by Kongtong, Liangzhou and Jiayu', ['Changan','Kongtong','Liangzhou','Jiayu','Tianshan'],'road'),
 ('Chengdu–Dali', ['Chengdu','Dali'],'road'),
 ('Wuchang–Guangzhou, by Changsha and Meiling', ['Wuchang','Changsha','Meiling','Guangzhou'],'road'),
]
SPEED=16*30/256  # cells per second at 1x
for name,pts,kind in routes:
    L=sum(d(a,b) for a,b in zip(pts,pts[1:]))
    K=sum(km(a,b) for a,b in zip(pts,pts[1:]))
    print(f"{name:55s} cells={L:6.0f} realkm={K:6.0f} km/cell={K/L:5.2f} walk_open_min={L/SPEED/60:5.1f}")
print(SPEED, 3840/SPEED/60, 4096/SPEED/60)
print()
W=SPEED
for name,pts,kind in routes:
    L=round(sum(d(a,b) for a,b in zip(pts,pts[1:])),-1)
    K=round(sum(km(a,b) for a,b in zip(pts,pts[1:])),-1)
    foot=L/W/60; road=L/(W*1.25)/60; horse=L/(W*2.5)/60
    days=L/(W*1.25)/60  # a day is 60 s at 1x
    boat = f"{L/(W*2.5)/60:.1f} down, {L/(W*0.8)/60:.1f} up" if kind=='river' else '—'
    print(f"| {name} | {int(L):,} | {int(K):,} | {foot:.1f} | {road:.1f} | {horse:.1f} | {boat} |")
