import sys
sys.path.insert(1, r'd:\Projects\slideio\slideio\build\Windows\bin\Debug')
#from memory_profiler import profile
import slideiopybind as sld

#@profile(precision=4)
def open_image(path, driver):
    image = sld.open_slide(str(path), driver)
    return image

path = "D:\\Projects\\slideio\\images\\pke\\openmicroscopy\\PKI_scans\\HandEcompressed_Scan1.qptiff"
slide = sld.open_slide(path,"QPTIFF")
scene = slide.get_scene(0)
thw = 500
rect = scene.rect
print(f'reading {rect}')
image = scene.read_block(rect,size=(thw,0))
print(f'done {image.shape}')

