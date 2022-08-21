// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#ifndef OPENCV_slideio_slide_HPP
#define OPENCV_slideio_slide_HPP

#include "slideio/core/slideio_core_def.hpp"
#include "slideio/core/cvscene.hpp"
#include <string>

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif

namespace slideio
{
    class SLIDEIO_CORE_EXPORTS CVSlide
    {
    public:
        virtual ~CVSlide(){}
        virtual int getNumScenes() const = 0;
        virtual std::string getFilePath() const = 0;
        virtual const std::string& getRawMetadata() const {return m_rawMetadata;}
        virtual std::shared_ptr<CVScene> getScene(int index) const = 0;
        virtual const std::list<std::string>& getAuxImageNames() const {
            return m_auxNames;
        }
        virtual int getNumAuxImages() const {
            return static_cast<int>(m_auxNames.size());
        }
        virtual std::shared_ptr<CVScene> getAuxImage(const std::string& sceneName) const;
    protected:
        std::string m_rawMetadata;
        std::list<std::string> m_auxNames;
    };
}

#endif