// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

#include "slideio/slideio_def.hpp"
#include <string>
#include <memory>
#include "scene.hpp"

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif


namespace slideio
{
    class CVSlide;
    class SLIDEIO_EXPORTS Slide
    {
    public:
        Slide(std::shared_ptr<CVSlide> slide);
        ~Slide();
        int getNumbScenes() const;
        std::string getFilePath() const;
        std::shared_ptr<Scene> getScene(int index) const;
    private:
        std::shared_ptr<CVSlide> m_slide;
    };
}


#if defined(_MSC_VER)
#pragma warning( pop )
#endif
