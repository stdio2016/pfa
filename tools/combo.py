import re
import argparse

args = argparse.ArgumentParser()
args.add_argument('-result', default='dat/result.txt')
args.add_argument('--input')
args.add_argument('--input2')
args = args.parse_args()

f1 = open('dat/groundTruth.txt', 'r')
A = f1.readlines()
f2 = open(args.input, 'r')
B = f2.readlines()
f3 = open(args.input2, 'r')
C = f3.readlines()

wavpat = re.compile(r'(\w{5})\.wav')
mp3pat = re.compile(r'(\w{5})\.mp3')

y1 = 0
y2 = 0
y12 = 0
no = 0
for gt, res, res2 in zip(A, B, C):
    my = res.split('\t')
    my_q = wavpat.search(my[0])[1]
    my_ans = mp3pat.search(my[1])[1]
    if my_ans == '00128':
        my_ans = '10077'

    my2 = res2.split('\t')
    my2_q = wavpat.search(my2[0])[1]
    my2_ans = mp3pat.search(my2[1])[1]
    if my2_ans == '00128':
        my2_ans = '10077'

    real = gt.split('\t')
    real_q = real[0]
    real_ans = real[1]
    if real_ans.endswith('\n'):
        real_ans = real_ans[:-1]
    if real_q != my_q:
        print('??')
    if real_ans == my_ans:
        if real_ans == my2_ans:
            y12 += 1
        else:
            y1 += 1
    else:
        if real_ans == my2_ans:
            y2 += 1
        else:
            no += 1
print("y12 %d y1 %d y2 %d no %d" % (y12, y1, y2, no))
