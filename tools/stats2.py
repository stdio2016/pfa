import argparse
import re
from collections import Counter

args = argparse.ArgumentParser()
args.add_argument('log')
args = args.parse_args()

total_times = Counter()
with open(args.log, encoding='utf8') as fin:
    for line in fin:
        split = line.find(']: ')
        if split == -1:
            body = line
        else:
            body = line[split+3:]
        s = re.search(r'(.+) (\d+\.\d+)(ms|s)', body)
        if s:
            task = s[1]
            if task.endswith(':'):
                task = task[:-1]
            secs = float(s[2])
            unit = s[3]
            if unit == 'ms':
                secs /= 1000
            total_times[task] += secs
for task in total_times:
    print('%s %.3f s' % (task, total_times[task]))
