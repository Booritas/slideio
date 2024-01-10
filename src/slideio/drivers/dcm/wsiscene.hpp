// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

#include "slideio/drivers/dcm/dcm_api_def.hpp"
#include "slideio/core/cvscene.hpp"
#include "slideio/drivers/dcm/dcmfile.hpp"

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif

namespace slideio
{
    class DCMSlide;
    class SLIDEIO_DCM_EXPORTS WSIScene : public CVScene
    {
    public:
        WSIScene();
        void addFile(std::shared_ptr<DCMFile>& file);
        void init();
        std::string getFilePath() const override;
        std::string getName() const override;
        cv::Rect getRect() const override;
        int getNumChannels() const override;
        slideio::DataType getChannelDataType(int channel) const override;
        Resolution getResolution() const override;
        double getMagnification() const override;
        Compression getCompression() const override;
    private:
        std::vector<std::shared_ptr<DCMFile>> m_files;
    };
};