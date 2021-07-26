import argparse
from pathlib import Path

import miniaudio
import numpy as np
import numpy.lib.recfunctions
import matplotlib
#matplotlib.use('Agg')
import matplotlib.pyplot as plt
import torch
import torchaudio

def miniaudio_get_audio(filename):
    code = Path(filename).read_bytes()
    audio = miniaudio.decode(code, output_format=miniaudio.SampleFormat.FLOAT32)
    samples = np.array(audio.samples).reshape([-1, audio.nchannels]).T
    return samples, audio.sample_rate

args = argparse.ArgumentParser()
args.add_argument('sound')
args.add_argument('landmark')
args.add_argument('--old', action='store_true')
args = args.parse_args()

smp, smprate = miniaudio_get_audio(args.sound)
smp = torch.FloatTensor(smp)
if smprate == 44100:
    smp = smp[:,::5]
else:
    smp = torchaudio.transforms.Resample(8820, smprate)(smp)
smp = smp.numpy()
smp = smp.mean(axis=0) * 32768

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
mask = peaks[:,0]*512/8820 < short
peaks = peaks[mask]
smp = smp[:int(short*8820)]
n_time = smp.shape[0]//512

def same_plt_setting():
    plt.xlabel('Time')
    plt.ylabel('Frequency')
    plt.xlim([0, n_time])
    plt.ylim([0, 512])

plt.figure(1)
plt.specgram(smp, Fs=1024, NFFT=1024, noverlap=512, xextent=(0, n_time), cmap='jet')
same_plt_setting()
plt.savefig('lmfig1.eps',format='eps')

plt.figure(2)
plt.specgram(smp, Fs=1024, NFFT=1024, noverlap=512, xextent=(0, n_time), cmap='jet')
plt.scatter(peaks[:,0]+2, peaks[:,1], color='k')
same_plt_setting()
plt.savefig('lmfig2.eps',format='eps')

plt.figure(3)
some = peaks[32]
print(some)
sx = some[0]+2
sy = some[1]
paired = lms[(lms['t1'] == some[0]) & (lms['f1'] == some[1])]

linkx = [sx]
linky = [sy]
#for x,y in peaks[(peaks[:,0]+2 >= sx+6) & (peaks[:,0]+2 <= sx+35) & (peaks[:,1] >= sy-127) & (peaks[:,1] <= sy+127)]:
for x,y in paired[['t2','f2']]:
    linkx.append((x+2))
    linkx.append(sx)
    linky.append(y)
    linky.append(sy)

plt.plot(linkx, linky, color='b')
plt.plot([sx+6, sx+6, sx+35, sx+35, sx+6], [sy-127, sy+127, sy+127, sy-127, sy-127], 'k')
peaksx = set((x,y) for x,y in peaks.tolist())
peaksx -= {(some[0],some[1])}
peaksx -= set((x,y) for x,y in paired[['t2','f2']].tolist())
peaksx = np.array(list(peaksx))
plt.scatter(peaksx[:,0]+2, peaksx[:,1], color='k')
plt.scatter([some[0]+2], [some[1]], color='r', s=100)
plt.scatter(paired['t2']+2, paired['f2'], color='b')

plt.plot([])
same_plt_setting()

plt.savefig('lmfig3.eps',format='eps')
plt.show()
