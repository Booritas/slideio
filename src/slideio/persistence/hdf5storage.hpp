// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.org/license.html.
#pragma once
#include "slideio_persistence_def.hpp"
#include "storage.hpp"
#include <opencv2/core.hpp>
#include <string>

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif

namespace H5
{
    class DataSet;
    class DataSpace;
    class Group;
    class H5File;
}

namespace slideio
{
    class SLIDEIO_PERSISTENCE_EXPORTS HDF5Storage : public Storage
    {
    public:
        HDF5Storage();
        virtual ~HDF5Storage() override;
        void open(const std::string& filePath);
        void create(const std::string& filePath, const cv::Size& imageSize, const cv::Size& tileSize);

    public:
        static std::shared_ptr<HDF5Storage> openStorage(const std::string& filePath);
        static std::shared_ptr<HDF5Storage> createStorage(const std::string& filePath, cv::Size imageSize,
                                                          cv::Size tileSize = cv::Size(512, 512));
        virtual void closeStorage() override;
        void writeTile(const cv::Mat& tile, const cv::Point& offset) override;
        void readTile(const cv::Point& offset, const cv::Size& size, cv::OutputArray mat) override;

    private:
        std::string m_filePath;
        cv::Size m_imageSize;
        cv::Size m_tileSize;
        std::shared_ptr<H5::H5File> m_file;
        std::shared_ptr<H5::Group> m_group;
        std::shared_ptr<H5::DataSpace> m_dataspace;
        std::shared_ptr<H5::DataSet> m_dataset;
    };
}

#if defined(_MSC_VER)
#pragma warning( pop )
#endif
