import argparse
from pathlib import Path

import numpy as np
import matplotlib
import matplotlib.pyplot as plt

args = argparse.ArgumentParser()
args.add_argument('songlm')
args.add_argument('querylm')
args.add_argument('--old', action='store_true')
args.add_argument('--ver', default='')
args = args.parse_args()

lm_dtype = [
    ('t1','<i4'),('f1','<i4'),('e1','<f4'),
    ('t2','<i4'),('f2','<i4'),('e2','<f4')
]
if args.old:
    lm_dtype = [('t1','<i4'),('f1','<i4'),('t2','<i4'),('f2','<i4')]

songlm = np.fromfile(args.songlm, dtype=lm_dtype)
querylm = np.fromfile(args.querylm, dtype=lm_dtype)

def get_lm_hash(t1, f1, t2, f2):
    return f1<<15 | (f2-f1 & 511)<<6 | (t2-t1)

# [t1, f1, t2, f2]
db = {}
for t1,f1,t2,f2 in songlm[['t1','f1','t2','f2']]:
    lm_key = get_lm_hash(t1, f1, t2, f2)
    if lm_key not in db:
        db[lm_key] = []
    db[lm_key].append(t1)

s_x = []
s_y = []
offsets = []

for t1,f1,t2,f2 in querylm[['t1','f1','t2','f2']]:
    lm_key = get_lm_hash(t1, f1, t2, f2)
    if lm_key in db:
        for t_song in db[lm_key]:
            s_x.append(t_song)
            s_y.append(t1)
            offsets.append(t_song - t1)

fig = plt.figure(1, figsize=(8, 3))
plt.axes([0.1, 0.16, 0.85, 0.75])
plt.title('(a) scatterplot of time features')
plt.xlabel('database music time')
plt.ylabel('query audio time')
plt.scatter(s_x, s_y)
#plt.figtext(0, 0, '(a)')
plt.savefig('lm_scatter'+args.ver+'.pdf', transparent=True)

fig = plt.figure(2, figsize=(8, 3))
fig.add_axes([0.1, 0.16, 0.85, 0.75])
plt.title('(b) histogram of time offset')
plt.xlabel('time offset $t_{song}-t_{query}$')
plt.ylabel('number of landmarks')
plt.hist(offsets, bins=(range(0, max(s_x), 5)))
#plt.figtext(0, 0, '(b)')
plt.savefig('lm_hist'+args.ver+'.pdf', transparent=True)
plt.show()
