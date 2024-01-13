// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#ifndef OPENCV_slideio_dcmslide_HPP
#define OPENCV_slideio_dcmslide_HPP

#include "slideio/drivers/dcm/dcm_api_def.hpp"
#include "slideio/core/cvslide.hpp"
#include "slideio/drivers/dcm/dcmscene.hpp"
#include <fstream>

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif

namespace slideio
{
    class SLIDEIO_DCM_EXPORTS DCMSlide : public CVSlide
    {
        friend class DCMImageDriver;
    protected:
        DCMSlide(const std::string& filePath);
    public:
        int getNumScenes() const override;
        std::string getFilePath() const override;
        std::shared_ptr<CVScene> getScene(int index) const override;
        void initFromFile();
        void processRegularSeries(std::vector<std::shared_ptr<DCMFile>>& files, bool keepOrder);

    private:
        void processWSISeries(std::vector<std::shared_ptr<DCMFile>>& dcmFiles);
        void processSeries(std::vector<std::shared_ptr<DCMFile>>& files, bool keepOrder=false);
        void initFromDir();
        void initFromDicomDirFile();
        void init();
        void initFromWSIFile();
        void initFromRegularDicomFile();
    private:
        std::vector<std::shared_ptr<CVScene>> m_scenes;
        std::string m_srcPath;
    };
}

#if defined(_MSC_VER)
#pragma warning( pop )
#endif

#endif
