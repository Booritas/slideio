// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

#include "slideio/drivers/mrxs/mrxs_api_def.hpp"
#include "slideio/core/cvslide.hpp"
#include <memory>
#include <string>

namespace slideio
{
    class MRXSScene;

    class SLIDEIO_MRXS_EXPORTS MRXSSlide : public CVSlide
    {
    public:
        /**@brief opens the bundle an entry path names (UTF-8).*/
        static std::shared_ptr<MRXSSlide> openFile(const std::string& filePath);

        int getNumScenes() const override { return 1; }
        std::string getFilePath() const override { return m_filePath; }
        std::shared_ptr<CVScene> getScene(int index) const override;

    private:
        std::string m_filePath;
        std::shared_ptr<MRXSScene> m_scene;
    };
}
