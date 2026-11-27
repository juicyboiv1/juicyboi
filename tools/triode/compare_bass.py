#!/usr/bin/env python3
"""Compact Rev2.4 vs Rev3 table from two juicyboi_bassprobe outputs."""
import re, sys
def parse(path):
    d = {}; cur = None
    for line in open(path):
        m = re.match(r'=== (\S+) ===', line)
        if m: cur = d.setdefault(m.group(1), {}); continue
        if cur is None: continue
        m = re.match(r'\s+(\d+)\s+(-?[\d.]+)\s+(-?[\d.]+)', line)
        if m: cur['f%s' % m.group(1)] = float(m.group(3)); continue
        m = re.search(r'lowE THD: (-?[\d.]+)', line);           cur.update(thd=float(m.group(1))) if m else None
        m = re.search(r'2-tone IMD.*?: (-?[\d.]+) dB', line);   cur.update(imd=float(m.group(1))) if m else None
        m = re.search(r'diff tone [\d.]+ Hz: (-?[\d.]+)', line); cur.update(diff=float(m.group(1))) if m else None
        m = re.search(r'gap-flub (-?[\d.]+)', line);            cur.update(flub=float(m.group(1))) if m else None
        m = re.search(r'LF\(<=200\) hit (-?[\d.]+)', line);      cur.update(hit=float(m.group(1))) if m else None
    return d
a, b = parse(sys.argv[1]), parse(sys.argv[2])
keys = [('f40','40Hz'),('f80','80Hz'),('f100','100Hz'),('thd','lowE THD'),('imd','2-tone IMD'),('diff','diff tone'),('hit','palm hit'),('flub','gap flub')]
print('%-11s' % 'config' + ''.join('%22s' % k[1] for k in keys))
for c in a:
    if c not in b: continue
    print('%-11s' % c + ''.join('%22s' % ('%6.1f -> %6.1f (%+.1f)' % (a[c][k], b[c][k], b[c][k]-a[c][k]) if k in a[c] and k in b[c] else '-') for k, _ in keys))
