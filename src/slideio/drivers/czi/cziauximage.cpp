#include "slideio/drivers/czi/cziauximage.hpp"

void slideio::CZIAuxImage::setAttachmentData(CZISlide* slide, Type type, int64_t position, int32_t size,
                                             const std::string& name)
{
    m_slide = slide;
    m_type = type;
    m_dataPos = position;
    m_dataSize = size;
    m_sceneName = name;
}

slideio::CZIAuxImage::Type slideio::CZIAuxImage::typeFromString(const std::string& typeName)
{
    Type type = Type::Unknown;

    if(typeName.compare("JPG")==0){
        type = Type::JPG;
    }
    else if (typeName.compare("PNG") == 0){
        type = Type::PNG;
    }
    else if (typeName.compare("CZI") == 0) {
        type = Type::CZI;
    }
    return type;
}

