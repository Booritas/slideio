// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.org/license.html.
#pragma once
#include "slideio_persistence_def.hpp"
#include <opencv2/core.hpp>
#include <string>
#include <memory>

namespace slideio
{
    class SLIDEIO_PERSISTENCE_EXPORTS Storage
    {
    protected:
        Storage(){}
        virtual ~Storage(){}
    public:
        static std::shared_ptr<Storage> openStorage(const std::string& filePath);
        static std::shared_ptr<Storage> createStorage(const std::string& filePath, cv::Size imageSize, cv::Size tileSize= cv::Size(512,512));
        virtual void closeStorage() = 0;
    };
}
