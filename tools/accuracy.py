import re
import argparse

args = argparse.ArgumentParser()
args.add_argument('-result', default='dat/result.txt')
args = args.parse_args()

f1 = open('dat/groundTruth.txt', 'r')
A = f1.readlines()
f2 = open(args.i, 'r')
B = f2.readlines()

wavpat = re.compile(r'(\w{5})\.wav')
mp3pat = re.compile(r'(\w{5})\.mp3')

yay = 0
no = 0
for gt, res in zip(A, B):
    my = res.split('\t')
    my_q = wavpat.search(my[0])[1]
    my_ans = mp3pat.search(my[1])[1]
    if my_ans == '00128':
        my_ans = '10077'

    real = gt.split('\t')
    real_q = real[0]
    real_ans = real[1]
    if real_ans.endswith('\n'):
        real_ans = real_ans[:-1]
    if real_q != my_q:
        print('??')
    if real_ans == my_ans:
        yay += 1
    else:
        no += 1
print("yay %d no %d acc %f" % (yay, no, yay/(yay+no)))
