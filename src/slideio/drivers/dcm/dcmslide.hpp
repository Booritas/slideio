// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#ifndef OPENCV_slideio_dcmslide_HPP
#define OPENCV_slideio_dcmslide_HPP
#include "slideio/core/cvslide.hpp"
#include "slideio/drivers/dcm/dcmscene.hpp"
#include <fstream>

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif

namespace slideio
{
    class SLIDEIO_EXPORTS DCMSlide : public CVSlide
    {
    public:
        DCMSlide(const std::string& filePath);
        int getNumScenes() const override;
        std::string getFilePath() const override;
        std::shared_ptr<CVScene> getScene(int index) const override;
        void initFromFile();
    private:
        void processSeries(std::vector<std::shared_ptr<DCMFile>>& files);
        void initFromDir();
        void init();
    private:
        std::vector<std::shared_ptr<DCMScene>> m_scenes;
        std::string m_srcPath;
    };
}

#if defined(_MSC_VER)
#pragma warning( pop )
#endif

#endif
