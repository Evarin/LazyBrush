# coding: utf-8
from PIL import Image, ImageFilter
from scipy import ndimage
import numpy as np
import math
from lazybrush import wrapper

def logkernel(sigma):
    s = 15
    kernel = []
    s2 = 2 * math.pow(sigma, 2)
    ct = -1 / (math.pi * s2)
    for i in range(s):
        x = i - (s-1)/2
        ck = []
        for j in range(s):
            y = j - (s-1)/2
            cf = (x*x + y*y) / s2;
            v = ct * (1 - cf) * math.exp(-cf)
            ck.append(v)
        kernel.append(ck)
    return kernel
    
def lazybrush(sketch, colors, sigma, l, kscale, useLoG):
    sketch_f = np.asarray(sketch, dtype=np.double)
    if useLoG:
        # LoG filter
        kernel = logkernel(sigma)
        sketch_f = ndimage.convolve(sketch_f, kernel)
        sketch_f = 1 - np.maximum(0, sketch_f / np.max(np.max(sketch_f)))
    else:
        sketch_f = (sketch_f / np.max(np.max(sketch_f))) ** 2.0
    print(np.min(np.min(sketch_f)))
    (wdt, hgt) = sketch_f.shape
    # Drop a dimension
    sketch_f = sketch_f.reshape(wdt, hgt)
    # Colors
    colors_f = np.asarray(colors, dtype=np.uint8).view(dtype=np.uint32).reshape(wdt, hgt)
    list_colors, colors_i = np.unique(colors_f, return_inverse=True)
    print list_colors
    output = colors_i.copy()
    # Core algorithm in c
    wrapper(sketch_f.T, colors_i, list_colors, kscale*2*(wdt+hgt), l, output)
    output = np.append(output,[0]) # Fix a little bug ?
    output = list_colors[output]
    osketch = (sketch_f*255).reshape(wdt, hgt).astype(np.uint8)
    return (osketch, output.reshape(wdt, hgt))
