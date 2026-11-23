import re, csv, matplotlib; matplotlib.use('Agg'); import matplotlib.pyplot as plt
def bass(p):
    d={};cur=None
    for l in open(p):
        m=re.match(r'=== (\S+) ===',l)
        if m: cur=m.group(1); d[cur]=[]; continue
        m=re.match(r'\s+(\d+)\s+(-?[\d.]+)\s+(-?[\d.]+)',l)
        if cur and m: d[cur].append((int(m.group(1)),float(m.group(3))))
    return d
def harm(p):
    d={}
    for l in open(p):
        f=l.split()
        if len(f)>12 and f[1].isdigit(): d[(f[0],int(f[1]),f[2])]=[float(x) for x in f[6:12]]
    return d
def chord(p): return [(float(a),float(b)) for a,b in csv.reader(open(p))]
A,B=bass('r24/bass.txt'),bass('r3/bass.txt'); HA,HB=harm('r24/harm.txt'),harm('r3/harm.txt')
fig,ax=plt.subplots(2,2,figsize=(13,9))
for i,c in enumerate(['RedV2-B5','Lead-B5']):
    a=ax[0][i]
    for d,lab,st in ((A,'Rev2.4','--'),(B,'Rev3','-')):
        f,v=zip(*d[c]); a.semilogx(f,v,st,marker='o',ms=3,label=lab)
    a.set_title(f'{c}: driven response (0.1 FS sine, Cab off)'); a.set_xlabel('Hz'); a.set_ylabel('dBFS'); a.grid(True,which='both',alpha=.3); a.legend()
a=ax[1][0]; hn=['H2','H3','H4','H5','H7','H9']; import numpy as np; x=np.arange(len(hn)); w=0.2
for j,(c,lab) in enumerate([('RedV2','Red v2'),('Lead','5150 Lead')]):
    k=(c,7,'0.25'); a.bar(x+(2*j-1.5)*w,HA[k],w,label=f'{lab} Rev2.4',alpha=.55); a.bar(x+(2*j-0.5)*w,HB[k],w,label=f'{lab} Rev3')
a.set_xticks(x); a.set_xticklabels(hn); a.set_ylabel('dBc'); a.set_title('Harmonics, 110 Hz @ 0.25 FS, Gain 7'); a.legend(fontsize=8); a.grid(True,axis='y',alpha=.3); a.set_ylim(-45,-5); a.invert_yaxis()
a=ax[1][1]
for c,col in (('RedV2','C0'),('Lead','C3')):
    for v,lab,st in (('r24','Rev2.4','--'),('r3','Rev3','-')):
        t,y=zip(*chord(f'{v}/d/chord-{c}.csv')); a.plot([1000*q for q in t],y,st,color=col,label=f'{c} {lab}')
a.set_title('Quiet note after a 300 ms power chord (bias-shift recovery)'); a.set_xlabel('ms after chord'); a.set_ylabel('dB re steady'); a.grid(True,alpha=.3); a.legend(fontsize=8); a.set_xlim(0,300)
fig.suptitle('juicyboi Rev3 (solved triodes) vs Rev2.4'); fig.tight_layout(); fig.savefig('../rev3-vs-rev2.4.png',dpi=110)
