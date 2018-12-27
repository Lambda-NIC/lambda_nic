import sys
import timeit
import numpy as np
import PIL
from PIL import Image

def get_stdin():
    buf = ""
    for line in sys.stdin:
        buf = buf + line
    buf = buf.strip()
    return buf

def transform_image(img_id):
    tic = timeit.default_timer()
    image_path = "./sample_images/img%s.png" % img_id
    im = PIL.Image.open(image_path)
    toc = timeit.default_timer()
    I = np.asarray(im)
    J = np.zeros((256,256))
    for y in range(256):
        for x in range(256):
            J[y][x] = I[y][x][0]/3 + I[y][x][1]/3 + I[y][x][2]/3

    J = J.astype(np.uint8)
    Image.fromarray(J, 'L')
    return toc - tic


if __name__ == "__main__":
    st = get_stdin()
    try:
        print transform_image(int(st))
    except Exception as error:
        print "Error: %s" % error
