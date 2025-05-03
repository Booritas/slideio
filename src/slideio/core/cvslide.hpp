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
    /**@brief CVSlide is an base class for representation of medical slide.
     *
     * Child classes of the CVSlide class are format specific representations of the medical slide.
     * The class provides access to a slide structure, internal images and metadata.
     * Each slide contains at least one image represented by #slideio::CVScene class.
     * A slide may contain multiple auxiliary images such as labels, macros, bar-codes, etc.
     * Object of the child classes are created by classes derived from slideio::ImageDriver class
     * and cannot be created directly; 
     */
    class SLIDEIO_CORE_EXPORTS CVSlide
    {
        friend class ImageDriver;
    protected:
        virtual ~CVSlide() = default;
    public:
        /**@brief The method returns number of Scene objects contained in the slide.*/
        virtual int getNumScenes() const = 0;
        /**@brief The method returns a string which represents file path of the slide.*/
        virtual std::string getFilePath() const = 0;
        /**@brief The method returns a string containing serialized metadata of the slide.
         *
         *Content of the string depends on image driver and may be plain text,
        xml or json formatted string. Default value is an empty string.*/
        virtual const std::string& getRawMetadata() const {return m_rawMetadata;}
        /**@brief The method returns a CVScene object by the scene index.*/
        virtual std::shared_ptr<CVScene> getScene(int index) const = 0;
        /**@brief The method returns a CVScene object by the scene name.*/
        virtual std::shared_ptr<CVScene> getSceneByName(const std::string& name);
        /**@brief The method returns list of names of auxiliary images contained in the slide.
         *
         *Default: empty list.*/
        virtual const std::list<std::string>& getAuxImageNames() const {
            return m_auxNames;
        }
        /**@brief The method returns number of auxiliary images contained in the slide.*/
        virtual int getNumAuxImages() const {
            return static_cast<int>(m_auxNames.size());
        }
        /**@brief Returns a slideio::CVScene object that represents an auxiliary image with the supplied name.
         *
        @param sceneName : name of the auxiliary image. It must be contained in the list returned by getAuxImageNames method.
        */
        virtual std::shared_ptr<CVScene> getAuxImage(const std::string& sceneName) const;
		/**@brief The method returns a string containing serialized metadata of the slide.
		 *
		 *Content of the string depends on image driver and may be plain text.
		 */
		 virtual MetadataFormat getMetadataFormat() const { return m_metadataFormat; }
		 /**@brief The method sets driver id of the slide.  */
        void setDriverId(const std::string& driverId) {
			m_driverId = driverId;
		 }
		/**@brief The method returns driver id of the slide. */
		const std::string& getDriverId() const {
			return m_driverId;
		}
        /**@brief The method returns format of a string passed as a parameter. */
         static MetadataFormat recognizeMetadataFormat(const std::string& metadata);
		 /**@brief The method returns a string containing serialized metadata of the slide. */
         std::string toString() const;
    protected:
        std::string m_rawMetadata;
		MetadataFormat m_metadataFormat = MetadataFormat::None;
        std::list<std::string> m_auxNames;
        std::string m_driverId;
    };
}

#define CVSlidePtr std::shared_ptr<slideio::CVSlide>

#if defined(_MSC_VER)
#pragma warning( pop )
#endif

#endif