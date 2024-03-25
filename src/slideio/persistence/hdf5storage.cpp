// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.org/license.html.

#include "hdf5storage.hpp"
#include "slideio/core/tools/tools.hpp"
#include "slideio/base/exceptions.hpp"
#include <hdf5.h>
using namespace slideio;

std::shared_ptr<HDF5Storage> HDF5Storage::openStorage(const std::string &filePath) 
{
   auto storage = std::make_shared<HDF5Storage>();
   storage->open(filePath);
   return storage;
}

std::shared_ptr <HDF5Storage> HDF5Storage::createStorage(const std::string &filePath, cv::Size imageSize, cv::Size tileSize) {
    auto storage = std::make_shared<HDF5Storage>();
    storage->create(filePath, imageSize, tileSize);
    return storage;
}

slideio::HDF5Storage::HDF5Storage() : m_fileId(-1), m_dataspace_id(-1), m_dataset_id(-1)
{
}

slideio::HDF5Storage::~HDF5Storage()
{
   closeStorage();
}

void slideio::HDF5Storage::open(std::string filePath)
{
   Tools::throwIfPathNotExist(filePath, "HDF5Storage::open");
}

void slideio::HDF5Storage::create(std::string filePath, cv::Size imageSize, cv::Size tileSize)
{
   Tools::throwIfPathExists(filePath, "HDF5Storage::create");
   hid_t file_id = H5Fcreate(filePath.c_str(), H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);
   if(file_id < 0) {
      RAISE_RUNTIME_ERROR << "HDF5Storage::create: Can't create file " << filePath;
   }

   hsize_t dims[2] = {(hsize_t)imageSize.height, (hsize_t)imageSize.width};
   hid_t dataspace_id = H5Screate_simple(2, dims, NULL);
   if(dataspace_id < 0) {
      RAISE_RUNTIME_ERROR << "HDF5Storage::create: Can't create dataspace for file " << filePath;
   }

   hid_t dataset_id = H5Dcreate2(file_id, "/dataset", H5T_NATIVE_INT, dataspace_id, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
   if (dataset_id < 0) {
      RAISE_RUNTIME_ERROR << "HDF5Storage::create: Can't create dataset for file " << filePath;
    }
}


void HDF5Storage::closeStorage() {
   if(m_dataset_id>=0) {
      H5Dclose(m_dataset_id);
      m_dataset_id = -1;
   }
   if(m_dataspace_id>=0) {
      H5Sclose(m_dataspace_id);
      m_dataspace_id = -1;
   }
   if(m_fileId>=0) {
      H5Fclose(m_fileId);
      m_fileId = -1;
   }
}
