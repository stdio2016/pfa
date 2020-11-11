import argparse
import re

args = argparse.ArgumentParser()
args.add_argument('-name')
args.add_argument('-t')
args = args.parse_args()

times = {}
lms = 0
durs = 0.0
for i in range(int(args.t)):
    filename = "%s%d.log" % (args.name, i)
    fin = open(filename, "r")
    for line in fin:
        if line.startswith('File: '):
            pass
        else:
            s = re.search(r'(.+) (\d+\.\d+)ms', line)
            if s:
                part = s[1]
                ms = float(s[2])
                if part in times:
                    times[part] += ms
                else:
                    times[part] = ms
            s = re.search(r'landmarks=(\d+)', line)
            if s:
                lms += int(s[1])
            s = re.search(r'duration=(\d+\.?\d*)s', line)
            if s:
                durs += float(s[1])
    fin.close()
for key in times:
    print("%s %.3fms" % (key, times[key]))
print('landmarks=%d' % lms)
print('duration=%fs' % durs)
