// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/mrxs/mrxsslide.hpp"
#include "slideio/drivers/mrxs/mrxsdataset.hpp"
#include "slideio/drivers/mrxs/mrxsscene.hpp"
#include "slideio/core/exceptions.hpp"

std::shared_ptr<slideio::MRXSSlide> slideio::MRXSSlide::openFile(const std::string& filePath)
{
    auto slide = std::make_shared<MRXSSlide>();
    slide->m_filePath = filePath;
    slide->m_scene = std::make_shared<MRXSScene>(mrxs::MRXSDataset::open(filePath));
    return slide;
}

std::shared_ptr<slideio::CVScene> slideio::MRXSSlide::getScene(int index) const
{
    if (index != 0) {
        RAISE_RUNTIME_ERROR << "MRXS: invalid scene index " << index << "; the slide has one scene";
    }
    return m_scene;
}
