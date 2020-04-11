"""
slideio SVS driver testing.

Testing of SVS driver functionality.
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


class TestSvsPriv(unittest.TestCase):
    """Tests for slideio SVS driver functionality."""

    def test_jp2k_16b_read_block(self):
        """
        Read a block of Jpeg2K compressed 16 bit image.

        Reads a block from a slide and compares it
        with a block exported by ImageScope software.
        """
        # Image to test
        image_path = get_test_image_path(
            "svs",
            "jp2k_1chnl.svs"
            )
        # Image to test
        reference_image_path = get_test_image_path(
            "svs",
            "jp2k_1chnl.region_x700_y400_w500_y400.tif"
            )
        # Read reference image
        reference_image = cv.imread(
            reference_image_path,
            cv.IMREAD_UNCHANGED
            )
        x_beg = 700
        y_beg = 400
        width = 500
        height = 400
        # Read region with slideio
        slide = slideio.open_slide(image_path, "SVS")
        self.assertTrue(slide is not None)
        scene = slide.get_scene(0)
        self.assertTrue(scene is not None)
        region = scene.read_block(
            (x_beg, y_beg, width, height)
        )
        # call structural difference score
        scores_cf = cv.matchTemplate(
            region.astype(np.float32),
            reference_image.astype(np.float32),
            cv.TM_CCOEFF_NORMED
            )[0][0]
        # calculate square normed errors
        scores_sq = cv.matchTemplate(
            region.astype(np.float32),
            reference_image.astype(np.float32),
            cv.TM_SQDIFF_NORMED
            )[0][0]
        self.assertLess(0.99, scores_cf)
        self.assertLess(scores_sq, 0.0002)

    def test_jp2k_16b_resample_block(self):
        """
        Test for resampling of a 16bit block.

        A region of 16 bit, jpeg 2000 compressed image is read
        and resized with different scale.
        An reference block is exportded by ImageScope software
        on scale 1:1 and resized with opencv with the same
        scale coefficients as the original region.
        """
        # Image to test
        image_path = get_test_image_path(
            "svs",
            "jp2k_1chnl.svs"
            )
        # Image to test
        reference_image_path = get_test_image_path(
            "svs",
            "jp2k_1chnl.region_x700_y400_w500_y400.tif"
            )
        # Read reference image
        reference_image = cv.imread(
            reference_image_path,
            cv.IMREAD_UNCHANGED
            )
        x_beg = 700
        y_beg = 400
        width = 500
        height = 400
        rect = (x_beg, y_beg, width, height)
        size = (width, height)

        # open the slide
        slide = slideio.open_slide(image_path, "SVS")
        self.assertTrue(slide is not None)
        scene = slide.get_scene(0)
        self.assertTrue(scene is not None)

        scaling_params = {
            1.0: (0.9999, 2.e-7),
            1.5: (0.99, 0.002),
            2.0: (0.999, 1.e-7),
            3.0: (0.98, 0.01),
            5.0: (0.98, 0.01)
            }
        for resize_cof, params in scaling_params.items():
            new_size = (
                int(round(size[0]/resize_cof)),
                int(round(size[1]/resize_cof))
                )
            resized_raster = scene.read_block(
                rect,
                size=new_size
                )
            cv_resized_raster = cv.resize(reference_image, new_size)
            # call structural difference score
            score_cf = cv.matchTemplate(
                resized_raster.astype(np.float32),
                cv_resized_raster.astype(np.float32),
                cv.TM_CCOEFF_NORMED
                )[0][0]
            # calculate square normed errors
            score_sq = cv.matchTemplate(
                resized_raster.astype(np.float32),
                cv_resized_raster.astype(np.float32),
                cv.TM_SQDIFF_NORMED
                )[0][0]
            self.assertLess(params[0], score_cf)
            self.assertLess(score_sq, params[1])

    def test_jp2k_16b_regression(self):
        """
        Regression test for 16b Jpeg 2K compressed image.

        Read the block from a slide and compares it
        with a reference image. The reference image
        is obtained by reading of the same region
        by the slideo and savint it as a tif file.
        """
        # Image to test
        image_path = get_test_image_path(
            "svs",
            "jp2k_1chnl.svs"
            )
        # Image to test
        reference_image_path = get_test_image_path(
            "svs",
            "jp2k_1chnl.regression_x700_y400_w500_y400.tif"
            )
        # Read reference image
        reference_image = cv.imread(
            reference_image_path,
            cv.IMREAD_UNCHANGED
            )
        x_beg = 700
        y_beg = 400
        width = 500
        height = 400
        rect = (x_beg, y_beg, width, height)
        new_size = (100, 80)

        # open the slide
        slide = slideio.open_slide(image_path, "SVS")
        self.assertTrue(slide is not None)
        scene = slide.get_scene(0)
        self.assertTrue(scene is not None)
        block_raster = scene.read_block(
            rect,
            size=new_size
            )
        self.assertTrue(np.array_equal(block_raster, reference_image))


if __name__ == '__main__':
    unittest.main()
