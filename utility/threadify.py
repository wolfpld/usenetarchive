import subprocess
import sys

archive = sys.argv[1]

print( ' ##### Processing %s #####' % archive )

cnt1 = 1
while True:
    cnt2 = 1
    while True:
        print('  ==> Iteration %d.%d' % (cnt1, cnt2))
        cnt2 += 1
        res = subprocess.call(['./threadify', archive])
        if res == 0:
            break
        subprocess.call(['./lexsort', archive])

    res = subprocess.call(['./threadify', archive, '-g'])
    if res == 0:
        break
    subprocess.call(['./lexsort', archive])
    cnt1 += 1
