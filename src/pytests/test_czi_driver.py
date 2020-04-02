from testlib import get_test_image_path
import slideio
import os

def test_open_scene():
	image_path = get_test_image_path("czi","test2.czi")
	slide = slideio.open_slide(image_path, "CZI")
	scene = slide.get_scene(2)
	image = scene.read_block(size=(500,0))

if __name__ == "__main__":
	test_open_scene()