import argparse
import miniaudio
from miniaudio import SampleFormat
import numpy as np
from scipy import signal
import soundfile
#import matplotlib.pyplot as plt

def my_box_conv(x, k):
    n = x.shape[0]
    c = np.cumsum(x)
    d = c[k:] - c[:n-k]
    return np.concatenate([[c[k-1]], d])

def estimate_conv(x, y, kern=100, mode='iter'):
    y = y[kern:-kern]
    k = kern*2+1
    n = len(x)
    if mode == 'iter':
        h = np.zeros(k)
        lr = 1/len(x)
        prev_loss = None
        for step in range(50):
            noise = signal.convolve(x, h, 'valid') - y
            loss = np.average(noise**2)
            grad = signal.convolve(np.flip(x), noise, 'valid')
            if prev_loss is not None and loss > prev_loss:
                lr *= 0.5
            h -= grad * lr
            print(loss)
            prev_loss = loss
    else:
        X = np.zeros([k, k])
        for i in range(k):
            X[0, i] = x[:n-k+1].dot(x[i:n-k+1+i])
        for j in range(1, k):
            X[j, 0] = X[0, j]
            X[j, 1:] = X[j-1, :-1] - x[j-1] * x[0:k-1] + x[n-k+j] * x[n-k+1:]
        X = X + np.eye(k) * np.sum(x**2) * 0.001
        Y = np.zeros(k)
        for i in range(k):
            Y[i] = y.dot(x[i:n-k+1+i])
        h = np.linalg.solve(X, Y)
        h = np.flip(h)
    sig = signal.convolve(x, h, 'valid')
    noise = sig - y
    print('signal=%f %f' % (np.sqrt(np.average(sig**2)), np.sqrt(np.average(x**2))))
    print('noise=%f' % np.sqrt(np.average(noise**2)))
    print('SNR2=%.3fdB' % (10 * np.log10(np.sum(sig**2) / np.sum(noise**2))))
    print('convolution=%.3fdB' % (10 * np.log10(np.sum(h**2))))
    return h

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
    #h1 = estimate_conv(part, query, 100, 'iter')
    h2 = estimate_conv(part, query, 100, 'direct')
    #plt.plot(h1)
    #plt.plot(h2)
    #plt.legend(['iter', 'direct'])
    #plt.show()
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
    qhat = signal.fftconvolve(audio, h2, 'full')[100:-100]
    padded[yn-1:yn+xn-1] = qhat
    part = padded[pos:pos+yn]
    diff = query - part
    soundfile.write('tst.wav', qhat, smpRate)
    soundfile.write('diff.wav', diff, smpRate)
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
    shift, snr, amp = signal_noise_ratio(x, y, audio.sample_rate, 5)
    print('shift time %.3fs SNR=%.3fdB amp=%.6f' % (shift, snr, amp))

if __name__ == '__main__':
    args = argparse.ArgumentParser()
    args.add_argument('audio')
    args.add_argument('query')
    args = args.parse_args()
    main(args)
