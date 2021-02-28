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
import czifile as czi
import slideio
from privtestlib import get_priv_test_image_path as get_test_image_path


class TestCziPriv(unittest.TestCase):
    """Tests for slideio CZI driver functionality."""
    def test_file_rgb_metadata_pictures(self):
        """
        Checks image metadata pictures.

        Opens 3 channel brightfield Jpeg commpressed file
        and checks metadata pictures agains extracted by
        a 3rd party tools pictures.
        """
        image_path = get_test_image_path(
            "czi",
            "jxr-rgb-5scenes.czi"
        )
        metadata_pic_paths = {
            "Label": get_test_image_path(
                "czi",
                "jxr-rgb-5scenes.label.tiff"
            ),
            "SlidePreview": get_test_image_path(
                "czi",
                "jxr-rgb-5scenes.preview.tiff"
            ),
            "Thumbnail": get_test_image_path(
                "czi",
                "jxr-rgb-5scenes.thumb.png"
            ),
        }
        slide = slideio.open_slide(image_path, "CZI")
        self.assertTrue(slide is not None)
        num_aux_images = slide.num_aux_images
        self.assertEqual(num_aux_images, 3)
        for image_name in metadata_pic_paths.keys():
            image = slide.get_aux_image_raster(image_name)
            reference_image = slideio.open_slide(metadata_pic_paths[image_name], "GDAL").get_scene(0).read_block()
            self.assertEqual(image.shape, reference_image.shape)
            score = slideio.compare_images(image, reference_image)
            self.assertEqual(score, 1)
            aux_scene = slide.get_aux_image(image_name)
            rect = aux_scene.rect;
            self.assertEqual(image.shape[0], rect[3])
            self.assertEqual(image.shape[1], rect[2])
