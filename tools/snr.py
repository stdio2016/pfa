import argparse
import miniaudio
from miniaudio import SampleFormat
import numpy as np
from scipy import signal

def my_box_conv(x, k):
    n = x.shape[0]
    c = np.cumsum(x)
    d = c[k:] - c[:n-k]
    return np.concatenate([[c[k-1]], d])

def signal_noise_ratio(audio, query, smpRate, minOverlap):
    xn = audio.shape[0]
    yn = query.shape[0]
    padded = np.zeros(yn-1 + xn + yn-1)
    padded[yn-1:yn+xn-1] = audio
    # find best a and shift
    # (ax - y) * (ax - y) = a^2 x^2 - 2a xy + y^2
    # a x^2 - 2 xy + y^2 / a
    xy = signal.fftconvolve(audio, np.flip(query), 'full')
    x2 = my_box_conv(padded**2, yn)
    y2 = np.sum(query**2)
    a = xy / (x2 + 1e-9)
    err = (a**2 * x2) / (a**2 * x2 - 2 * a * xy + y2)
    overlap = int(minOverlap * smpRate)
    pos = np.argmax(err[overlap:xn+yn-1-overlap]) + overlap
    
    part = padded[pos:pos+yn]
    amp = np.sum(query*part) / np.sum(part**2)
    diff = query - part * amp
    noise = np.average(diff**2)
    sig = (amp**2) * (np.sum(part**2) / yn)
    snr = 10 * np.log10(sig / noise)
    shift = pos - (yn-1)
    #print('shift is', off / smpRate)
    #print('signal multiply is', a)
    #print('signal rms is', np.sqrt(sig))
    #print('query rms is', np.sqrt(np.average(query**2)))
    #print('noise rms is', np.sqrt(noise))
    return shift/smpRate, snr, amp

def main(args):
    with open(args.audio, 'rb') as fin:
        code = fin.read()
    audio = miniaudio.decode(code, nchannels=1, output_format=SampleFormat.FLOAT32)
    with open(args.query, 'rb') as fin:
        code = fin.read()
    query = miniaudio.decode(code, nchannels=1, sample_rate=audio.sample_rate, output_format=SampleFormat.FLOAT32)
    x = np.asarray(audio.samples)
    y = np.asarray(query.samples)
    shift, snr, amp = signal_noise_ratio(x, y, audio.sample_rate, 0.9)
    print('shift time %.3fs SNR=%.3fdB amp=%.6f' % (shift, snr, amp))

if __name__ == '__main__':
    args = argparse.ArgumentParser()
    args.add_argument('audio')
    args.add_argument('query')
    args = args.parse_args()
    main(args)
