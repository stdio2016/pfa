import argparse
from pathlib import Path

import miniaudio
import numpy as np
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
args = args.parse_args()

smp, smprate = miniaudio_get_audio(args.sound)
smp = torch.FloatTensor(smp)
#smp = torchaudio.compliance.kaldi.resample_waveform(smp, smprate, 8820)
smp = smp[:,::5]
smp = smp.numpy()
smp = smp.mean(axis=0) * 32768

lms = np.fromfile(args.landmark, dtype=np.int32).reshape([-1,4])
# lms: [t1, f1, t2, f2]
peaks_t = lms[:,0::2].flatten()
peaks_f = lms[:,1::2].flatten()
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

plt.figure(2)
plt.specgram(smp, Fs=1024, NFFT=1024, noverlap=512, xextent=(0, n_time), cmap='jet')
plt.scatter(peaks[:,0]+2, peaks[:,1], color='K')
same_plt_setting()

plt.figure(3)
some = peaks[21]
print(some)
sx = some[0]+2
sy = some[1]
paired = lms[(lms[:,0] == some[0]) & (lms[:,1] == some[1])]
plt.plot([sx+6, sx+6, sx+35, sx+35, sx+6], [sy-127, sy+127, sy+127, sy-127, sy-127], 'K')
plt.scatter(peaks[:,0]+2, peaks[:,1], color='K')
plt.scatter([some[0]+2], [some[1]], color='R')
plt.scatter(paired[:,2]+2, paired[:,3], color='B')
same_plt_setting()

plt.show()
