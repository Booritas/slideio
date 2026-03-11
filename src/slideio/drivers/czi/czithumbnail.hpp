// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#ifndef OPENCV_slideio_czithumbnail_HPP
#define OPENCV_slideio_czithumbnail_HPP


#include "slideio/drivers/czi/czi_api_def.hpp"
#include "slideio/core/cvsmallscene.hpp"

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif

namespace slideio
{
    class CZISlide;

    class SLIDEIO_CZI_EXPORTS CZIThumbnail : public CVSmallScene
    {
    public:
        CZIThumbnail(const std::string& driverId) : m_driverId(driverId) {
        }
        bool init() override;
        void setAttachmentData(CZISlide* slide, int64_t position, int64_t size, const std::string& name);
        int getSceneIndex() const override {
            return -1;
		}
        const std::string& getDriverId() const override {
            return m_driverId;
        }
    protected:
        void readImage(cv::OutputArray output) override;
    private:
        int64_t m_dataPos{ 0 };
        int64_t m_dataSize{ 0 };
        CZISlide* m_slide{ nullptr };
        std::shared_ptr<CVSmallScene> m_scene;
        std::string m_driverId;
    };
};

#if defined(_MSC_VER)
#pragma warning( pop )
#endif

#endif
