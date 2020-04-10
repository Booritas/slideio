"""slideio module core functionality testing."""

import sys
import os
import unittest
import pytest
import slideio
from testlib import get_test_image_path


class TestCore(unittest.TestCase):
    """Tests for core functionality of the slideio module."""

    def test_driver_list(self):
        """The test checks if all drivers are available."""
        driver_ids = slideio.get_driver_ids()
        self.assertTrue("SVS" in driver_ids)
        self.assertTrue("GDAL" in driver_ids)
        self.assertTrue("CZI" in driver_ids)

    def test_not_existing_driver(self):
        """
        Test for calling of not-existing driver.

        Checks if slideio throws RuntimeError
        exception during opening of not existing file.
        """
        image_path = get_test_image_path(
            "GDAL",
            "img_2448x2448_3x8bit_SRC_RGB_ducks.png"
            )
        with pytest.raises(RuntimeError):
            slideio.open_slide(image_path, "AAAA")


if __name__ == '__main__':
    unittest.main()
