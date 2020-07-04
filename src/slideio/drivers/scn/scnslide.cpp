// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/scn/scnslide.hpp"
#include "slideio/imagetools/imagetools.hpp"

#include <boost/filesystem.hpp>

#include "slideio/imagetools/tifftools.hpp"


using namespace slideio;

SCNSlide::SCNSlide()
{
}

SCNSlide::~SCNSlide()
{
}

int SCNSlide::getNumScenes() const
{
    return (int)m_Scenes.size();
}

std::string SCNSlide::getFilePath() const
{
    return m_filePath;
}

std::shared_ptr<CVScene> SCNSlide::getScene(int index) const
{
    if(index>=getNumScenes())
        throw std::runtime_error("SCN driver: invalid m_scene index");
    return m_Scenes[index];
}

std::shared_ptr<SCNSlide> SCNSlide::openFile(const std::string& filePath)
{
    namespace fs = boost::filesystem;
    std::shared_ptr<SCNSlide> slide;
    if(!fs::exists(filePath)){
        throw std::runtime_error(std::string("SCNImageDriver: File does not exist:") + filePath);
    }
    std::vector<TiffDirectory> directories;
    TIFF* tiff(nullptr);
    tiff = TIFFOpen(filePath.c_str(), "r");
    if(!tiff)
        return slide;
    TIFFKeeper keeper(tiff);

    TiffTools::scanFile(tiff, directories);

    return slide;
}
