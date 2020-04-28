"""Auxilary functions for testing."""

import os
import cv2 as cv
import slideio


def get_priv_test_image_path(folder, image_name):
    """
    Return a path to a test image.

    The function returns path to a test image by
    specifying folder and image name.
    Parameters:
        folder: a string that defines image subfolder
                in the folder of test images.
        image_name: image file name, include file extension.
    """
    root_folder = os.environ['SLIDEIO_TEST_DATA_PRIV_PATH']
    if root_folder is None:
        raise "Environment variable SLIDEIO_TEST_DATA_PRIV_PATH is not set"
    image_path = os.path.join(root_folder, folder, image_name)
    return image_path


def generate_priv_regression_image_name(image_name, scene, channel, rect, ext):
    """
    Generate a name for a regression test image.

    Generated name is based on the original image name,
    channel, scene and region rectangle.
    """
    name = os.path.splitext(image_name)[0]
    x = rect[0]
    y = rect[1]
    w = rect[2]
    h = rect[3]
    return f"{name}/{name}_s{scene}_c{channel}_x{x}_y{y}_w{w}_h{h}{ext}"


def get_priv_regression_image_path(folder, image_name,
                                   scene, channel, rect, ext):
    """
    Return path for a regression test image.

    Generated name is based on the original image name,
    channel, scene and region rectangle. The image is
    located in a subfoldler of the original image folder
    with name of the original image (without extension)
    """
    return get_priv_test_image_path(
        folder,
        generate_priv_regression_image_name(
            image_name, scene, channel, rect, ext
            )
        )


def export_priv_image_region(folder, image_name, driver,
                             scene_index, rect, size, ext):
    """
    Export a region of an image for regression tests.

    The function extracts region from an image scene
    and saves multiple images channel by channel
    to a folder with the same name for a regression test.
    """
    image_path = get_priv_test_image_path(folder, image_name)
    slide = slideio.open_slide(image_path, driver)
    scene = slide.get_scene(scene_index)
    num_channels = scene.num_channels
    for channel_index in range(num_channels):
        image_raster = scene.read_block(
            rect, size=size,
            channel_indices=[channel_index]
            )
        regr_image_name = generate_priv_regression_image_name(
            image_name, scene_index, channel_index, rect, ext
            )
        regr_image_path = get_priv_test_image_path(folder, regr_image_name)
        # check if we have to create a folder
        dir_path = os.path.split(regr_image_path)[0]
        if not os.path.exists(dir_path):
            os.makedirs(dir_path)
        # save output image
        cv.imwrite(regr_image_path, image_raster)
