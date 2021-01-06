"""slideio ZVI driver testing."""

import unittest
import pytest
import cv2 as cv
import numpy as np
import slideio
from testlib import get_test_image_path


class TestZVI(unittest.TestCase):
    """Tests for slideio ZVI driver functionality."""

    def test_not_existing_file(self):
        """
        Opening of not existing image.

        Checks if slideio throws RuntimeError
        exception during opening of not existing file.
        """
        image_path = "missing_file.png"
        with pytest.raises(RuntimeError):
            slideio.open_slide(image_path, "ZVI")

    def test_readblock_emptyChannelIndices(self):
        """
        Read a 3D image block w/o setting of channel indices

        """
        image_path = get_test_image_path(
            "zvi",
            "Zeiss-1-Stacked.zvi"
            )
            
        with slideio.open_slide(image_path, "ZVI") as slide:
            self.assertTrue(slide is not None)
            with slide.get_scene(0) as scene:
                # read channel block
                raster = scene.read_block(slices=(0,3))
                shape = raster.shape
                self.assertEqual(shape[0],3)
                self.assertEqual(shape[1],1040)
                self.assertEqual(shape[2],1388)
                self.assertEqual(shape[3],3)

if __name__ == '__main__':
    unittest.main()
