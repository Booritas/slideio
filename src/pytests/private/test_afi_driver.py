"""slideio AFI driver testing."""

import cv2 as cv
import numpy as np
import posixpath
import os
import posixpath
import pytest
import slideio
import unittest
from privtestlib import get_priv_test_image_path


class TestAFI(unittest.TestCase):
    """Tests for slideio AFI driver functionality."""

    def test_not_existing_file(self):
        """
        Opening of not existing image.

        Checks if slideio throws RuntimeError
        exception during opening of not existing file.
        """
        image_path = "missing_file.png"
        with pytest.raises(RuntimeError):
            slideio.open_slide(image_path, "AFI")

    def disabled_test_corrupted_fail_file(self):
        """
        Opening of a corrupted image.

        Checks if slideio throws RuntimeError
        exception during opening.
        """
        image_path = get_priv_test_image_path(
            "afi",
            "corrupted.afi"
            )
        slide = slideio.open_slide(image_path, "AFI")
        scene = slide.get_scene(0)
        with pytest.raises(RuntimeError):
            scene.read_block()

    def test_valid_file(self):
        """
        Checks scenes of a valid file.

        Opens a valid afi file and checks the number of
        scenes loaded
        """
        image_path = get_priv_test_image_path(
            "afi",
            "fs.afi"
            )
        slide = slideio.open_slide(image_path, "AFI")
        self.assertTrue(slide is not None)
        num_scenes = slide.num_scenes
        self.assertEqual(num_scenes, 3)
        self.assertEqual(image_path, slide.file_path)
        self.assertEqual(slide.get_scene(1).name, "Image")
        expected_path = get_priv_test_image_path("afi", 
                                                 "fs_Alexa Fluor 488.svs")
        expected_path = expected_path.replace(os.sep, posixpath.sep)
        self.assertEqual(slide.get_scene(1).file_path, 
                         expected_path)

    def test_image_block(self):
        """
        Checks image block

        Opens a valid file and checks image block from
        one of the scenes.
        """
        image_path = get_priv_test_image_path(
            "afi",
            "fs.afi"
            )
        slide = slideio.open_slide(image_path, "AFI")
        self.assertTrue(slide is not None)
        scene = slide.get_scene(1)
        self.assertTrue(scene is not None)
        
        rect = (2500, 4000, 400, 400)
        scene_image = scene.read_block(rect)

        scene_image = scene_image.astype("float32")
        
        reference_image_file = get_priv_test_image_path(
                    "afi", 
                    "fs_Alexa Fluor 488_block_2500_4000_400_400.tif"
                    )
        reference_image = cv.imread(
            reference_image_file,
            cv.IMREAD_UNCHANGED
            )
        reference_image = reference_image.astype("float32")
        score = cv.matchTemplate(
                scene_image,
                reference_image,
                cv.TM_CCOEFF_NORMED
                )
        min_score = np.amin(score)
        self.assertLess(.99, min_score);


    def test_image_block_resampled(self):
        """
        Checks image block (resampled)

        Opens a valid file, reads an image block 
        from
        one of the scenes.
        """
        image_path = get_priv_test_image_path(
            "afi",
            "fs.afi"
            )
        slide = slideio.open_slide(image_path, "AFI")
        self.assertTrue(slide is not None)
        scene = slide.get_scene(1)
        self.assertTrue(scene is not None)
        
        rect = (2500, 4000, 400, 400)
        size = (800, 800)
        scene_image = scene.read_block(rect, size=size)
        scene_image = scene_image.astype("float32")
        
        reference_image_file = get_priv_test_image_path(
                    "afi", 
                    "fs_Alexa Fluor 488_block_2500_4000_400_400.tif"
                    )
        reference_image = cv.imread(
            reference_image_file,
            cv.IMREAD_UNCHANGED
            )
        reference_region = cv.resize(reference_image, size)
        reference_region = reference_region.astype("float32")
        self.assertEqual(scene_image.shape, reference_region.shape)
        score = cv.matchTemplate(
                scene_image,
                reference_region,
                cv.TM_CCOEFF_NORMED
                )
        min_score = np.amin(score)
        self.assertLess(.99, min_score);


if __name__ == '__main__':
    unittest.main()
