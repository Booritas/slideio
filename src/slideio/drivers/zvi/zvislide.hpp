// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#ifndef OPENCV_slideio_zvislide_HPP
#define OPENCV_slideio_zvislide_HPP
#include "slideio/core/cvslide.hpp"
#include "slideio/drivers/zvi/zviscene.hpp"
#include <fstream>

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif

namespace slideio
{
    class SLIDEIO_EXPORTS ZVISlide : public CVSlide
    {
    public:
        ZVISlide(const std::string& filePath);
        int getNumScenes() const override;
        std::string getFilePath() const override;
        std::shared_ptr<CVScene> getScene(int index) const override;
        double getMagnification() const;
        Resolution getResolution() const;
        double getZSliceResolution() const;
        double getTFrameResolution() const;
    private:
        void init();
    private:
        std::string m_filePath;
    };
}

#if defined(_MSC_VER)
#pragma warning( pop )
#endif

#endif
