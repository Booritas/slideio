import slideio
import os

def get_test_image_path(folder, image):
    root_path = os.getenv("SLIDEIO_TEST_DATA_PATH")
    return os.path.join(root_path, folder, image)

def test_open_slide():
    image_path = get_test_image_path("gdal","img_2448x2448_3x8bit_SRC_RGB_ducks.png")
    slide = slideio.open_slide(image_path, "GDAL")
    numb_scenes = slide.get_numb_scenes()
    assert numb_scenes == 1