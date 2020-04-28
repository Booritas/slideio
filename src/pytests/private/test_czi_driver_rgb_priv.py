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
from privtestlib import get_priv_test_image_path as get_test_image_path,\
    get_priv_regression_image_path as get_regression_image_path


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
            "jxr-rgb-5scenes.czi"
            )
        # Reference channel images
        ref_image_paths = get_test_image_path(
            "czi",
            "jxr-rgb-5scenes/jxr-rgb-5scenes_s3_x2500_y3000_w400_h600.png"
            )
        slide = slideio.open_slide(image_path, "CZI")
        self.assertTrue(slide is not None)
        scene = slide.get_scene(3)
        self.assertTrue(scene is not None)
        image_raster = scene.read_block(
            (2500, 3000, 400, 600)
            )
        reference_raster = cv.imread(ref_image_paths,
                                     cv.IMREAD_UNCHANGED)
        scores_cf = cv.matchTemplate(
            reference_raster,
            image_raster,
            cv.TM_CCOEFF_NORMED
            )[0][0]
        self.assertLess(0.99, scores_cf)

    def test_jpegxr_rgb_resample_block(self):
        """
        Read a block of rgb jpegxr compressed image.

        Read the block from a slide and compares it
        with a reference image.
        """
        # Image to test
        image_path = get_test_image_path(
            "czi",
            "jxr-rgb-5scenes.czi"
            )
        # Reference channel images
        ref_image_paths = get_test_image_path(
            "czi",
            "jxr-rgb-5scenes/jxr-rgb-5scenes_s3_x2500_y3000_w400_h600.png"
            )

        scaling_params = {
            1.0: 0.99,
            1.5: 0.94,
            2.0: 0.92,
            3.0: 0.85
            }

        reference_image = cv.imread(ref_image_paths)
        slide = slideio.open_slide(image_path, "CZI")
        scene = slide.get_scene(3)
        # check accuracy of resizing for different scale coeeficients
        for resize_cof, thresh in scaling_params.items():
            new_size = (
                int(round(reference_image.shape[1]/resize_cof)),
                int(round(reference_image.shape[0]//resize_cof))
                )
            reference_raster = cv.resize(reference_image, new_size)
            image_raster = scene.read_block(
                (2500, 3000, 400, 600),
                size=new_size,
                channel_indices=[0, 1, 2]
                )
            scores_cf = cv.matchTemplate(
                reference_raster,
                image_raster,
                cv.TM_CCOEFF_NORMED
                )[0][0]
            self.assertLess(thresh, scores_cf)

    def test_jpegxr_rgb_regression(self):
        """
        Regression test for rgb jpegxr compressed image.

        Read the block from a slide and compares it
        with a reference image. The reference image
        is obtained by reading of the same region
        by the slideo and saving it as png file.
        """
        image_name = "jxr-rgb-5scenes.czi"
        folder = "czi"
        driver = "CZI"
        ext = ".png"
        # Image to test
        image_path = get_test_image_path(
            folder,
            image_name
            )
        rect = (4000, 2000, 1000, 800)
        new_size = (250, 200)
        scene_index = 2
        # export_image_region(folder,
        #                    image_name, driver, scene_index,
        #                    rect, new_size, ext
        #                    )
        slide = slideio.open_slide(image_path, driver)
        scene = slide.get_scene(scene_index)

        for channel_index in range(scene.num_channels):
            image_raster = scene.read_block(
                rect,
                new_size,
                channel_indices=[channel_index]
                )
            regr_image_path = get_regression_image_path(
                folder, image_name, scene_index, channel_index,
                rect, ext
                )
            reference_image = cv.imread(regr_image_path,
                                        cv.IMREAD_UNCHANGED)
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
