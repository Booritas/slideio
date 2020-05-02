// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#ifndef OPENCV_slideio_svsscene_HPP
#define OPENCV_slideio_svsscene_HPP

#include "slideio/slideio_def.hpp"
#include "slideio/core/cvscene.hpp"
#include "slideio/imagetools/tifftools.hpp"

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif

namespace slideio
{
    class SLIDEIO_EXPORTS SVSScene : public CVScene
    {
    public:
        /**
         * \brief Constructor
         * \param filePath: path to the slide file
         * \param name: scene name
         * \param hfile: tiff file handle of the slide
         */
        SVSScene(const std::string& filePath, const std::string& name);

        virtual ~SVSScene();

        std::string getFilePath() const override {
            return m_filePath;
        }
        std::string getName() const override {
            return m_name;
        }
        Compression getCompression() const override{
            return m_compression;
        }
        slideio::Resolution getResolution() const override{
            return m_resolution;
        }
        double getMagnification() const override{
            return m_magnification;
        }
        DataType getChannelDataType(int) const override{
            return m_dataType;
        }
    protected:
        std::string m_filePath;
        std::string m_name;
        Compression m_compression;
        Resolution m_resolution;
        double m_magnification;
        slideio::DataType m_dataType;
        TIFF* m_hFile;
    };
}

#if defined(_MSC_VER)
#pragma warning( pop )
#endif

#endif
