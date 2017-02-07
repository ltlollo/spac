import os
import sys
import glob
import itertools


def test_join(n, m, split_list, forig):
    for r in range(n, m+1):
        for p in itertools.permutations(split_list, r):
            args = ' '.join(p)
            jret = os.system('./spac -j ' + forig + '.o ' + args)
            if jret != 0:
                print(args + ': join failed')
            jret = os.system('diff ' + forig + ' ' + forig + '.o')
            rret = os.system('rm ' + forig + '.o')
            if rret == 0 and jret != 0:
                print(args + ': system in unclean state')
            if rret != 0 and jret == 0:
                print(args + ': non removable file')
                sys.exit(1)


def test(n, m, forig):
    args = forig + ' ' + str(n) + ' ' + str(m)
    if os.system('./spac -s ' + args) != 0:
        print(args + ': split failed')
        sys.exit(1)
    split_list = glob.glob(forig + '.[0-9]*.spl')
    if len(split_list) == 0:
        print(args + ': silent fail')
        sys.exit(1)
    test_join(n, m, split_list, forig)
    rret = os.system('rm ' + ' '.join(split_list))
    if rret != 0:
        print(args + ': non removable files')
        sys.exit(1)


def cover(size, forig='t'):
    if size != 0:
        args = 'dd if=/dev/urandom of='+forig+' bs='+str(size)+' count=1'
    else:
        args = 'touch ' + forig
    if os.system(args) != 0:
        print(args + ': creating test file')
        sys.exit(1)
    for m in range(2, 256):
        for n in range(2, m+1):
            test(n, m, forig)
    if os.system('rm ' + forig) != 0:
        print('non removable test file')
        sys.exit(1)

if __name__ == '__main__':
    sizes = [0, 1, 2, 2000, 4095, 4096, 4097, 6000, 2*4096-1, 2*4096, 2*4096+1]
    for size in sizes:
        cover(size, '/tmp/t')
    sys.exit(0)
