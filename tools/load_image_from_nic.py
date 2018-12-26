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

def Conv2Dto1D(x, y):
    return x + y * IMAGE_SIZE


tic = timeit.default_timer()
I = numpy.zeros((IMAGE_SIZE, IMAGE_SIZE))

for y in range(IMAGE_SIZE):
    for x in range(0,IMAGE_SIZE, BATCH_SIZE):
        addr = Conv2Dto1D(x,y)
        cmd = "nfp-rtsym -vl %d _output_image:%d" % (BATCH_SIZE, addr)
        print "calling %s" % cmd
        result = subprocess.check_output(cmd.split()).split()
        for i in range(BATCH_SIZE/WORD_SIZE):
            val = int(result[1 + i], 16)
            print "value %d at %s" % (val, addr)
            I[y][x + i*WORD_SIZE] = (val >> 24) & 0xFF
            I[y][x + 1 + i*WORD_SIZE] = (val >> 16) & 0xFF
            I[y][x + 2 + i*WORD_SIZE] = (val >> 8) & 0xFF
            I[y][x + 3 + i*WORD_SIZE] = val & 0xFF

toc = timeit.default_timer()

print "%f taken" % (toc - tic)

img = Image.fromarray(I, 'L')
img.save("test.jpg")
