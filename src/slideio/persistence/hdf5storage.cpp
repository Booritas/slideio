// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.org/license.html.

#include "hdf5storage.hpp"
#include "slideio/core/tools/tools.hpp"
#include "slideio/base/exceptions.hpp"
#include <H5Cpp.h>

using namespace slideio;

std::shared_ptr<HDF5Storage> HDF5Storage::openStorage(const std::string& filePath) {
    auto storage = std::make_shared<HDF5Storage>();
    storage->open(filePath);
    return storage;
}

std::shared_ptr<HDF5Storage> HDF5Storage::createStorage(const std::string& filePath, cv::Size imageSize,
                                                        cv::Size tileSize) {
    auto storage = std::make_shared<HDF5Storage>();
    storage->create(filePath, imageSize, tileSize);
    return storage;
}

HDF5Storage::HDF5Storage() {
}

HDF5Storage::~HDF5Storage() {
    closeStorage();
}

void HDF5Storage::open(const std::string& filePath) {
    Tools::throwIfPathNotExist(filePath, "HDF5Storage::open");
}

void HDF5Storage::create(const std::string& filePath, const cv::Size& imageSize, const cv::Size& tileSize) {
    Tools::throwIfPathExists(filePath, "HDF5Storage::create");

    m_file.reset(new H5::H5File(filePath, H5F_ACC_TRUNC));
    const hsize_t dims[2] = { static_cast<hsize_t>(imageSize.height), static_cast<hsize_t>(imageSize.width) };
    const hsize_t chunkDims[2] = { static_cast<hsize_t>(tileSize.height), static_cast<hsize_t>(tileSize.width) };

    m_group.reset(new H5::Group(m_file->createGroup("/Level")));

    H5::DataSpace dataSpace(2, dims);
    H5::DSetCreatPropList creatPropList;
    creatPropList.setChunk(2, chunkDims);
    creatPropList.setDeflate(6);
    m_dataset.reset(new H5::DataSet(m_file->createDataSet("/Level/Data",H5::PredType::NATIVE_INT32, dataSpace)));
}


void HDF5Storage::closeStorage() {
    m_dataset.reset();
    //m_dataspace.reset();
    m_group.reset();
    m_file.reset();
}

void HDF5Storage::writeTile(const cv::Mat& tile, const cv::Point& offset) {
}

void HDF5Storage::readTile(const cv::Point& offset, const cv::Size& size, cv::OutputArray mat) {
}
