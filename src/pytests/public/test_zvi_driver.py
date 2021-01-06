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


if __name__ == '__main__':
    unittest.main()
