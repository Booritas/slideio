import sys
import os
sys.path.insert(1, r'd:\Projects\slideio\slideio\build\Windows\bin\Debug')
import slideiopybind as sld

path = r"d:\Projects\slideio\images\czi\jxr-rgb-5scenes.czi"
slide_input = sld.open_slide(path,"AUTO")
scene_input = slide_input.get_scene(0)

params = sld.ColorTransformation()
params.color_space = sld.ColorSpace.HSV
transformed = sld.transform_scene(scene_input,[params])

print('done')

