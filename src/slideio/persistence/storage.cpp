// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.org/license.html.
#include "storage.hpp"
#include "hdf5storage.hpp"

using namespace slideio;

std::shared_ptr<Storage> Storage::openStorage(const std::string& filePath) {
    return HDF5Storage::openStorage(filePath);
}

std::shared_ptr<Storage> Storage::createStorage(const std::string& filePath, cv::Size imageSize, cv::Size tileSize) {
    return HDF5Storage::createStorage(filePath, imageSize, tileSize);
}
