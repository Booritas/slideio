"""
slideio CZI driver testing.

Testing of CZI driver functionality
for 16 bit images.
The testing images are located in a private repository
and cannot be shared.
Path to the repository is defined in the
environmental variable SLIDEIO_TEST_DATA_PRIV_PATH.
"""

import unittest
import cv2 as cv
import numpy as np
import slideio
from testlib import get_test_image_path
import czifile as czi


class TestCzi16bPriv(unittest.TestCase):
    """Tests for slideio CZI driver functionality."""

    def test_jpegxr_16b_read_block(self):
        """
        Read a block of jpegxr compressed 16 bit image.

        Reads a block from a slide and compares it
        with a block read by czifile package.
        Small deviations can be explained by different
        order in processing of tiles. Czi file contains
        overlapped tiles. Rasters can be slightly different
        in the overlapped areas (depends of tile order).
        """
        # Image to test
        image_path = get_test_image_path(
            "czi",
            "jxr-16bit-4chnls.czi"
            )
        slide = slideio.open_slide(image_path, "CZI")
        self.assertTrue(slide is not None)
        scene = slide.get_scene(0)
        self.assertTrue(scene is not None)
        image_raster = scene.read_block()
        reference_raster = np.transpose(
            czi.imread(image_path)[0, 0, 0, :, :, :, 0],
            (1, 2, 0)
            )
        # call structural difference score
        scores_cf = cv.matchTemplate(
                    reference_raster.astype(np.float32),
                    image_raster.astype(np.float32),
                    cv.TM_CCOEFF_NORMED
                    )[0][0]
        # calculate square normed errors
        scores_sq = cv.matchTemplate(
                    reference_raster.astype(np.float32),
                    image_raster.astype(np.float32),
                    cv.TM_SQDIFF_NORMED
                    )[0][0]
        self.assertLess(0.99, scores_cf)
        self.assertLess(scores_sq, 0.0002)

    def test_jpegxr_16b_resample_block(self):
        """
        Test for resampling of a block.

        A region of 16 bit, jpegxr compressed image is read
        with 1:1 scale and resampled with different scales.
        1:1 block is resized in memory by opencv and compared
        with resampled region from slideio.
        """
        # Image to test
        image_path = get_test_image_path(
            "czi",
            "jxr-16bit-4chnls-2.czi"
            )
        region_size = (1000, 500)
        region_rect = (5000, 2000, region_size[0], region_size[1])
        slide = slideio.open_slide(image_path, "CZI")
        self.assertTrue(slide is not None)
        scene = slide.get_scene(0)
        self.assertTrue(scene is not None)
        origin_raster = scene.read_block(region_rect)
        scaling_params = {
            1.0: (0.9999, 1.e-7),
            1.5: (0.99, 0.0004),
            2.0: (0.98, 0.0035),
            3.0: (0.95, 0.007),
            5.0: (0.95, 0.008)
            }
        for resize_cof, params in scaling_params.items():
            new_size = (
                int(round(region_size[0]/resize_cof)),
                int(round(region_size[1]/resize_cof))
                )
            resized_raster = scene.read_block(
                region_rect,
                size=new_size
                )
            cv_resized_raster = cv.resize(origin_raster, new_size)
            # call structural difference score
            scores_cf = cv.matchTemplate(
                        resized_raster.astype(np.float32),
                        cv_resized_raster.astype(np.float32),
                        cv.TM_CCOEFF_NORMED
                        )[0][0]
            # calculate square normed errors
            scores_sq = cv.matchTemplate(
                        resized_raster.astype(np.float32),
                        cv_resized_raster.astype(np.float32),
                        cv.TM_SQDIFF_NORMED
                        )[0][0]
            self.assertLess(params[0], scores_cf)
            self.assertLess(scores_sq, params[1])

    def test_jpegxr_16b_regression(self):
        """
        Regression test for 16b jpegxr compressed image.

        Read the block from a slide and compares it
        with a reference image. The reference image
        is obtained by reading of the same region
        by the slideo and savint it as a tif file.
        """
        # Image to test
        image_path = get_test_image_path(
            "czi",
            "jxr-16bit-4chnls-2.czi"
            )
        # Reference image
        ref_image_path = get_test_image_path(
            "czi",
            "jxr-16bit-4chnls-2/regression_x5000_y2000_w1000_h500.tif"
        )
        region_size = (1000, 500)
        region_new_size = (300, 150)
        region_rect = (5000, 2000, region_size[0], region_size[1])
        slide = slideio.open_slide(image_path, "CZI")
        self.assertTrue(slide is not None)
        scene = slide.get_scene(0)
        self.assertTrue(scene is not None)
        image = scene.read_block(region_rect, size=region_new_size)
        reference_image = cv.imread(ref_image_path, cv.IMREAD_UNCHANGED)
        self.assertTrue(np.array_equal(image, reference_image))


if __name__ == '__main__':
    unittest.main()
