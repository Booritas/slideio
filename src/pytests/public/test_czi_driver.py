"""slideio CZI driver testing."""

import unittest
import pytest
import cv2 as cv
import numpy as np
import slideio
from testlib import get_test_image_path


class TestCZI(unittest.TestCase):
    """Tests for slideio CZI driver functionality."""

    def test_not_existing_file(self):
        """
        Opening of not existing image.

        Checks if slideio throws RuntimeError
        exception during opening of not existing file.
        """
        image_path = "missing_file.png"
        with pytest.raises(RuntimeError):
            slideio.open_slide(image_path, "CZI")

    def test_file_metadata(self):
        """
        Checks image metadata.

        Opens 3 channel uncompressed czi file and checks metadata.
        """
        image_path = get_test_image_path(
            "czi",
            "pJP31mCherry.czi"
            )
        slide = slideio.open_slide(image_path, "CZI")
        self.assertTrue(slide is not None)
        num_scenes = slide.num_scenes
        self.assertEqual(num_scenes, 1)
        self.assertEqual(image_path, slide.file_path)
        scene = slide.get_scene(0)
        self.assertTrue(scene is not None)
        self.assertEqual(image_path, scene.file_path)
        self.assertEqual(3, scene.num_channels)
        scene_rect = scene.rect
        self.assertEqual(0, scene_rect[0])
        self.assertEqual(0, scene_rect[1])
        self.assertEqual(512, scene_rect[2])
        self.assertEqual(512, scene_rect[3])
        self.assertEqual(100, scene.magnification)
        raw_metadata = slide.raw_metadata
        self.assertTrue(raw_metadata.startswith("<ImageDocument>"))
        for channel_index in range(scene.num_channels):
            channel_type = scene.get_channel_data_type(channel_index)
            self.assertEqual(channel_type, np.uint8)
            compression = scene.compression
            self.assertEqual(compression, slideio.Compression.Uncompressed)
        res = scene.resolution
        correct_res = 9.76783e-8
        self.assertEqual(correct_res, pytest.approx(res[0]))
        self.assertEqual(correct_res, pytest.approx(res[1]))

    def test_file_metadata2(self):
        """
        Checks image metadata.

        Opens 6 channel uncompressed czi file and checks metadata.
        """
        image_path = get_test_image_path(
            "czi",
            "08_18_2018_enc_1001_633.czi"
            )
        channel_names = ["646", "655", "664", "673", "682", "691"]
        slide = slideio.open_slide(image_path, "CZI")
        self.assertTrue(slide is not None)
        num_scenes = slide.num_scenes
        self.assertEqual(num_scenes, 1)
        self.assertEqual(image_path, slide.file_path)
        scene = slide.get_scene(0)
        self.assertTrue(scene is not None)
        self.assertEqual(image_path, scene.file_path)
        self.assertEqual(6, scene.num_channels)
        scene_rect = scene.rect
        self.assertEqual(0, scene_rect[0])
        self.assertEqual(0, scene_rect[1])
        self.assertEqual(1000, scene_rect[2])
        self.assertEqual(1000, scene_rect[3])
        self.assertEqual(63, scene.magnification)
        for channel_index in range(scene.num_channels):
            channel_type = scene.get_channel_data_type(channel_index)
            self.assertEqual(channel_type, np.uint16)
            compression = scene.compression
            self.assertEqual(compression, slideio.Compression.Uncompressed)
            channel_name = scene.get_channel_name(channel_index)
            self.assertEqual(channel_name, channel_names[channel_index])
        res = scene.resolution
        correct_res = 6.7475572821478794e-8
        self.assertEqual(correct_res, pytest.approx(res[0]))
        self.assertEqual(correct_res, pytest.approx(res[1]))

    def test_read_block_2d(self):
        """
        Read 2d uncompressed block.

        Reads thew whole uncompressed image and compares received
        raster against reference images.
        """
        # Image to test
        image_path = get_test_image_path(
            "czi",
            "pJP31mCherry.czi"
            )
        # Reference channel images
        channel_image_paths = [
            get_test_image_path(
                "czi",
                "pJP31mCherry.grey/pJP31mCherry_b0t0z0c0x0-512y0-512.bmp"
                ),
            get_test_image_path(
                "czi",
                "pJP31mCherry.grey/pJP31mCherry_b0t0z0c1x0-512y0-512.bmp"
                ),
            get_test_image_path(
                "czi",
                "pJP31mCherry.grey/pJP31mCherry_b0t0z0c2x0-512y0-512.bmp"
                )
            ]
        slide = slideio.open_slide(image_path, "CZI")
        self.assertTrue(slide is not None)
        scene = slide.get_scene(0)
        self.assertTrue(scene is not None)
        for channel_index in range(scene.num_channels):
            image_raster = scene.read_block(channel_indices=[channel_index])
            reference_raster = cv.imread(
                channel_image_paths[channel_index],
                cv.IMREAD_GRAYSCALE
            )
            self.assertTrue(np.array_equal(image_raster, reference_raster))

    def test_read_block_4d(self):
        """
        Read 4d uncompressed block.

        Reads thew whole uncompressed image and compares received
        raster against reference images.
        """
        # Image to test
        image_path = get_test_image_path(
            "czi",
            "pJP31mCherry.czi"
            )
        slide = slideio.open_slide(image_path, "CZI")
        self.assertTrue(slide is not None)
        scene = slide.get_scene(0)
        self.assertTrue(scene is not None)
        slices = (2, 5)
        frames = (0, 1)
        for channel_index in range(scene.num_channels):
            image_raster = scene.read_block(
                channel_indices=[channel_index],
                slices=slices,
                frames=frames
                )
            image_shape = image_raster.shape
            self.assertEqual(image_shape, (3, 512, 512))
            for slice_index in range(slices[0], slices[1]):
                slice_raster = image_raster[slice_index-slices[0], :, :]
                slice_shape = slice_raster.shape
                self.assertEqual(slice_shape, (512, 512))
                ref_image_name = "pJP31mCherry.grey/pJP31mCherry_b0t0z" + \
                    str(slice_index) + "c" + str(channel_index) + \
                    "x0-512y0-512.bmp"
                ref_image_path = get_test_image_path(
                    "czi",
                    ref_image_name
                    )
                reference_raster = cv.imread(
                    ref_image_path,
                    cv.IMREAD_GRAYSCALE
                    )
                self.assertTrue(np.array_equal(slice_raster, reference_raster))

    def test_corrupted_image(self):
        """
        Raise an error for corrupted images.

        Tries to open and read a corrupted image.
        slideio shall raise an exception.
        """
        # Image to test
        image_path = get_test_image_path(
            "czi",
            "corrupted.czi"
            )
        with pytest.raises(RuntimeError):
            slideio.open_slide(image_path, "CZI")




if __name__ == '__main__':
    unittest.main()
