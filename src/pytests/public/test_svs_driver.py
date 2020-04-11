"""slideio CZI driver testing."""

import unittest
import pytest
import cv2 as cv
import numpy as np
import slideio
from testlib import get_test_image_path


class TestSVS(unittest.TestCase):
    """Tests for slideio CZI driver functionality."""

    def test_not_existing_file(self):
        """
        Opening of not existing image.

        Checks if slideio throws RuntimeError
        exception during opening of not existing file.
        """
        image_path = "missing_file.png"
        with pytest.raises(RuntimeError):
            slideio.open_slide(image_path, "SVS")

    def test_file_j2k_metadata(self):
        """
        Checks image metadata.

        Opens 3 channel Jpeg 2000 commpressed file
        and checks metadata.
        """
        image_path = get_test_image_path(
            "svs",
            "JP2K-33003-1.svs"
            )
        slide = slideio.open_slide(image_path, "SVS")
        self.assertTrue(slide is not None)
        num_scenes = slide.num_scenes
        self.assertEqual(num_scenes, 4)
        self.assertEqual(image_path, slide.file_path)

        raw_metadata = slide.raw_metadata
        self.assertTrue(raw_metadata.startswith("Aperio Image Library"))

        # test scene metadates
        scene_names = ["Image", "Thumbnail", "Label", "Macro"]
        compression_types = [
            slideio.Jpeg2000,
            slideio.Jpeg,
            slideio.LempelZivWelch,
            slideio.Jpeg
            ]
        scene_sizes = [
            (15374, 17497),
            (674, 768),
            (415, 422),
            (1280, 421)
            ]
        scene_magnifications = [40., 40., 0., 0.]

        for scene_index in range(num_scenes):
            scene = slide.get_scene(scene_index)
            self.assertTrue(scene is not None)
            scene_rect = scene.rect
            scene_size = (scene_rect[2], scene_rect[3])
            self.assertEqual(scene.name, scene_names[scene_index])
            self.assertEqual(scene.compression, compression_types[scene_index])
            self.assertEqual(scene.num_channels, 3)
            self.assertEqual(scene_size, scene_sizes[scene_index])
            self.assertEqual(image_path, scene.file_path)
            self.assertEqual(
                scene_magnifications[scene_index],
                scene.magnification
            )
            for channel_index in range(scene.num_channels):
                channel_type = scene.get_channel_data_type(channel_index)
                self.assertEqual(channel_type, np.uint8)

        # test scene resolutuion
        scene_resolutions = [0.2498e-6, 0, 0, 0]
        for scene_index in range(num_scenes):
            scene = slide.get_scene(scene_index)
            self.assertTrue(scene is not None)
            res = scene.resolution
            self.assertEqual(
                scene_resolutions[scene_index],
                pytest.approx(res[0])
                )
            self.assertEqual(
                scene_resolutions[scene_index],
                pytest.approx(res[1])
                )

    def test_file_rgb_metadata(self):
        """
        Checks image metadata.

        Opens 3 channel brightfield Jpeg commpressed file
        and checks metadata.
        """
        image_path = get_test_image_path(
            "svs",
            "CMU-1-Small-Region.svs"
            )
        slide = slideio.open_slide(image_path, "SVS")
        self.assertTrue(slide is not None)
        num_scenes = slide.num_scenes
        self.assertEqual(num_scenes, 4)
        self.assertEqual(image_path, slide.file_path)

        raw_metadata = slide.raw_metadata
        self.assertTrue(raw_metadata.startswith("Aperio Image Library"))

        # test scene metadates
        scene_names = ["Image", "Thumbnail", "Label", "Macro"]
        compression_types = [
            slideio.Jpeg,
            slideio.Jpeg,
            slideio.LempelZivWelch,
            slideio.Jpeg
            ]
        scene_sizes = [
            (2220, 2967),
            (574, 768),
            (387, 463),
            (1280, 431)
            ]
        scene_magnifications = [20., 20., 0., 0.]

        for scene_index in range(num_scenes):
            scene = slide.get_scene(scene_index)
            self.assertTrue(scene is not None)
            scene_rect = scene.rect
            scene_size = (scene_rect[2], scene_rect[3])
            self.assertEqual(scene.name, scene_names[scene_index])
            self.assertEqual(scene.compression, compression_types[scene_index])
            self.assertEqual(scene.num_channels, 3)
            self.assertEqual(scene_size, scene_sizes[scene_index])
            self.assertEqual(image_path, scene.file_path)
            self.assertEqual(
                scene_magnifications[scene_index],
                scene.magnification
            )
            for channel_index in range(scene.num_channels):
                channel_type = scene.get_channel_data_type(channel_index)
                self.assertEqual(channel_type, np.uint8)

        # test scene resolutuion
        scene_resolutions = [0.4990e-6, 0, 0, 0]
        for scene_index in range(num_scenes):
            scene = slide.get_scene(scene_index)
            self.assertTrue(scene is not None)
            res = scene.resolution
            self.assertEqual(
                scene_resolutions[scene_index],
                pytest.approx(res[0])
                )
            self.assertEqual(
                scene_resolutions[scene_index],
                pytest.approx(res[1])
                )

    def test_file_rgb_metadata_pictures(self):
        """
        Checks image metadata pictures.

        Opens 3 channel brightfield Jpeg commpressed file
        and checks metadata pictures agains extracted by
        a 3rd party tools pictures.
        """
        image_path = get_test_image_path(
            "svs",
            "CMU-1-Small-Region.svs"
            )
        metadata_pic_paths = [
            get_test_image_path(
                "svs",
                "CMU-1-Small-Region-page-0.tif"
            ),
            get_test_image_path(
                "svs",
                "CMU-1-Small-Region-page-1.tif"
            ),
            get_test_image_path(
                "svs",
                "CMU-1-Small-Region-page-2.tif"
            ),
            get_test_image_path(
                "svs",
                "CMU-1-Small-Region-page-3.tif"
            ),
        ]
        slide = slideio.open_slide(image_path, "SVS")
        self.assertTrue(slide is not None)
        num_scenes = slide.num_scenes
        self.assertEqual(num_scenes, 4)
        for scene_index in range(1, num_scenes):
            scene = slide.get_scene(scene_index)
            self.assertTrue(scene is not None)
            scene_image = scene.read_block(channel_indices=[2, 1, 0])
            reference_image = cv.imread(
                metadata_pic_paths[scene_index],
                cv.IMREAD_UNCHANGED
                )
            self.assertEqual(scene_image.shape, reference_image.shape)
            
    def test_file_rgb_image(self):
        """
        Checks image raster pictures.

        Opens 3 channel brightfield Jpeg commpressed file
        and checks raster agains extracted by
        a 3rd party tools pictures.
        """
        image_path = get_test_image_path(
            "svs",
            "CMU-1-Small-Region.svs"
            )
        reference_image_path = get_test_image_path(
                "svs",
                "CMU-1-Small-Region-page-0.tif"
            )
        slide = slideio.open_slide(image_path, "SVS")
        self.assertTrue(slide is not None)
        num_scenes = slide.num_scenes
        self.assertEqual(num_scenes, 4)
        scene = slide.get_scene(0)
        self.assertTrue(scene is not None)
        scene_image = scene.read_block()
        reference_image = cv.imread(
            reference_image_path,
            cv.IMREAD_UNCHANGED
            )
        reference_image = cv.cvtColor(reference_image, cv.COLOR_BGR2RGB)
        self.assertEqual(scene_image.shape, reference_image.shape)
        self.assertTrue(np.array_equal(scene_image, reference_image))

    def test_file_rgb_image_region(self):
        """
        Checks reading of an image region.

        Opens 3 channel brightfield Jpeg commpressed file
        and checks raster agains extracted by
        a 3rd party tools pictures.
        """
        image_path = get_test_image_path(
            "svs",
            "CMU-1-Small-Region.svs"
            )
        reference_image_path = get_test_image_path(
                "svs",
                "CMU-1-Small-Region-page-0.tif"
            )
        slide = slideio.open_slide(image_path, "SVS")
        self.assertTrue(slide is not None)
        num_scenes = slide.num_scenes
        self.assertEqual(num_scenes, 4)
        scene = slide.get_scene(0)
        self.assertTrue(scene is not None)
        xn = 500
        yn = 500
        width = 600
        height = 300
        rect = (xn, yn, width, height)
        scene_image = scene.read_block(rect)
        reference_image = cv.imread(
            reference_image_path,
            cv.IMREAD_UNCHANGED
            )
        reference_image = cv.cvtColor(reference_image, cv.COLOR_BGR2RGB)
        xe = xn + width
        ye = yn + height
        reference_region = reference_image[yn:ye, xn:xe]
        self.assertEqual(scene_image.shape, reference_region.shape)
        self.assertTrue(np.array_equal(scene_image, reference_region))

    def test_file_rgb_image_region_resizing(self):
        """
        Checks reading of an image region with resizing.

        Opens 3 channel brightfield Jpeg commpressed file
        and checks raster agains extracted by
        a 3rd party tools pictures.
        """
        image_path = get_test_image_path(
            "svs",
            "CMU-1-Small-Region.svs"
            )
        reference_image_path = get_test_image_path(
                "svs",
                "CMU-1-Small-Region-page-0.tif"
            )
        slide = slideio.open_slide(image_path, "SVS")
        self.assertTrue(slide is not None)
        num_scenes = slide.num_scenes
        self.assertEqual(num_scenes, 4)
        scene = slide.get_scene(0)
        self.assertTrue(scene is not None)
        xn = 500
        yn = 500
        width = 600
        height = 300
        params = [
            ((400, 200), (0.0011, 0.98)),
            ((200, 100), (0.012, 0.83))
        ]
        rect = (xn, yn, width, height)
        xe = xn + width
        ye = yn + height
        reference_image = cv.imread(
            reference_image_path,
            cv.IMREAD_UNCHANGED
            )
        reference_image = cv.cvtColor(reference_image, cv.COLOR_BGR2RGB)
        reference_image = reference_image[yn:ye, xn:xe]
        for param in params:
            size = param[0]
            cf = param[1][1]
            sq = param[1][0]
            scene_image = scene.read_block(rect, size=size)
            reference_region = cv.resize(reference_image, size)
            self.assertEqual(scene_image.shape, reference_region.shape)
            score_sq = cv.matchTemplate(
                scene_image,
                reference_region,
                cv.TM_SQDIFF_NORMED
                )[0][0]
            score_cf = cv.matchTemplate(
                scene_image,
                reference_region,
                cv.TM_CCOEFF_NORMED
                )[0][0]
            self.assertLess(score_sq, sq)
            self.assertLess(cf, score_cf)

    def test_file_rgb_image_channel_swap(self):
        """
        Checks image raster pictures.

        Reads 3 channel brightfield Jpeg commpressed file
        with channel swaping and checks raster against 
        extracted by a 3rd party tools pictures.
        """
        image_path = get_test_image_path(
            "svs",
            "CMU-1-Small-Region.svs"
            )
        reference_image_path = get_test_image_path(
            "svs",
            "CMU-1-Small-Region-page-0.tif"
            )
        slide = slideio.open_slide(image_path, "SVS")
        self.assertTrue(slide is not None)
        num_scenes = slide.num_scenes
        self.assertEqual(num_scenes, 4)
        scene = slide.get_scene(0)
        self.assertTrue(scene is not None)
        scene_image = scene.read_block(
            channel_indices=[2, 1, 0]
            )
        reference_image = cv.imread(
            reference_image_path,
            cv.IMREAD_UNCHANGED
            )
        self.assertEqual(scene_image.shape, reference_image.shape)
        self.assertTrue(np.array_equal(scene_image, reference_image))


if __name__ == '__main__':
    unittest.main()
