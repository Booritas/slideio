import os

def get_test_image_path(folder, image):
    root_path = os.getenv("SLIDEIO_TEST_DATA_PATH")
    return os.path.join(root_path, folder, image)
