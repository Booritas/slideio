"""slideio GDAL driver testing."""

import unittest
import pytest
import cv2 as cv
import slideio
import numpy as np
from testlib import get_test_image_path


class TestGDAL(unittest.TestCase):
    """Tests for slideio GDAL driver functionality."""

    def test_not_existing_file(self):
        """
        Opening of not existing image.

        slideio shall throw RuntimeError
        exception during opening of not existing image.
        """
        image_path = "missing_file.png"
        with pytest.raises(RuntimeError):
            slideio.open_slide(image_path, "GDAL")

    def test_3chnl_png_metadata(self):
        """Opens 3 channel png file and checks metadata."""
        image_path = get_test_image_path(
            "gdal",
            "img_2448x2448_3x8bit_SRC_RGB_ducks.png"
            )
        slide = slideio.open_slide(image_path, "GDAL")
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
        self.assertEqual(2448, scene_rect[2])
        self.assertEqual(2448, scene_rect[3])
        for channel_index in range(scene.num_channels):
            channel_type = scene.get_channel_data_type(channel_index)
            self.assertEqual(channel_type, np.uint8)
            compression = scene.compression
            self.assertEqual(compression, slideio.Compression.Png)
        res = scene.resolution
        self.assertEqual(0, res[0])
        self.assertEqual(0, res[1])

    def test_1chnl_png_metadata(self):
        """Opens 3 channel png file and checks metadata."""
        image_path = get_test_image_path(
            "gdal",
            "img_2448x2448_1x8bit_SRC_GRAY_ducks.png"
            )
        slide = slideio.open_slide(image_path, "GDAL")
        self.assertTrue(slide is not None)
        num_scenes = slide.num_scenes
        self.assertEqual(num_scenes, 1)
        self.assertEqual(image_path, slide.file_path)
        scene = slide.get_scene(0)
        self.assertTrue(scene is not None)
        self.assertEqual(image_path, scene.file_path)
        self.assertEqual(1, scene.num_channels)
        scene_rect = scene.rect
        self.assertEqual(0, scene_rect[0])
        self.assertEqual(0, scene_rect[1])
        self.assertEqual(2448, scene_rect[2])
        self.assertEqual(2448, scene_rect[3])
        for channel_index in range(scene.num_channels):
            channel_type = scene.get_channel_data_type(channel_index)
            self.assertEqual(channel_type, np.uint8)
            compression = scene.compression
            self.assertEqual(compression, slideio.Compression.Png)
        res = scene.resolution
        self.assertEqual(0, res[0])
        self.assertEqual(0, res[1])

    def test_3chnl_png16b_metadata(self):
        """Opens 3 channel 16 bit png file and checks metadata."""
        image_path = get_test_image_path(
            "gdal",
            "img_2448x2448_3x16bit_SRC_RGB_ducks.png"
            )
        slide = slideio.open_slide(image_path, "GDAL")
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
        self.assertEqual(2448, scene_rect[2])
        self.assertEqual(2448, scene_rect[3])
        for channel_index in range(scene.num_channels):
            channel_type = scene.get_channel_data_type(channel_index)
            self.assertEqual(channel_type, np.uint16)
            compression = scene.compression
            self.assertEqual(compression, slideio.Compression.Png)
        res = scene.resolution
        self.assertEqual(0, res[0])
        self.assertEqual(0, res[1])

    def test_3chnl_jpeg_metadata(self):
        """Opens 3 channel jpeg file and checks metadata."""
        image_path = get_test_image_path(
            "gdal",
            "Airbus_Pleiades_50cm_8bit_RGB_Yogyakarta.jpg"
            )
        slide = slideio.open_slide(image_path, "GDAL")
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
        self.assertEqual(5494, scene_rect[2])
        self.assertEqual(5839, scene_rect[3])
        for channel_index in range(scene.num_channels):
            channel_type = scene.get_channel_data_type(channel_index)
            self.assertEqual(channel_type, np.uint8)
            compression = scene.compression
            self.assertEqual(compression, slideio.Compression.Jpeg)
        res = scene.resolution
        self.assertEqual(0, res[0])
        self.assertEqual(0, res[1])

    def test_readblock_png8bit(self):
        """
        8 bit png image.

        Reads 8b png images and checks the raster.
        by calculation of raster statistics for
        specific rectangles
        """
        image_path = get_test_image_path(
            "gdal",
            "img_1024x600_3x8bit_RGB_color_bars_CMYKWRGB.png"
            )
        slide = slideio.open_slide(image_path, "GDAL")
        self.assertTrue(slide is not None)
        scene = slide.get_scene(0)
        block_rect = (260, 500, 100, 100)
        # read 3 channel block
        raster = scene.read_block(block_rect)
        mean, stddev = cv.meanStdDev(raster)
        self.assertEqual(mean[0], 255)
        self.assertEqual(stddev[0], 0)
        self.assertEqual(mean[1], 255)
        self.assertEqual(stddev[1], 0)
        self.assertEqual(mean[2], 0)
        self.assertEqual(stddev[2], 0)
        # read one channel block
        raster = scene.read_block(block_rect, channel_indices=[1])
        mean, stddev = cv.meanStdDev(raster)
        self.assertEqual(mean[0], 255)
        self.assertEqual(stddev[0], 0)

    def test_resampling_block_png8bit(self):
        """
        Resampling of a png image.

        Reads and resamples 8b png images and checks the raster.
        by calculation of raster statistics for
        specific rectangles
        """
        image_path = get_test_image_path(
            "gdal",
            "img_1024x600_3x8bit_RGB_color_bars_CMYKWRGB.png"
            )
        slide = slideio.open_slide(image_path, "GDAL")
        self.assertTrue(slide is not None)
        scene = slide.get_scene(0)
        block_rect = (260, 500, 100, 100)
        block_size = (12, 12)
        # read 3 channel block
        raster = scene.read_block(block_rect, size=block_size)
        mean, stddev = cv.meanStdDev(raster)
        self.assertEqual(mean[0], 255)
        self.assertEqual(stddev[0], 0)
        self.assertEqual(mean[1], 255)
        self.assertEqual(stddev[1], 0)
        self.assertEqual(mean[2], 0)
        self.assertEqual(stddev[2], 0)
        # read one channel block
        raster = scene.read_block(
            block_rect,
            size=block_size,
            channel_indices=[1]
            )
        mean, stddev = cv.meanStdDev(raster)
        self.assertEqual(mean[0], 255)
        self.assertEqual(stddev[0], 0)


if __name__ == '__main__':
    unittest.main()
