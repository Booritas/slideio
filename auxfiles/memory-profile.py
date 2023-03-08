import sys
sys.path.insert(1, r'd:\Projects\slideio\slideio\build\Windows\bin\Debug')
#from memory_profiler import profile
import slideiopybind as sld

#@profile(precision=4)
def open_image(path, driver):
    image = sld.open_slide(str(path), driver)
    return image

path1 = r"d:\Projects\slideio\images\svs\JP2K-33003-1.svs"
path2 = r"d:\Projects\slideio\images\svs\S1303802-11-HE-DX1.svs"
#slide = open_image(path1,"SVS")
slide = open_image(path2,"SVS")
print('done')

