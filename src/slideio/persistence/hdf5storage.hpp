// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.org/license.html.
#pragma once
#include "slideio_persistence_def.hpp"
#include "storage.hpp"
#include <opencv2/core.hpp>
#include <string>
#include <hdf5.h>

namespace slideio
{
   class SLIDEIO_PERSISTENCE_EXPORTS HDF5Storage : public Storage
   {
   public:
      HDF5Storage();
      virtual ~HDF5Storage() override;
      void open(std::string filePath);
      void create(std::string filePath, cv::Size imageSize, cv::Size tileSize);
   public:
      static std::shared_ptr<HDF5Storage> openStorage(const std::string& filePath);
      static std::shared_ptr<HDF5Storage> createStorage(const std::string& filePath, cv::Size imageSize, cv::Size tileSize= cv::Size(512,512));
      virtual void closeStorage() override;
   private:
      std::string m_filePath;
      cv::Size m_imageSize;
      cv::Size m_tileSize;
      hid_t m_fileId;
      hid_t m_dataspace_id;
      hid_t m_dataset_id;
   };
}
