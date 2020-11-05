import argparse

args = argparse.ArgumentParser()
args.add_argument('-prefix')
args.add_argument('-i')
args.add_argument('-o')
args = args.parse_args()

fin = open(args.i, 'rb')
fout = open(args.o, 'wb')

for line in fin.readlines():
    if line.endswith(b'\r\n'):
        line = line[:-2] + b'\n'
    fout.write(bytes(args.prefix, 'utf8') + line)
