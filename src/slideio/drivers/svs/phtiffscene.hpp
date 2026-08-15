// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "slideio/drivers/svs/svstiledscene.hpp"
#include "slideio/drivers/svs/phtmetadata.hpp"

namespace slideio
{
    // The whole slide image of a philips tiff. Everything but the reading of the philips
    // metadata is the shared tiff behaviour of SVSTiledScene.
    class SLIDEIO_SVS_EXPORTS PHTIFFTiledScene : public SVSTiledScene
    {
    public:
        static std::shared_ptr<PHTIFFTiledScene> create(const std::string& filePath,
            libtiff::TIFF* hFile, const std::string& name,
            const std::vector<slideio::TiffDirectory>& dirs, const PHTMetadata& metadata);
    protected:
        PHTIFFTiledScene(const std::string& filePath, libtiff::TIFF* hFile,
            const std::string& name, const std::vector<slideio::TiffDirectory>& dirs,
            const PHTMetadata& metadata);
        void processImageDescription() override;
    private:
        // A copy, not a reference: the metadata is parsed once by PHTIFFSlide::init into a
        // local variable, and this scene outlives that call, so a reference would dangle.
        PHTMetadata m_metadata;
    };
}
