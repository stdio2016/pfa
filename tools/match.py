import numpy as np
import argparse
import wave
from matplotlib import pyplot as plt
from matplotlib import collections  as mc

args = argparse.ArgumentParser()
args.add_argument('-song', required=True)
args.add_argument('-query', required=True)
args.add_argument('-wav')
args.add_argument('-offset')
args = args.parse_args()

FFT_SIZE = 1024
NOVERLAP = int(FFT_SIZE * 0.5)
SAMPLE_RATE = 8000

song = np.fromfile(args.song, np.int32)
song = song.reshape((-1,6))
print('landmarks in song:', song.shape[0])
query = np.fromfile(args.query, np.int32)
query = query.reshape((-1,6))
print('landmarks in query:', query.shape[0])
query_dur = np.max(query[:,[0,3]])

# build a database
db = {}
song_peaks = set()
for i in range(song.shape[0]):
    t1, f1, _, t2, f2, _ = song[i]
    song_peaks.add((int(t1), int(f1)))
    song_peaks.add((int(t2), int(f2)))
    key = int((f1<<15) + (((f2-f1)&0xff)<<7) + ((t2-t1)&0x7f))
    value = t1
    if key in db:
        db[key].append(int(value))
    else:
        db[key] = [int(value)]
print('distinct hashes in song:', len(db))
print('peaks in song:', len(song_peaks))
print(np.bincount(song[:,3] - song[:,0]))

# query landmarks in db
meet = []
query_peaks = set()
for i in range(query.shape[0]):
    t1, f1, _, t2, f2, _ = query[i]
    query_peaks.add((int(t1), int(f1)))
    query_peaks.add((int(t2), int(f2)))
    key = int((f1<<15) + (((f2-f1)&0xff)<<7) + ((t2-t1)&0x7f))
    value = t1
    if key in db:
        for t_song in db[key]:
            meet.append(t_song - t1)
print('peaks in query:', len(query_peaks))
print('hash hits:', len(meet))

meet.sort()
prev = None
# score is the number of matched landmarks
max_score = 0
# the time offset to get max score
best_offset = None
cur_score = 0
for t_meet in meet:
    if prev == t_meet:
        cur_score += 1
    else:
        cur_score = 1
    if cur_score > max_score:
        max_score = cur_score
        best_offset = t_meet
    prev = t_meet
print('score:', max_score, '(%.2f%%)' % (max_score / query.shape[0] * 100))

if max_score > 0:
    sec = best_offset * (FFT_SIZE - NOVERLAP) / SAMPLE_RATE
    print('match position:', best_offset, '(%.2fs)' % sec)

    if args.offset:
        best_offset = int(args.offset)
    match_peaks = set()
    match_lms = set()
    for i in range(query.shape[0]):
        t1, f1, _, t2, f2, _ = query[i].tolist()
        key = (f1<<15) + (((f2-f1)&0xff)<<7) + ((t2-t1)&0x7f)
        value = t1
        if key in db:
            for t_song in db[key]:
                if t_song - t1 == best_offset:
                    match_peaks.add((int(t1), int(f1)))
                    match_peaks.add((int(t2), int(f2)))
                    match_lms.add((t1, f1, t2, f2))
    print('matched peaks:', len(match_peaks), '(%.2f%%)' % (len(match_peaks) / len(query_peaks) * 100))

    ax = plt.axes(label='123')

    # show spectrogram
    frame_per_sec = 1 / ((FFT_SIZE - NOVERLAP) / SAMPLE_RATE)
    if args.wav:
        w = wave.open(args.wav)
        n = w.getnframes()
        fs = w.getframerate()
        channel = w.getnchannels()
        buf = w.readframes(n)

        dat = np.frombuffer(buf, np.int16).reshape([n, channel])
        dat = dat.astype(np.float) / 32768
        dat = dat.mean(axis=1)
        t_pnt = np.arange(int(dat.shape[0] * SAMPLE_RATE / fs), dtype=np.float64) * fs / SAMPLE_RATE
        dat = np.interp(t_pnt, np.arange(dat.shape[0]), dat)
        n = dat.shape[0]
        framen = (n-NOVERLAP) // (FFT_SIZE-NOVERLAP)
        plt.specgram(dat, Fs=FFT_SIZE+2, NFFT=FFT_SIZE, noverlap=NOVERLAP, xextent=(-0.5,framen-0.5))

    # analyze peaks
    qlen = 10 * frame_per_sec
    # time shift song peaks
    song_peaks = set((t-best_offset, f) for (t,f) in song_peaks if 0 <= t-best_offset <= max(query_dur,qlen))
    intersect_peaks = song_peaks & query_peaks
    song_peaks -= intersect_peaks
    query_peaks -= intersect_peaks
    intersect_peaks -= match_peaks

    song_peaks = np.array(list(song_peaks), ndmin=2).reshape([-1, 2])
    query_peaks = np.array(list(query_peaks), ndmin=2).reshape([-1, 2])
    match_peaks = np.array(list(match_peaks), ndmin=2).reshape([-1, 2])
    intersect_peaks = np.array(list(intersect_peaks), ndmin=2).reshape([-1, 2])
    print('blue peaks:', len(intersect_peaks))
    print('green peaks:', len(match_peaks))
    
    lines = [((t1,f1), (t2,f2)) for t1,f1,t2,f2 in match_lms]
    colors = np.array(list(match_lms), dtype=np.float64)
    colors = np.sin(colors[:,0:3]) * 0.5 + 0.5
    colors[:, 2] = 0
    line_segs = mc.LineCollection(lines, linewidths=2, colors=colors)
    ax.add_collection(line_segs)

    plt.scatter(song_peaks[:,0], song_peaks[:,1], color='red')
    plt.scatter(query_peaks[:,0], query_peaks[:,1], color='yellow')
    if match_peaks.size > 0:
        # match_peaks: peaks of matched landmarks
        plt.scatter(match_peaks[:,0], match_peaks[:,1], color='green')
    if intersect_peaks.size > 0:
        # intersect_peaks: intersection of song peaks and query peaks that is not in match_peaks
        plt.scatter(intersect_peaks[:,0], intersect_peaks[:,1], color='blue')
    plt.show()
