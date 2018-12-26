import subprocess
import sys
import numpy
import PIL
from PIL import Image
import timeit

IMAGE_SIZE = 256
WORD_SIZE = 4
COLORS = 3
BATCH_SIZE = 16

def Conv2Dto1DAddr(x, y):
    return (x + y * IMAGE_SIZE) * WORD_SIZE


tic = timeit.default_timer()
I = numpy.zeros((IMAGE_SIZE, IMAGE_SIZE, COLORS))

for y in range(IMAGE_SIZE):
    for x in range(0, IMAGE_SIZE, BATCH_SIZE/WORD_SIZE):
        addr = Conv2Dto1DAddr(x,y)
        cmd = "nfp-rtsym -vl %d _input_image:%d" % (BATCH_SIZE, addr)
        print "calling %s" % cmd
        result = subprocess.check_output(cmd.split()).split()
        for i in range(BATCH_SIZE/WORD_SIZE):
            val = int(result[1 + i], 16)
            print "value %d at %s" % (val, addr)
            I[y][x + i][0] = (val) & 0xFF
            I[y][x + i][1] = (val >> 8) & 0xFF
            I[y][x + i][2] = (val >> 16) & 0xFF

I = I.astype(numpy.uint8)
toc = timeit.default_timer()

print "%f taken" % (toc - tic)

img = Image.fromarray(I, 'RGB')
img.save("test.png")
