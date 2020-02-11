// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#ifndef OPENCV_slideio_svsscene_HPP
#define OPENCV_slideio_svsscene_HPP

#include "slideio/slideio_def.hpp"
#include "slideio/core/scene.hpp"

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif

namespace slideio
{
    class SLIDEIO_EXPORTS SVSScene : public Scene
    {
    public:
        SVSScene(const std::string& filePath, const std::string& name) :
            m_filePath(filePath),
            m_name(name) {
        }
        std::string getFilePath() const override {
            return m_filePath;
        }
        std::string getName() const override {
            return m_name;
        }
    protected:
        std::string m_filePath;
        std::string m_name;
    };
}

#if defined(_MSC_VER)
#pragma warning( pop )
#endif

#endif
