// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

#include "slideio/slideio/slideio_def.hpp"
#include "slideio/base/slideio_enums.hpp"
#include <string>
#include <vector>
#include <memory>
#include <list>

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif

namespace slideio
{
    class CVScene;
    /**@brief Scene class represents a raster image contained in a slide.
    * 
    * Scene class allows extracting information from image of a slide. It includes raster data as well as metadata.
    * The object supports multichannel multi-dimensional rasters. The class provides methods for resampling of multi-dimensional rasters.
    */
    class SLIDEIO_EXPORTS Scene
    {
        friend class Slide;
    private:
        Scene(std::shared_ptr<CVScene> scene);
    public:
        virtual ~Scene(){
        }
        /**@brief returns path of the slide */
        std::string getFilePath() const;
        /**@brief returns scene name */
        std::string getName() const;
        /**@brief returns image rectangle in pixels.
        * 
        Rectangle is represented as std::tuple (x,y,width,height). Where:
        - x,y - image origins (pixel coordinates of the left top corner of the image in slide coordinates),
        - width - image width in pixels,
        - height - image height in pixels.
        @note Scene origins should be ignored for reading of image rasters.*/
        std::tuple<int,int,int,int> getRect() const;
        /**@brief returns number of raster channels of the image */
        int getNumChannels() const;
        /**@brief returns number of slices for 3D/4D images. 
        *
        * The method returns 1 for 2D images. */
        int getNumZSlices();
        /**@brief returns number of time frames for 3D/4D images.
        *
        * The method returns 1 for images without time frames. */
        int getNumTFrames();
        /**@brief returns compression of the raster data */
        Compression getCompression() const;
        /**brief returns data type of a channel 
        
        @param channel : index of the channel. Should be in the range (0, numberOfChannels)*/
        slideio::DataType getChannelDataType(int channel) const;
        /**@brief returns channel name. 
        @param channel :  index of the channel. Should be in the range (0, numberOfChannels)*/
        std::string getChannelName(int channel) const;
        /**@brief returns image resolution in x and y direction.

        Resolution is returned as a std::tuple (resolutionXdirection, resolutionYdirection). Resolution is size of a pixel in meters.
        */
        std::tuple<double,double> getResolution() const;
        /**@brief returns thickness of a Z slice in meters for 3D images.*/
        double getZSliceResolution() const;
        /**@brief returns time between 2 time frames in seconds for images with time frames.*/
        double getTFrameResolution() const;
        /**@brief returns slide magnification extracted from the slide metadata. */
        double getMagnification() const;
        /**@brief returns memory size in the bytes required for a raster block.
         *
         * @param blockSize : a std::tuple<int,int> defining with and height of the image block;
         * @param refChannel : index of the first channel to be included in the block;
         * @param numChannels : number of channels in the block;
         * @param numSlices : number of Z slices in the block. For plane images it shall be 1;
         * @param numFrames : number of time frames in the block. For plane images it shall be 1;
         
         */
        int getBlockSize(const std::tuple<int,int>& blockSize, int refChannel, int numChannels, int numSlices, int numFrames) const;
        /**@brief reads raster rectangle of a plane image to a memory buffer.
         *
         * @param blockRect : rectangle of the block to be read. The rectangle is represented by std::tuple(x,y,with,height). Here:
         * - x, y: pixel coordinates of the top left corner of the block in image coordinates (pixel shifts of the left top corner of the block;
         * - width: block width in pixels;
         * - height: block height in pixels;
         * from left top corner of the image).
         * @param buffer : pointer to an allocated memory buffer for the raster block. Size of the block can be computed with the method getBlockSize;
         * @param bufferSize : size of the memory buffer in bytes.
         *
         * All channels have to be of the same type. The method will throw an error by attempt to put read several channels of different types in one block.
         * The raster will be placed in the memory buffer. The raster is organized as a continuous 3-dimensional array.
         * @code
         * DataType raster[numberOfRows][numberOfColumns][numberOfChannels];
         * @endcode
         * where:
         * - DataType is a channel data type (i.e. uint8_t, uint16_t, ..., float, double);
         * - numberOfChannels: number of channels in the block;
         * - numberOfRows: width of the block in pixels;
         * - numberOfColumns: height of the block in pixels;
         * The following code snippet can be used to compute a location of a channel i uint16_t channel of a pixel with coordinates x, y:
         * @code
         * // convert buffer to the correct data type
         * uint16_t* typedBuff = (uint16_t*) buffer;
         * // compute value location
         * uint32_t location = (numberOfChannels*numberOfColumns)*y + (numberOfChannels*x) + i;
         * // extract value from the buffer
         * uint16_t channelValue = typedBuffer[location];
         * @endcode
         */
        void readBlock(const std::tuple<int,int,int,int>& blockRect, void* buffer, size_t bufferSize);
        /**@brief reads raster rectangle plane image combined from selected channels to a memory buffer.
         *
         * @param blockRect : rectangle of the block to be read. The rectangle is represented by std::tuple(x,y,with,height). Here:
         * - x, y: pixel coordinates of the top left corner of the block in image coordinates (pixel shifts of the left top corner of the block;
         * - width: block width in pixels;
         * - height: block height in pixels;
         * from left top corner of the image).
         * @param channelIndices : vector of indices of channels to be extracted;
         * @param buffer : pointer to an allocated memory buffer for the raster block. Size of the block can be computed with the method getBlockSize;
         * @param bufferSize : size of the memory buffer in bytes.
         *
         * All channels have to be of the same type. The method will throw an error by attempt to put read several channels of different types in one block.
         * The raster will be placed in the memory buffer. Memory layout of the buffer is described in the #readBlock method.
         */
        void readBlockChannels(const std::tuple<int,int,int,int>& blockRect, const std::vector<int>& channelIndices, void* buffer, size_t bufferSize);
        /**@brief reads raster rectangle plane image and resizes it to the size specified in the parameters.
         *
         * @param blockRect : rectangle of the block to be read. The rectangle is represented by @b std::tuple<x,y,with,height>. Here:
         * - x, y: pixel coordinates of the top left corner of the block in image coordinates (pixel shifts of the left top corner of the block;
         * - width: block width in pixels;
         * - height: block height in pixels;
         * from left top corner of the image).
         * @param blockSize : size of the block after resizing. The size is set as a @b std::tuple<width,height>;
         * @param buffer : pointer to an allocated memory buffer for the raster block. Size of the block can be computed with the method getBlockSize.
         * @note the size should be large enough to fit resized block (i.e. with width equal @b std::get<0>(blockSize), and height equal @b std::get<1>(blockSize);
         * @param bufferSize : size of the memory buffer in bytes.
         *
         * All channels have to be of the same type. The method will throw an error by attempt to put read several channels of different types in one block.
         * The raster will be placed in the memory buffer. Memory layout of the buffer is described in the #readBlock method.
         */
        void readResampledBlock(const std::tuple<int,int,int,int>& blockRect, const std::tuple<int,int>& blockSize, void* buffer, size_t bufferSize);
        /**@brief reads raster rectangle plane image combined from selected channels and resizes it to the size specified in the parameters.
         *
         * @param blockRect : rectangle of the block to be read. The rectangle is represented by @b std::tuple<x,y,with,height>. Here:
         * - x, y: pixel coordinates of the top left corner of the block in image coordinates (pixel shifts of the left top corner of the block;
         * - width: block width in pixels;
         * - height: block height in pixels;
         * from left top corner of the image).
         * @param blockSize : size of the block after resizing. The size is set as a @b std::tuple<width,height>;
         * @param channelIndices : vector of indices of channels to be extracted;
         * @param buffer : pointer to an allocated memory buffer for the raster block. Size of the block can be computed with the method getBlockSize.
         * @note the size should be large enough to fit resized block (i.e. with width equal @b std::get<0>(blockSize), and height equal @b std::get<1>(blockSize);
         * @param bufferSize : size of the memory buffer in bytes.
         *
         * All channels have to be of the same type. The method will throw an error by attempt to put read several channels of different types in one block.
         * The raster will be placed in the memory buffer. Memory layout of the buffer is described in the #readBlock method.
         */
        void readResampledBlockChannels(const std::tuple<int,int,int,int>& blockRect, const std::tuple<int,int>& blockSize, const std::vector<int>& channelIndices, void* buffer, size_t bufferSize);
        /**@brief reads multi-dimensional raster block to a memory buffer.
         *
         * @param blockRect : rectangle of the block to be read. The rectangle is represented by std::tuple(x,y,with,height). Here:
         * - x, y: pixel coordinates of the top left corner of the block in image coordinates (pixel shifts of the left top corner of the block;
         * - width: block width in pixels;
         * - height: block height in pixels;
         * from left top corner of the image).
         * @param zSliceRange : range of z-slices to be read. Defined as a @b std::tuple<indexOfFirstSliceToRead,numberOfSlicesToRead>
         * @param timeFrameRange : range of time frames to be read. Defined as a @b std::tuple<indexOfFirstTimeFrameToRead,numberOfTimeFramesToRead>;
         * @param buffer : pointer to an allocated memory buffer for the raster block. Size of the block can be computed with the method getBlockSize;
         * @param bufferSize : size of the memory buffer in bytes.
         *
         * All channels have to be of the same type. The method will throw an error by attempt to put read several channels of different types in one block.
         * The raster will be placed in the memory buffer. The raster is organized as a continuous multi-dimensional array.
         * @code
         * DataType raster[numberOfTimeFramesToRead][numberOfzSlicesToRead][numberOfBlockRows][numberOfBlockColumns][numberOfBlockChannels];
         * @endcode
         * where:
         * - DataType is a channel data type (i.e. uint8_t, uint16_t, ..., float, double);
         * - numberOfChannels: number of channels in the block;
         * - numberOfRows: width of the block in pixels;
         * - numberOfColumns: height of the block in pixels;
         * The following code snippet can be used to compute a location of a channel i uint16_t channel of a pixel with coordinates x, y
         * located in time frame tz and zSlice zs:
         * @code
         * // convert buffer to the correct data type
         * uint16_t* typedBuff = (uint16_t*) buffer;
         * // compute row size
         * uint32_t rowSize = numberOfBlockChannels*numberOfBlockColumns;
         * // compute slice size
         * uint32_t sliceSize = numberOfBlockRows*rowSize;
         * // compute time frame size
         * uint32_t tframeSize = numberOfzSlicesToRead*sliceSize;
         * // compute value location
         * uint32_t location = tf*tframeSize + zs*sliceSize + y*rowSize + (numberOfChannels*x) + i;
         * // extract value from the buffer
         * uint16_t channelValue = typedBuffer[location];
         * @endcode
         */
        void read4DBlock(const std::tuple<int,int,int,int>& blockRect, const std::tuple<int,int>& zSliceRange, const std::tuple<int,int>& timeFrameRange, void* buffer, size_t bufferSize);
        /**@brief reads selected channels of multi-dimensional raster block to a memory buffer.
         *
         * @param blockRect : rectangle of the block to be read. The rectangle is represented by std::tuple(x,y,with,height). Here:
         * - x, y: pixel coordinates of the top left corner of the block in image coordinates (pixel shifts of the left top corner of the block;
         * - width: block width in pixels;
         * - height: block height in pixels;
         * from left top corner of the image).
         * @param channelIndices : vector of indices of channels to be extracted;
         * @param zSliceRange : range of z-slices to be read. Defined as a @b std::tuple<indexOfFirstSliceToRead,numberOfSlicesToRead>
         * @param timeFrameRange : range of time frames to be read. Defined as a @b std::tuple<indexOfFirstTimeFrameToRead,numberOfTimeFramesToRead>;
         * @param buffer : pointer to an allocated memory buffer for the raster block. Size of the block can be computed with the method getBlockSize;
         * @param bufferSize : size of the memory buffer in bytes.
         *
         * All channels have to be of the same type. The method will throw an error by attempt to put read several channels of different types in one block.
         * The raster will be placed in the memory buffer. The raster is organized as a continuous multi-dimensional array.
         * Memory layout is the same as in #read4DBlock method.
         */
        void read4DBlockChannels(const std::tuple<int,int,int,int>& blockRect, const std::vector<int>& channelIndices, const std::tuple<int,int>& zSliceRange, const std::tuple<int,int>& timeFrameRange, void* buffer, size_t bufferSize);
        /**@brief reads multi-dimensional raster block to a memory buffer with resizing to the specified size.
         *
         * @param blockRect : rectangle of the block to be read. The rectangle is represented by std::tuple(x,y,with,height). Here:
         * - x, y: pixel coordinates of the top left corner of the block in image coordinates (pixel shifts of the left top corner of the block;
         * - width: block width in pixels;
         * - height: block height in pixels;
         * from left top corner of the image).
         * @param blockSize : size of the block after resizing. The size is set as a @b std::tuple<width,height>;
         * @param zSliceRange : range of z-slices to be read. Defined as a @b std::tuple<indexOfFirstSliceToRead,numberOfSlicesToRead>
         * @param timeFrameRange : range of time frames to be read. Defined as a @b std::tuple<indexOfFirstTimeFrameToRead,numberOfTimeFramesToRead>;
         * @param buffer : pointer to an allocated memory buffer for the raster block. Size of the block can be computed with the method getBlockSize;
         * @param bufferSize : size of the memory buffer in bytes.
         *
         * All channels have to be of the same type. The method will throw an error by attempt to put read several channels of different types in one block.
         * The raster will be placed in the memory buffer. The raster is organized as a continuous multi-dimensional array.
         * Memory layout is the same as in #read4DBlock method.
         */
        void readResampled4DBlock(const std::tuple<int,int,int,int>& blockRect, const std::tuple<int,int>& blockSize, const std::tuple<int,int>& zSliceRange, const std::tuple<int,int>& timeFrameRange, void* buffer, size_t bufferSize);
        /**@brief reads selected channels of multi-dimensional raster block to a memory buffer with resizing to the specified size.
         *
         * @param blockRect : rectangle of the block to be read. The rectangle is represented by std::tuple(x,y,with,height). Here:
         * - x, y: pixel coordinates of the top left corner of the block in image coordinates (pixel shifts of the left top corner of the block;
         * - width: block width in pixels;
         * - height: block height in pixels;
         * from left top corner of the image).
         * @param blockSize : size of the block after resizing. The size is set as a @b std::tuple<width,height>;
         * @param channelIndices : vector of indices of channels to be extracted;
         * @param zSliceRange : range of z-slices to be read. Defined as a @b std::tuple<indexOfFirstSliceToRead,numberOfSlicesToRead>
         * @param timeFrameRange : range of time frames to be read. Defined as a @b std::tuple<indexOfFirstTimeFrameToRead,numberOfTimeFramesToRead>;
         * @param buffer : pointer to an allocated memory buffer for the raster block. Size of the block can be computed with the method getBlockSize;
         * @param bufferSize : size of the memory buffer in bytes.
         *
         * All channels have to be of the same type. The method will throw an error by attempt to put read several channels of different types in one block.
         * The raster will be placed in the memory buffer. The raster is organized as a continuous multi-dimensional array.
         * Memory layout is the same as in #read4DBlock method.
         */
        void readResampled4DBlockChannels(const std::tuple<int,int,int,int>& blockRect, const std::tuple<int,int>& blockSize, const std::vector<int>& channelIndices, const std::tuple<int,int>& zSliceRange, const std::tuple<int,int>& timeFrameRange, void* buffer, size_t bufferSize);
        /**@brief returns array of names of auxiliary images available for the scene.*/
        virtual const std::list<std::string>& getAuxImageNames() const;
        /**@brief returns number of auxiliary images available for the scene.*/
        virtual int getNumAuxImages() const;
        /**@brief returns string of serialized metadata. Content of the string depends on image format.*/
        std::string getRawMetadata() const;
        /**@brief returns a slideio::Scene object that represents an auxiliary image.
         * @param imageName : name of the auxiliary image.
         */
        virtual std::shared_ptr<Scene> getAuxImage(const std::string& imageName) const;
        std::shared_ptr<CVScene> getCVScene() { return m_scene; }
    private:
        std::shared_ptr<CVScene> m_scene;
    };
}

#define ScenePtr std::shared_ptr<slideio::Scene>

#if defined(_MSC_VER)
#pragma warning( pop )
#endif
