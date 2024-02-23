// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#ifndef OPENCV_slideio_scene_HPP
#define OPENCV_slideio_scene_HPP

#include "slideio/core/slideio_core_def.hpp"
#include "slideio/core/cvstructs.hpp"
#include "slideio/base/slideio_enums.hpp"
#include "opencv2/core.hpp"
#include <vector>
#include <string>
#include <list>
#include "refcounter.hpp"
#include <map>

#include "levelinfo.hpp"

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif

namespace slideio
{
    /**@brief class CVScene represents a base class for opencv based representations of
     * raster images contained in a medical slide.
     *
     * Most of the class methods are pure virtual and must be overriten by child classes that
     * implement specific image format.
     * The class exposes methods for extraction raster data as well as metadata for the image.
     * The class supports 2D, 3D and 4D multi-channel images and provides methods for extraction
     * of arbitrary image regions, optionally with resizing.
     * Methods for working with raster data deliver image blocks as opencv Mat objects.
     */
    class SLIDEIO_CORE_EXPORTS CVScene : public RefCounter
    {
    public:
        /**@brief default constructor*/
        virtual ~CVScene() = default;
        /**@brief returns path of the slide */
        virtual std::string getFilePath() const = 0;
        /**@brief returns scene name */
        virtual std::string getName() const = 0;
        /**@brief returns image rectangle.
         *
         *Origins of the rectangle define shift of top left image corner from top left slide corner. */
        virtual cv::Rect getRect() const = 0;
        /**@brief returns number of image channels.*/
        virtual int getNumChannels() const = 0;
        /**@brief returns number of z-slices in the images.
         *
         * The method returns 1 for plain images.
         */
        virtual int getNumZSlices() const {return 1;}
        /**@brief returns number of time frames in the image.
         *
         * The method returns 1 for single frame images (images without time sequences).
         */
        virtual int getNumTFrames() const {return 1;}
        /**@brief returns channel data type.
         *
         * Some image format may have channels with different data types.
         */
        virtual slideio::DataType getChannelDataType(int channel) const = 0;
        /**@ returns channel name by channel index.
         *
         * Some formats provide human readable names for image channels. The method returns such
         * names extracted from image metadata.
         */
        virtual std::string getChannelName(int channel) const;
        /**@brief returns image resolution in x and y direction.

        Resolution is returned as class Resolution with pixel sizes in meters for x and y directions.
        */
        virtual Resolution  getResolution() const = 0;
        /**@brief returns thickness of a Z slice in meters for 3D images.*/
        virtual double getZSliceResolution() const {return 0;}
        /**@brief returns time between 2 time frames in seconds for images with time frames.*/
        virtual double getTFrameResolution() const {return 0;}
        /**@brief returns slide magnification extracted from the slide metadata. */
        virtual double getMagnification() const = 0;
        /**@brief returns compression of the raster data */
        virtual Compression getCompression() const = 0;
        /**@brief reads raster rectangle of a plane image.
         *
         * @param blockRect : rectangle of the block to be read. The rectangle is represented by cv::Rect structure
         * where x and y properties define shift of the top left corner of the block from the top left corner of the image. 
         * @param output : reference to cv::OutputArray object. The method creates cv::Mat object for the parameters and
         * allocate memory for the object if it is not yet allocated or allocated not enough memory.
         * All channels have to be of the same type. The method will throw an error by attempt to put read several channels of different types in one block.
         */
        virtual void readBlock(const cv::Rect& blockRect, cv::OutputArray output);
        /**@brief reads selected channels of raster rectangle of a plane image.
         *
         * @param blockRect : rectangle of the block to be read. The rectangle is represented by cv::Rect structure.
         * @param channelIndices : vector of indices of channels to be extracted;
         * @param output : reference to cv::OutputArray object. The method creates cv::Mat object for the parameters and
         * allocate memory for the object if it is not yet allocated or allocated not enough memory.
         * All channels have to be of the same type. The method will throw an error by attempt to put read several channels
         * of different types in one block.
         */
        virtual void readBlockChannels(const cv::Rect& blockRect, const std::vector<int>& channelIndices, cv::OutputArray output);
        /**@brief reads raster rectangle of a plane image with resizing.
         *
         * @param blockRect : rectangle of the block to be read. The rectangle is represented by cv::Rect structure
         * where x and y properties define shift of the top left corner of the block from the top left corner of the image.
         * @param blockSize : size of the block after resizing.
         * @param output : reference to cv::OutputArray object. The method creates cv::Mat object for the parameters and
         * allocate memory for the object if it is not yet allocated or allocated not enough memory.
         * All channels have to be of the same type. The method will throw an error by attempt to put read several channels
         * of different types in one block.
         */
        virtual void readResampledBlock(const cv::Rect& blockRect, const cv::Size& blockSize, cv::OutputArray output);
        /**@brief reads selected channels raster rectangle of a plane image with resizing.
         *
         * @param blockRect : rectangle of the block to be read. The rectangle is represented by cv::Rect structure
         * where x and y properties define shift of the top left corner of the block from the top left corner of the image.
         * @param blockSize : size of the block after resizing.
         * @param channelIndices : vector of indices of channels to be extracted;
         * @param output : reference to cv::OutputArray object. The method creates cv::Mat object for the parameters and
         * allocate memory for the object if it is not yet allocated or allocated not enough memory.
         * All channels have to be of the same type. The method will throw an error by attempt to put read several channels
         * of different types in one block.
         */
        virtual void readResampledBlockChannels(const cv::Rect& blockRect, const cv::Size& blockSize, const std::vector<int>& channelIndices, cv::OutputArray output);
        /**@brief reads multi-dimensional raster block.
         *
         * @param blockRect : rectangle of the block to be read. The rectangle is represented by cv::Rect structure
         * where x and y properties define shift of the top left corner of the block from the top left corner of the image.
         * @param zSliceRange : range of z-slices to be read. Defined as a @b std::tuple<indexOfFirstSliceToRead,numberOfSlicesToRead>
         * @param timeFrameRange : range of time frames to be read. Defined as a @b std::tuple<indexOfFirstTimeFrameToRead,numberOfTimeFramesToRead>;
         * @param output : reference to cv::OutputArray object. The method creates cv::Mat object for the parameters and
         * allocate memory for the object if it is not yet allocated or allocated not enough memory.
         * All channels have to be of the same type. The method will throw an error by attempt to put read several channels
         * of different types in one block.
         */
        virtual void read4DBlock(const cv::Rect& blockRect, const cv::Range& zSliceRange, const cv::Range& timeFrameRange, cv::OutputArray output);
        /**@brief multi-dimensional raster block with resizing.
         *
         * @param blockRect : rectangle of the block to be read. The rectangle is represented by cv::Rect structure
         * where x and y properties define shift of the top left corner of the block from the top left corner of the image.
         * @param channelIndices : vector of indices of channels to be extracted;
         * @param zSliceRange : range of z-slices to be read. Defined as a @b std::tuple<indexOfFirstSliceToRead,numberOfSlicesToRead>
         * @param timeFrameRange : range of time frames to be read. Defined as a @b std::tuple<indexOfFirstTimeFrameToRead,numberOfTimeFramesToRead>;
         * @param output : reference to cv::OutputArray object. The method creates cv::Mat object for the parameters and
         * allocate memory for the object if it is not yet allocated or allocated not enough memory.
         * All channels have to be of the same type. The method will throw an error by attempt to put read several channels
         * of different types in one block.
         */
        virtual void read4DBlockChannels(const cv::Rect& blockRect, const std::vector<int>& channelIndices, const cv::Range& zSliceRange, const cv::Range& timeFrameRange, cv::OutputArray output);
        /**@brief reads multi-dimensional raster block with resizing.
         *
         * @param blockRect : rectangle of the block to be read. The rectangle is represented by cv::Rect structure
         * where x and y properties define shift of the top left corner of the block from the top left corner of the image.
         * @param blockSize : size of the block after resizing.
         * @param zSliceRange : range of z-slices to be read. Defined as a @b std::tuple<indexOfFirstSliceToRead,numberOfSlicesToRead>
         * @param timeFrameRange : range of time frames to be read. Defined as a @b std::tuple<indexOfFirstTimeFrameToRead,numberOfTimeFramesToRead>;
         * @param output : reference to cv::OutputArray object. The method creates cv::Mat object for the parameters and
         * allocate memory for the object if it is not yet allocated or allocated not enough memory.
         * All channels have to be of the same type. The method will throw an error by attempt to put read several channels
         * of different types in one block.
         */
        virtual void readResampled4DBlock(const cv::Rect& blockRect, const cv::Size& blockSize, const cv::Range& zSliceRange, const cv::Range& timeFrameRange, cv::OutputArray output);
        /**@brief reads selected channels of multi-dimensional raster block.
         *
         * @param blockRect : rectangle of the block to be read. The rectangle is represented by cv::Rect structure
         * where x and y properties define shift of the top left corner of the block from the top left corner of the image.
         * @param blockSize : size of the block after resizing.
         * @param channelIndices : vector of indices of channels to be extracted;
         * @param zSliceRange : range of z-slices to be read. Defined as a @b std::tuple<indexOfFirstSliceToRead,numberOfSlicesToRead>
         * @param timeFrameRange : range of time frames to be read. Defined as a @b std::tuple<indexOfFirstTimeFrameToRead,numberOfTimeFramesToRead>;
         * @param output : reference to cv::OutputArray object. The method creates cv::Mat object for the parameters and
         * allocate memory for the object if it is not yet allocated or allocated not enough memory.
         * All channels have to be of the same type. The method will throw an error by attempt to put read several channels
         * of different types in one block.
         */
        virtual void readResampled4DBlockChannels(const cv::Rect& blockRect, const cv::Size& blockSize, const std::vector<int>& channelIndices, const cv::Range& zSliceRange, const cv::Range& timeFrameRange, cv::OutputArray output);
        /**@brief returns list of auxiliary image names.*/
        virtual const std::list<std::string>& getAuxImageNames() const {
            return m_auxNames;
        }
        /**@brief returns number of auxiliary images in the scene object.*/
        virtual int getNumAuxImages() const {
            return static_cast<int>(m_auxNames.size());
        }
        /**@brief returns a slideio::CVScene object that represents an auxiliary image.
         * @param imageName : name of the auxiliary image.
         */
        virtual std::shared_ptr<CVScene> getAuxImage(const std::string& imageName) const;
        /**@brief returns string of serialized metadata. Content of the string depends on image format.*/
        virtual std::string getRawMetadata() const { return ""; }
        virtual void readResampledBlockChannelsEx(const cv::Rect& blockRect, const cv::Size& blockSize,
            const std::vector<int>& componentIndices, int zSliceIndex, int tFrameIndex, cv::OutputArray output);
        virtual int getNumZoomLevels() const;
        virtual const LevelInfo* getZoomLevelInfo(int level) const;
        std::string toString() const;
    protected:
        std::vector<int> getValidChannelIndices(const std::vector<int>& channelIndices);
        void initializeSceneBlock(const cv::Size& blockSize, const std::vector<int>& channelIndices,
                                  cv::OutputArray output) const;

    protected:
        std::list<std::string> m_auxNames;
        std::vector<LevelInfo> m_levels;
    };
}

#define CVScenePtr std::shared_ptr<slideio::CVScene>

#if defined(_MSC_VER)
#pragma warning( pop )
#endif

#endif