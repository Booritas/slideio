// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/mrxs/mrxsslide.hpp"
#include "slideio/drivers/mrxs/mrxsdataset.hpp"
#include "slideio/drivers/mrxs/mrxsscene.hpp"
#include "slideio/drivers/mrxs/mrxsauxscene.hpp"
#include "slideio/drivers/mrxs/mrxsdescription.hpp"
#include "slideio/core/exceptions.hpp"

std::shared_ptr<slideio::MRXSSlide> slideio::MRXSSlide::openFile(const std::string& filePath)
{
    auto slide = std::make_shared<MRXSSlide>();
    slide->m_filePath = filePath;
    slide->m_dataset = mrxs::MRXSDataset::open(filePath);
    slide->m_scene = std::make_shared<MRXSScene>(slide->m_dataset);
    slide->m_rawMetadata = slide->m_dataset->structure.ini.text();
    slide->m_metadataFormat = MetadataFormat::Text;
    // OpenSlide's names, in its order: SlideThumbnail is the macro, SlidePreview the thumbnail.
    for (const char* name : {"Macro", "Label", "Thumbnail"}) {
        const auto& associated = slide->m_dataset->structure.associated;
        const auto it = associated.find(name);
        if (it != associated.end()) {
            slide->m_auxImages[name] = std::make_shared<MRXSAuxScene>(slide->m_dataset, name, it->second);
            slide->m_auxNames.push_back(name);
        }
    }
    return slide;
}

std::shared_ptr<slideio::CVScene> slideio::MRXSSlide::getAuxImage(const std::string& name) const
{
    const auto it = m_auxImages.find(name);
    if (it == m_auxImages.end()) {
        RAISE_RUNTIME_ERROR << "MRXS: the slide has no associated image '" << name << "'";
    }
    return it->second;
}

slideio::MetadataBuilder slideio::MRXSSlide::buildMetadataTree() const
{
    return mrxs::describe(*m_dataset);
}

std::shared_ptr<slideio::CVScene> slideio::MRXSSlide::getScene(int index) const
{
    if (index != 0) {
        RAISE_RUNTIME_ERROR << "MRXS: invalid scene index " << index << "; the slide has one scene";
    }
    return m_scene;
}
