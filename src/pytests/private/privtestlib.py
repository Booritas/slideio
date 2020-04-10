"""Auxilary functions for testing."""

import os


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
