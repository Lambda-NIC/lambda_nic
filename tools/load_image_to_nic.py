import subprocess
import sys
import numpy as np
import PIL
from PIL import Image
import timeit

IMAGE_SIZE = 256
NUM_VALS = 4
WORD_SIZE = 4
COLORS = 3
# Batch size in bytes
BATCH_SIZE = NUM_VALS * WORD_SIZE

def Conv2Dto1DAddr(x, y):
    return (x + y * IMAGE_SIZE) * WORD_SIZE

img_id = int(sys.argv[1])
image_path = "../sample_images/img%s.png" % img_id

tic = timeit.default_timer()

im = PIL.Image.open(image_path)
I = np.asarray(im)
#I = I.view(dtype=np.uint32).reshape(I.shape[:-1])

toc = timeit.default_timer()

print "%f taken" % (toc - tic)

printed = False
#IMAGES ARE IN y, x format ROW MAJOR INDEXED
for y in range(IMAGE_SIZE):
    for x in range(0, IMAGE_SIZE, NUM_VALS):
        addr = Conv2Dto1DAddr(x,y)
        cmd = "nfp-rtsym -vl %d _input_image:%d" % (BATCH_SIZE, addr)
        for i in range(NUM_VALS):
            # Get the RGBA value into
            val = (I[y][x+i][0] & 0xFF) + \
                  ((I[y][x+i][1] << 8) & 0xFF00) + \
                  ((I[y][x+i][2] << 16) & 0xFF0000)
            if not printed:
                avg = (I[y][x+i][0]/3 + I[y][x+i][1]/3 + I[y][x+i][2]/3)
                if avg < 200 and avg > 100:
                    print y, x+i,  I[y][x+i][0], I[y][x+i][1], I[y][x+i][2], avg
                    printed = True
            cmd += " %d" % val
        #print cmd
        subprocess.call(cmd.split())

cmd = "nfp-rtsym -vl 4 _image_input_ready:0 1"
subprocess.call(cmd.split())
