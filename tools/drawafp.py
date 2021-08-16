import argparse
from pathlib import Path

from pydub import AudioSegment
import numpy as np
import numpy.lib.recfunctions
import matplotlib
#matplotlib.use('Agg')
import matplotlib.pyplot as plt

sample_rate = 8000

def get_audio(filename):
    audio = AudioSegment.from_file(filename)
    samples = audio.get_array_of_samples()
    samples = np.frombuffer(samples, dtype=np.int16).reshape([-1, audio.channels])
    return samples, audio.frame_rate

args = argparse.ArgumentParser()
args.add_argument('sound')
args.add_argument('landmark')
args.add_argument('--old', action='store_true')
args = args.parse_args()

smp, smprate = get_audio(args.sound)
smp = smp[1152:] # 1152 is a magic number to align FFmpeg and minimp3
smp = smp.mean(axis=1) * 32768
# resample
t_pnt = np.arange(int(smp.shape[0] * sample_rate / smprate), dtype=np.float64) * smprate / sample_rate
smp = np.interp(t_pnt, np.arange(smp.shape[0]), smp)

lm_dtype = [
    ('t1','<i4'),('f1','<i4'),('e1','<f4'),
    ('t2','<i4'),('f2','<i4'),('e2','<f4')
]
if args.old:
    lm_dtype = [('t1','<i4'),('f1','<i4'),('t2','<i4'),('f2','<i4')]
lms = np.fromfile(args.landmark, dtype=lm_dtype)
# lms: [t1, f1, t2, f2]
peaks_t = np.lib.recfunctions.structured_to_unstructured(lms[['t1','t2']]).flatten()
peaks_f = np.lib.recfunctions.structured_to_unstructured(lms[['f1','f2']]).flatten()
print(peaks_t.shape)
peaks = np.unique(np.stack([peaks_t, peaks_f], axis=1), axis=0)
short = 5
mask = peaks[:,0]*512/sample_rate < short
peaks = peaks[mask]
smp = smp[:int(short*sample_rate)]
n_time = (smp.shape[0]-512)//512

def same_plt_setting():
    plt.xlabel('Time')
    plt.ylabel('Frequency')
    plt.xlim([0, n_time])
    plt.ylim([0, 513])

plt.figure(1)
plt.specgram(smp, Fs=1026, NFFT=1024, noverlap=512, xextent=(0, n_time), cmap='jet')
same_plt_setting()
plt.savefig('lmfig1.png',format='png')

plt.figure(2)
plt.specgram(smp, Fs=1026, NFFT=1024, noverlap=512, xextent=(0, n_time), cmap='jet')
plt.scatter(peaks[:,0]+0.5, peaks[:,1]+0.5, color='k')
same_plt_setting()
plt.savefig('lmfig2.png',format='png')

plt.figure(3)
some = peaks[32]
print(some)
sx = some[0]
sy = some[1]
paired = lms[(lms['t1'] == some[0]) & (lms['f1'] == some[1])]

linkx = [sx]
linky = [sy]
#for x,y in peaks[(peaks[:,0]+2 >= sx+6) & (peaks[:,0]+2 <= sx+35) & (peaks[:,1] >= sy-127) & (peaks[:,1] <= sy+127)]:
for x,y in paired[['t2','f2']]:
    linkx.append(x+0.5)
    linkx.append(sx+0.5)
    linky.append(y+0.5)
    linky.append(sy+0.5)

plt.plot(linkx, linky, color='b')
plt.plot([sx+6, sx+6, sx+35, sx+35, sx+6], [sy-127, sy+127, sy+127, sy-127, sy-127], 'k')
peaksx = set((x,y) for x,y in peaks.tolist())
peaksx -= {(some[0],some[1])}
peaksx -= set((x,y) for x,y in paired[['t2','f2']].tolist())
peaksx = np.array(list(peaksx))
plt.scatter(peaksx[:,0]+0.5, peaksx[:,1]+0.5, color='k')
plt.scatter([some[0]+0.5], [some[1]+0.5], color='r', s=100)
plt.scatter(paired['t2']+0.5, paired['f2']+0.5, color='b')

plt.plot([])
same_plt_setting()

plt.savefig('lmfig3.png',format='png')
plt.show()
