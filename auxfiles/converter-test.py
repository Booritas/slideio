import sys
import os
sys.path.insert(1, r'd:\Projects\slideio\slideio\build\Windows\bin\Debug')
import slideiopybind as sld

def delete_file(file_path):
    if os.path.isfile(file_path):
        try:
            os.remove(file_path)
            print(f"{file_path} deleted successfully.")
        except Exception as e:
            print(f"Error deleting {file_path}: {e}")
    else:
        print(f"{file_path} does not exist.")

def convertJpeg(path1, path2):
    slide_input = sld.open_slide(path1,"AUTO")
    scene_input = slide_input.get_scene(0)

    params = sld.SVSJpegParameters()
    params.quality = 90
    sld.convert_scene(scene_input,params,path2)

def convertJp2K(path1, path2, callback):
    slide_input = sld.open_slide(path1,"AUTO")
    scene_input = slide_input.get_scene(0)

    params = sld.SVSJp2KParameters()
    sld.convert_scene_ex(scene_input,params,path2, callback)

def callback(a):
    print(f"Hello from python: {a}")

path1 = r"d:\Projects\slideio\images\czi\jxr-rgb-5scenes.czi"
#path1 = r"d:\Projects\slideio\images\czi\16bit_CH_1_doughnut_crop.czi"
path2 = r"d:\Projects\temp\out.svs"
delete_file(path2)
convertJp2K(path1, path2, callback)
print('done')

