import sys
sys.path.insert(1, r'd:\Projects\slideio\slideio\build\Windows\bin\Debug')
import slideiopybind as sld

path1 = r"d:\Projects\slideio\images\czi\jxr-rgb-5scenes.czi"
path2 = r"d:\Projects\temp\out.svs"
slide_input = sld.open_slide(path1,"AUTO")
scene_input = slide_input.get_scene(0)

params = sld.SVSJpegParameters()
params.quality = 90
sld.convert_scene(scene_input,params,path2)
print('done')

