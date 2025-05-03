// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

#include "slideio/slideio/slideio_def.hpp"
#include <string>
#include <memory>
#include <list>
#include "scene.hpp"

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif


namespace slideio
{
    class CVSlide;
    /** @brief Slide class is an interface for accessing the information on a medical slide. 

    Slide class is an interface for accessing the information on a medical  Slide class represents a medical slide. 
    A medical slide is normally a is represented by a single file or folder.  A slide may contain multiple images represented by the Scene class.
    Additionally a slide may contain a number of auxiliary images like labels, macro, etc.
    Objects of the class can be obtained by calling of slideio::openSlide() global function.
    */
    class SLIDEIO_EXPORTS Slide
    {
        friend SLIDEIO_EXPORTS std::shared_ptr<Slide> openSlide(const std::string& path, const std::string& driver);
    private:
        /** Constructor of the class. 
        @param slide : object of a CVSlide class created by a corresponding ImageDriver object. */
        Slide(std::shared_ptr<CVSlide> slide);
    public:
        virtual ~Slide();
        /**@brief The method returns number of Scene objects contained in the slide.*/
        int getNumScenes() const;
        /**@brief The method returns a string which represents file path of the slide.*/
        std::string getFilePath() const;
        /**@brief The method returns a Scene object by the scene index.*/
        std::shared_ptr<Scene> getScene(int index) const;
        /**@brief The method returns a Scene object by the scene name.*/
        std::shared_ptr<Scene> getSceneByName(const std::string& name) const;
        /**@brief The method returns a string containing serialized metadata of the slide.
         *
         *Content of the string depends on image driver and may be plain text, 
        xml or json formatted string. Default value is an empty string.*/
        const std::string& getRawMetadata() const;
        /**@brief The method returns list of names of auxiliary images contained in the slide.
         *
         *Default: empty list.*/
		MetadataFormat getMetadataFormat() const;
        const std::list<std::string>& getAuxImageNames() const;
        /**@brief The method returns number of auxiliary images contained in the slide.*/
        virtual int getNumAuxImages() const;
        /**@brief Returns a Scene object that represents an auxiliary image with the supplied name.
         *
        @param sceneName : name of the auxiliary image. It must be contained in the list returned by getAuxImageNames method. 
        */
        virtual std::shared_ptr<Scene> getAuxImage(const std::string& sceneName) const;
		/**@brief The method returns a string containing serialized metadata of the slide.
		 */
        std::string toString() const;
        void setDriverId(const std::string& driverId);
        const std::string& getDriverId() const;
    private:
        std::shared_ptr<CVSlide> m_slide;
    };
}

#define SlidePtr std::shared_ptr<slideio::Slide>

#if defined(_MSC_VER)
#pragma warning( pop )
#endif
