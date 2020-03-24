from testlib import get_test_image_path
import slideio
import os

def test_open_scene():
    image_path = get_test_image_path("gdal","img_2448x2448_3x8bit_SRC_RGB_ducks.png")
    scene = slideio.open_slide(image_path, "GDAL").get_scene(0)
    img = scene.read_block()
    print(img)