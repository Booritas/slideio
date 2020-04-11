"""
slideio CZI driver testing.

Testing of CZI driver functionality
for bright field images.
The testing images are located in a private repository
and cannot be shared.
Path to the repository is defined in the
environmental variable SLIDEIO_TEST_DATA_PRIV_PATH.
"""

import unittest
import cv2 as cv
import numpy as np
import slideio
from privtestlib import get_priv_test_image_path as get_test_image_path


class TestCziRgbPriv(unittest.TestCase):
    """Tests for slideio CZI driver functionality."""

    def test_jpegxr_rgb_read_block(self):
        """
        Read a block of rgb jpegxr compressed image.

        Read the block from a slide and compares it
        with a reference image.
        """
        # Image to test
        image_path = get_test_image_path(
            "czi",
            "jxr-rgb-1scene-large.czi"
            )
        # Reference channel images
        ref_image_paths = get_test_image_path(
            "czi",
            "jxr-rgb-1scene-large/region.png"
            )
        slide = slideio.open_slide(image_path, "CZI")
        self.assertTrue(slide is not None)
        scene = slide.get_scene(0)
        self.assertTrue(scene is not None)
        image_raster = scene.read_block(
            (63000, 18000, 1000, 500),
            channel_indices=[2, 1, 0]
            )
        reference_raster = cv.imread(ref_image_paths)
        self.assertTrue(np.array_equal(image_raster, reference_raster))

    def test_jpegxr_rgb_resample_block(self):
        """
        Read a block of rgb jpegxr compressed image.

        Read the block from a slide and compares it
        with a reference image.
        """
        # Image to test
        image_path = get_test_image_path(
            "czi",
            "jxr-rgb-1scene-large.czi"
            )
        # Reference channel images
        ref_image_paths = get_test_image_path(
            "czi",
            "jxr-rgb-1scene-large/region.png"
            )
        scaling_params = {
            1.0: (0.9999, 1.e-7),
            1.5: (0.98, 0.005),
            2.0: (0.92, 0.03),
            3.0: (0.90, 0.035),
            5.0: (0.86, 0.05)
            }

        reference_image = cv.imread(ref_image_paths)
        slide = slideio.open_slide(image_path, "CZI")
        scene = slide.get_scene(0)
        # check accuracy of resizing for different scale coeeficients
        for resize_cof, params in scaling_params.items():
            new_size = (
                int(round(reference_image.shape[1]/resize_cof)),
                int(round(reference_image.shape[0]//resize_cof))
                )
            reference_raster = cv.resize(reference_image, new_size)
            image_raster = scene.read_block(
                (63000, 18000, 1000, 500),
                size=new_size,
                channel_indices=[2, 1, 0]
                )
            scores_cf = cv.matchTemplate(
                reference_raster,
                image_raster,
                cv.TM_CCOEFF_NORMED
                )[0][0]

            scores_sq = cv.matchTemplate(
                reference_raster,
                image_raster,
                cv.TM_SQDIFF_NORMED
                )[0][0]
            print(resize_cof, scores_cf, scores_sq)
            self.assertLess(params[0], scores_cf)
            self.assertLess(scores_sq, params[1])

    def test_jpegxr_rgb_regression(self):
        """
        Regression test for rgb jpegxr compressed image.

        Read the block from a slide and compares it
        with a reference image. The reference image
        is obtained by reading of the same region
        by the slideo and saving it as png file.
        """
        # Image to test
        image_path = get_test_image_path(
            "czi",
            "jxr-rgb-1scene-large.czi"
            )
        # Reference channel images
        ref_image_path = get_test_image_path(
            "czi",
            "jxr-rgb-1scene-large/regression_x63000_y18000_w1000_h500.png"
            )

        reference_image = cv.imread(ref_image_path)
        new_size = (reference_image.shape[1], reference_image.shape[0])
        slide = slideio.open_slide(image_path, "CZI")
        scene = slide.get_scene(0)
        # check accuracy of resizing for different scale coeeficients
        image_raster = scene.read_block(
            (63000, 18000, 1000, 500),
            size=new_size
            )
        self.assertTrue(np.array_equal(image_raster, reference_image))

    def test_jpegxr_rgb_regression_small(self):
        """
        Regression test for rgb jpegxr compressed image.

        Read the block from a slide and compares it
        with a reference image. The reference image
        is obtained by reading of the same region
        by the slideo and saving it as png file.
        Old version of the jpegxr codec delivered
        corupted images for the scene 2 with low
        scaling
        """
        # Image to test
        image_path = get_test_image_path(
            "czi",
            "jxr-rgb-5scenes.czi"
            )
        # Reference channel images
        ref_image_path = get_test_image_path(
            "czi",
            "jxr-rgb-5scenes.czi.regression_s2.png"
            )

        reference_image = cv.imread(ref_image_path, cv.IMREAD_UNCHANGED)
        slide = slideio.open_slide(image_path, "CZI")
        scene = slide.get_scene(2)
        # check accuracy of resizing for different scale coeeficients
        image_raster = scene.read_block(
            size=(400, 0)
            )
        self.assertTrue(np.array_equal(image_raster, reference_image))


if __name__ == '__main__':
    unittest.main()
