// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#ifndef OPENCV_slideio_cziauximage_HPP
#define OPENCV_slideio_cziauximage_HPP

#include <cstdint>
#include <fstream>
#include <string>

#include "slideio/slideio_def.hpp"
#include "slideio/core/cvsmallscene.hpp"

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif

namespace slideio
{
    class CZISlide;
    class SLIDEIO_EXPORTS CZIAuxImage : public CVSmallScene
    {
    public:
        enum class Type
        {
            Unknown = 0,
            JPG = 1,
            PNG = 2,
            CZI = 3
        };
        CZIAuxImage(){}
        ~CZIAuxImage(){}
        void setAttachmentData(CZISlide* slide, Type type, int64_t position, int32_t size, const std::string& name);
        static CZIAuxImage::Type typeFromString(const std::string& typeName);
    protected:
        int64_t m_dataPos{0};
        int32_t m_dataSize{0};
        Type m_type{Type::Unknown};
        CZISlide* m_slide{nullptr};
        std::shared_ptr<CVSmallScene> m_scene;
    };

}


#if defined(_MSC_VER)
#pragma warning( pop )
#endif

#endif
