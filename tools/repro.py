import soundfile as sf
import numpy as np
import matplotlib.mlab as mlab
import scipy.signal as signal
wavfile = '../shapeclear-html/assets/sound/music.wav'

# need param
wsize = 1024
wratio = 0.5
samples, sr = sf.read(wavfile)

# stereo to mono
if samples.ndim == 2:
    if samples.shape[1] == 2:
        pow1 = np.linalg.norm(samples[:,0] - samples[:,1])**2
        pow2 = np.linalg.norm(samples[:,0] + samples[:,1])**2
        if pow1 > pow2 * 1000:
            print('fake stereo with opposite phase detected')
            samples[:,1] *= -1
    samples = np.mean(samples, axis=1)

# linear resample
pfa_sr = 8000
t_pnt = np.arange(int(samples.shape[0] * pfa_sr / sr), dtype=np.float64) * sr / pfa_sr
samples = np.interp(t_pnt, np.arange(samples.shape[0]), samples)

arr2D = mlab.specgram( samples, NFFT=wsize, Fs=pfa_sr, window=signal.windows.hann(wsize, sym=False), noverlap=int(wsize * wratio), scale_by_freq=False)[0]
# my power density is unscaled
arr2D *= (wsize/2)**2
arr2D[1:-1] *= 0.5
arr2D = 10 * np.log10(arr2D + 1e-10, out=np.zeros_like(arr2D))
# rescale here
arr2D -= 20 * np.log10(wsize/2)

# load my cpp result for comparison
spec_cpp = np.fromfile('test').reshape([-1, 513]).T
err = np.abs(spec_cpp - arr2D) 
maxerr = np.max(err)
maxerr_at = np.unravel_index(np.argmax(err), arr2D.shape)
print('max dB (Python side):', np.max(arr2D))
print('min dB (Python side):', np.min(arr2D))
print('max dB (C++ side):', np.max(spec_cpp))
print('min dB (C++ side):', np.min(spec_cpp))
print('max dB error:', maxerr)
print('dB at max error (Python side):', arr2D[maxerr_at])
print('dB at max error (C++ side):', spec_cpp[maxerr_at])
errs = np.where(err > 0.1)
if len(errs[0]) > 0:
    print('max dB when error > 0.1dB:', max(arr2D[errs]), max(spec_cpp[errs]))
print('average dB error:', np.mean(err))
