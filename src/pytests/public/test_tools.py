"""slideio module tools functionality testing."""

import unittest
import pytest
import slideio
import numpy as np
import cv2 as cv
from testlib import get_test_image_path


class TestTools(unittest.TestCase):
    """Tests for core functionality of the slideio module."""

    def test_image_similarity_equal(self):
        """Similarity score of equal images shall be 1."""
        image_path = get_test_image_path(
            "GDAL",
            "img_2448x2448_3x8bit_SRC_RGB_ducks.png"
            )
        with slideio.open_slide(image_path, "gdal") as slide:
            with slide.get_scene(0) as scene:
                img1 = scene.read_block()
                img2 = scene.read_block()
        score = slideio.compare_images(img1, img2)
        self.assertEqual(score, 1)
        blank_image = np.zeros(img1.shape, np.uint8)
        score = slideio.compare_images(img1, blank_image)
        self.assertLess(score, 0.5)
        blur = cv.blur(img2,(20,20))
        score = slideio.compare_images(img1, blur)
        self.assertLess(score, 0.98)
        self.assertGreater(score, 0.8)


if __name__ == '__main__':
    unittest.main()
