import os
import math
import time
import argparse
from PIL import Image

# configuration

working_dir = os.getcwd()

src_dir = working_dir + os.sep + 'src'
dst_dir = working_dir + os.sep + 'dst'

output_dir = dst_dir

# arguments

parser = argparse.ArgumentParser(description="librif image encoder")

parser.add_argument('-i', "--input", help="input file, optional")
parser.add_argument("-v", "--verbose", help="enable verbose mode", action="store_true")
parser.add_argument("-c", "--compress", help="compress the image", action="store_true")
parser.add_argument("-pmin", "--pattern-min", type=int, help="minimum pattern size", default=8)
parser.add_argument("-pmax", "--pattern-max", type=int, help="maximum pattern size", default=8)
parser.add_argument("-pstep", "--pattern-step", type=int, help="step used to find the pattern", default=2)
parser.add_argument("--png", help="save grayscale png output", action="store_true")

args = parser.parse_args()

input_file = None
if not args.input == None:
    input_file = args.input

verbose = args.verbose
compressed = args.compress

min_pattern_size = args.pattern_min
max_pattern_size = args.pattern_max
pattern_step = args.pattern_step

png_output = args.png

def console_print(str):
    if verbose:
        print(str)

def get_pattern(im_pixels, start_x, start_y, w, h, s):

    pixels = []
    hash = 0

    color_count = 0

    for j in range(0, s):
        y = start_y + j

        for z in range(0, s):
            x = start_x + z

            pixel = (0, 255)

            if y < h and x < w:
                pixel = im_pixels[y][x]

            color, alpha = pixel

            if not (alpha == 0):
                color_count += 1

            hash += (color + alpha)
            pixels.append(pixel)

    return (hash, pixels, color_count)

def find_pattern(pattern, patterns):

    hash = pattern[0]
    pixels = pattern[1]

    for i in range(0, len(patterns)):
        pattern_1 = patterns[i]
        hash_1 = pattern_1[0]
        pixels_1 = pattern_1[1]

        if hash == hash_1:
            if pixels == pixels_1:
                return i

    return -1

unfiltered_files = []

if not input_file == None:
    output_dir = working_dir
    
    if os.path.isabs(input_file):
        unfiltered_files = [input_file]
    else:
        unfiltered_files = [working_dir + os.sep + input_file]
else:
    if os.path.isdir(src_dir):
        src_files = os.listdir(src_dir)
        for filename in src_files:
            file_path = src_dir + os.sep + filename
            unfiltered_files.append(file_path)

input_files = []

for unfiltered_file in unfiltered_files:
    
    if os.path.isfile(unfiltered_file):
        extension = os.path.splitext(unfiltered_file)[1]

        if extension in [".png", ".jpg", ".jpeg"]:
            input_files.append(unfiltered_file)

for input_file in input_files:
        filename = os.path.basename(input_file)
        filename_no_ext = os.path.splitext(filename)[0]

        console_print("encoding " + filename)

        alpha_channel = False
    
        im = Image.open(input_file)
        im = im.convert('RGBA')
        
        w, h = im.size
        pixels_n = w * h

        rgb_pixels = im.load()

        # get pixels

        pixels = [[] for i in range(0,h)]

        for y in range(0, h):
            pixels[y] = [() for i in range(0,w)]

            for x in range(0, w):
                rgb_pixel = rgb_pixels[x,y]
                r, g, b, alpha = rgb_pixel

                color = round(0.2125 * r + 0.7154 * g + 0.0721 * b)
                pixel = (color, alpha)

                if not (alpha == 255):
                    alpha_channel = True

                pixels[y][x] = pixel

        if png_output:
            output_filename = output_dir + os.sep + filename_no_ext + "-grayscale.png"
            output_im = Image.new('RGBA', (w, h))
            output_pixels = output_im.load()

            for y in range(0, h):
                for x in range(0, w):
                    color, alpha = pixels[y][x]
                    output_pixels[x,y] = (color, color, color, alpha)

            output_im.save(output_filename)

        data = bytearray()

        alpha_channel_int = 1 if alpha_channel else 0
        data.extend(alpha_channel_int.to_bytes(1, byteorder="big"))

        data.extend(w.to_bytes(4, byteorder="big"))
        data.extend(h.to_bytes(4, byteorder="big"))

        if compressed:
            # find pattern size

            console_print("find pattern size...")

            min_memory = ()
            min_memory_set = False

            start_time = time.time()

            safe_max = max(min_pattern_size, min(max_pattern_size, w))

            for k in range(min_pattern_size, safe_max + 1, pattern_step):
                s = safe_max - (k - min_pattern_size)
                p_x = math.ceil(w / s)
                p_y = math.ceil(h / s)

                memory_sum = 0
                save_memory = True
                patterns = []

                for j in range(0, p_y):
                    break_j = False

                    y = j * s

                    elapsed_time_formatted = "{:.2f}".format(time.time() - start_time)
                    console_print(str(s) + "x" + str(s) + " at [ " + str(y) + " ] [ " + elapsed_time_formatted + " s ]")

                    for z in range(0, p_x):
                        x = z * s
                        
                        pattern = get_pattern(pixels, x, y, w, h, s)
                        color_count = pattern[2]

                        found_index = find_pattern(pattern, patterns)

                        if found_index < 0:

                            # count pattern bytes
                            if alpha_channel:
                                memory_sum += s * s
                                memory_sum += color_count
                            else:
                                memory_sum += s * s

                            patterns.append(pattern)

                        # count cell bytes
                        memory_sum += 4

                        if min_memory_set and memory_sum > min_memory[1]:
                            save_memory = False
                            break_j = True
                            break
                            
                    if break_j:
                        break
                
                if save_memory:
                    memory = (s, memory_sum, len(patterns))

                    if not min_memory_set:
                        min_memory_set = True
                        min_memory = memory

                    if memory[1] <= min_memory[1]:
                        min_memory = memory

            # read cells

            pattern_info =  []

            pattern_info.append("pattern size: " + str(min_memory[0]))
            pattern_info.append("pattern count: " + str(min_memory[2]))
            pattern_info.append("bytes: " + str(min_memory[1]))

            console_print(', '.join(pattern_info))

            pattern_size = min_memory[0]

            patterns = []
            cells = []

            p_x = math.ceil(w / pattern_size)
            p_y = math.ceil(h / pattern_size)

            for j in range(0, p_y):
                y = j * pattern_size

                for z in range(0, p_x):
                    x = z * pattern_size

                    pattern = get_pattern(pixels, x, y, w, h, pattern_size)
                        
                    index = 0
                    found_index = find_pattern(pattern, patterns)

                    if found_index >= 0:
                        index = found_index
                    else:
                        index = len(patterns)
                        patterns.append(pattern)

                    cells.append(index)

            # write data

            data.extend(p_x.to_bytes(4, byteorder="big"))
            data.extend(p_y.to_bytes(4, byteorder="big"))

            data.extend(pattern_size.to_bytes(4, byteorder="big"))
            data.extend(len(patterns).to_bytes(4, byteorder="big"))

            for pattern in patterns:
                pattern_pixels = pattern[1]

                for pixel in pattern_pixels:
                    color, alpha = pixel
                    
                    data.extend(color.to_bytes(1, byteorder="big"))
                    if alpha_channel:
                        data.extend(alpha.to_bytes(1, byteorder="big"))

            for index in cells:
                data.extend(index.to_bytes(4, byteorder="big"))
            
        else:
            # write data

            for y in range(0, h):
                for x in range(0, w):
                    pixel = pixels[y][x]
                    color, alpha = pixel

                    data.extend(color.to_bytes(1, byteorder="big"))
                    if alpha_channel:
                        data.extend(alpha.to_bytes(1, byteorder="big"))
                        

        if os.path.isdir(output_dir):
            output_filename = filename_no_ext + ".rif"

            f = open(output_dir + os.sep + output_filename, "wb")
            f.write(data)
            f.close()

        console_print(output_filename + " saved")