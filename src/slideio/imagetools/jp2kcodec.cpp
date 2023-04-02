// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/imagetools/imagetools.hpp"
#include <opencv2/imgproc.hpp>
#include "slideio/imagetools/memory_stream.hpp"
#include "slideio/base/exceptions.hpp"
#include "slideio/base/log.hpp"
#include "jp2kcodec.hpp"

#include <openjpeg.h>
#include <boost/format.hpp>
#include <boost/filesystem.hpp>
#include <fstream>

#include "single_tests/jp2k/jp2_memory.hpp"

/* opj_* Helper code from https://groups.google.com/forum/#!topic/openjpeg/8cebr0u7JgY */


static void openjpeg_warning(const char* msg, void* client_data)
{
    SLIDEIO_LOG(WARNING) << msg;
}

static void openjpeg_error(const char* msg, void* client_data)
{
    RAISE_RUNTIME_ERROR << msg;
}

static void openjpeg_info(const char* msg, void* client_data)
{
    SLIDEIO_LOG(INFO) << msg;
}



void slideio::ImageTools::readJp2KFile(const std::string& filePath, cv::OutputArray output)
{
    auto fileSize = boost::filesystem::file_size(filePath);
    if(fileSize<=0)
        throw std::runtime_error(
            (boost::format("Invalid file: %1%") % filePath).str());
    
    std::ifstream file(filePath, std::ios::binary);
    // Stop eating new lines in binary mode!!!
    file.unsetf(std::ios::skipws);
    // reserve capacity
    std::vector<uint8_t> vec;
    vec.reserve(fileSize);
    // read the data:
    vec.insert(vec.begin(),
        std::istream_iterator<uint8_t>(file),
        std::istream_iterator<uint8_t>());
    decodeJp2KStream(vec, output);
}

static int getComponentDataType(const opj_image_comp_t* comp)
{
    switch(comp->prec)
    {
    case 8:
        return ((comp->sgnd)?CV_8S:CV_8U);
    case 16:
        return ((comp->sgnd)?CV_16S:CV_16U);
    case 32:
        return CV_32S;
    }
    throw std::runtime_error(
        (boost::format("Unknown data type of data: %1%") % (int)comp->bpp).str());
}

static OPJ_CODEC_FORMAT getJP2KCodec(const std::vector<uint8_t>& data)
{
    static const unsigned char jpc_header[] = {0xff, 0x4f};
    static const unsigned char jp2_box_jp[] = {0x6a, 0x50, 0x20, 0x20}; /* 'jP  ' */

    const uint8_t *buf = data.data();
    size_t len = data.size();

    OPJ_CODEC_FORMAT eCodecFormat;
    if (len >= sizeof(jpc_header) &&
            memcmp(buf, jpc_header, sizeof(jpc_header)) == 0) {
        eCodecFormat = OPJ_CODEC_J2K;
    }
    else if (len >= 4 + sizeof(jp2_box_jp) &&
               memcmp(buf + 4, jp2_box_jp, sizeof(jp2_box_jp)) == 0) {
        eCodecFormat = OPJ_CODEC_JP2;
    }
    else
    {
        throw std::runtime_error("Unknown file format");
    }
    return eCodecFormat;
}

void slideio::ImageTools::decodeJp2KStream(
    const std::vector<uint8_t>& data,
    cv::OutputArray output,
    const std::vector<int>& channelIndices,
    bool forceYUV)
{
    opj_codec_t* codec(nullptr);
    opj_image_t* image(nullptr);
    opj_stream_t* stream(nullptr);
    try
    {
        OPJ_CODEC_FORMAT codecId = getJP2KCodec(data);
        OPJStreamUserData userData((uint8_t*)data.data(), data.size());
        stream = createOPJMemoryStream(&userData, data.size(), true);
        codec = opj_create_decompress(codecId);
        if(!codec)
            throw std::runtime_error("Cannot get required codec");
        opj_dparameters_t jp2dParams;
        opj_set_default_decoder_parameters(&jp2dParams);
        if (!opj_setup_decoder(codec, &jp2dParams)){
            throw std::runtime_error("Cannot setup codec");
        }
        if(!opj_read_header(stream, codec, &image) || (image->numcomps == 0)){
            throw std::runtime_error("Error reading image header");
        }
        if(forceYUV)
            image->color_space = OPJ_CLRSPC_SYCC;
        // decode the image
        OPJ_BOOL ret = opj_decode(codec, stream, image);
        if(!ret)
            throw std::runtime_error("Error by decoding of Jp2K stream");

        opj_end_decompress(codec,stream);
        opj_destroy_codec(codec);
        codec=nullptr;
        opj_stream_destroy(stream);
        stream = nullptr;

        const OPJ_UINT32 imageWidth = image->x1 - image->x0;
        const OPJ_UINT32 imageHeight = image->y1 - image->y0;
        const OPJ_UINT32 numComps = image->numcomps;
        const int dt = getComponentDataType(image->comps);

        output.create(imageHeight, imageWidth,CV_MAKETYPE(dt, image->numcomps));
        const cv::Size imageSize(imageWidth, imageHeight); 

        std::vector<cv::Mat> imagePlanes;
        std::vector<int> channels(channelIndices);

        for(OPJ_UINT32 channel=0; channel<numComps; channel++)
        {
            const opj_image_comp& component = image->comps[channel];
            // create a cv::Mat object with the buffer 
            cv::Mat compRaster32S(component.h, component.w, CV_MAKETYPE(CV_32S, 1), component.data);
            // convert raster from 32 bit integer to the original type
            cv::Mat compRaster;
            compRaster32S.convertTo(compRaster, CV_MAKETYPE(dt,1));
            // check if we need to resize the component
            if(component.w!=imageWidth || component.h!=imageHeight)
            {
                // resize the component so it fits to the image size
                cv::Mat resized(imageSize.height, imageSize.width, CV_MAKETYPE(dt, 1));
                cv::resize(compRaster, resized, imageSize);
                imagePlanes.push_back(resized);
            }
            else
            {
                imagePlanes.push_back(compRaster);
            }
        }
        cv::Mat targetImage;
        if(forceYUV)
        {
            cv::Mat cvImage;
            cv::merge(imagePlanes, cvImage);
            cv::cvtColor(cvImage, targetImage, cv::COLOR_YUV2RGB);
        }
        else
        {
            cv::merge(imagePlanes, targetImage);
        }
        if(channels.empty())
        {
            // if no channel is defined - return all channels
            targetImage.copyTo(output);
        }
        else
        {
            std::vector<cv::Mat> targetChannels;
            for(const int& channel : channels)
            {
                cv::Mat channelRaster;
                cv::extractChannel(targetImage,channelRaster, channel);
                targetChannels.push_back(channelRaster);
            }
            if(targetChannels.size()==1)
            {
                targetChannels[0].copyTo(output);
            }
            else
            {
                cv::merge(targetChannels, output);
            }
        }
        opj_image_destroy(image);
    }
    catch(std::exception& ex)
    {
        if(codec)
            opj_destroy_codec(codec);
        if(image)
            opj_image_destroy(image);
        if(stream)
            opj_stream_destroy(stream);
        throw ex;
    }
}

void rasterToOPJImage(const cv::Mat& mat, ImagePtr& image, const slideio::ImageTools::JP2KEncodeParameters& params)
{
    const int numChannels = mat.channels();
    std::vector<opj_image_cmptparm_t> channelParameters(numChannels);
    const int type = mat.type();
    const int depth = type & CV_MAT_DEPTH_MASK;
    const int bitDepth = static_cast<int>(8 * mat.elemSize() / numChannels);
    const int width = mat.cols;
    const int height = mat.rows;
    int sign = 0;

    switch (depth) {
    case CV_8S:
    case CV_16S:
    case CV_32S:
        sign = 1;
    }

    for (auto& parameter : channelParameters) {
        /* bits_per_pixel: 8 or 16 */
        memset(&parameter, 0, sizeof(opj_image_cmptparm_t));
        parameter.prec = static_cast<OPJ_UINT32>(bitDepth);
        parameter.sgnd = sign;
        parameter.dx = static_cast<OPJ_UINT32>(params.subSamplingDX);
        parameter.dy = static_cast<OPJ_UINT32>(params.subSamplingDY);
        parameter.w = static_cast<OPJ_UINT32>(width);
        parameter.h = static_cast<OPJ_UINT32>(height);
    }
    COLOR_SPACE colorSpace = OPJ_CLRSPC_UNKNOWN;
    if (numChannels == 1) {
        colorSpace = OPJ_CLRSPC_GRAY;
    }
    else if (numChannels == 3) {
        colorSpace = OPJ_CLRSPC_SRGB;
    }

    image = opj_image_create(numChannels, channelParameters.data(),
        colorSpace);
    image.get()->x1 = width;
    image.get()->y1 = height;
    image.get()->numcomps = numChannels;


    uint8_t* data = mat.data;
    std::vector<int32_t*> channelData(numChannels);
    for (int channelIndex = 0; channelIndex < numChannels; ++channelIndex) {
        channelData[channelIndex] = image.get()->comps[channelIndex].data;
    }

    switch (depth) {
    case CV_8U:
        slideio::ImageTools::convertTo32bitChannels(data, mat.cols,
            mat.rows, numChannels, channelData.data());
        break;
    case CV_8S:
        slideio::ImageTools::convertTo32bitChannels(reinterpret_cast<int8_t*>(data), mat.cols,
            mat.rows, numChannels, channelData.data());
        break;
    case CV_16U:
        slideio::ImageTools::convertTo32bitChannels(reinterpret_cast<uint16_t*>(data), mat.cols,
            mat.rows, numChannels, channelData.data());
        break;
    case CV_16S:
        slideio::ImageTools::convertTo32bitChannels(reinterpret_cast<int16_t*>(data), mat.cols,
            mat.rows, numChannels, channelData.data());
        break;
    case CV_32S:
        slideio::ImageTools::convertTo32bitChannels(reinterpret_cast<int32_t*>(data), mat.cols,
            mat.rows, numChannels, channelData.data());
        break;
    default:
        RAISE_RUNTIME_ERROR << "Unsupported type for Jpeg2000 conversion: " << depth;
    }
}

int slideio::ImageTools::encodeJp2KStream(const cv::Mat& mat, uint8_t* buffer, int bufferSize,
    const JP2KEncodeParameters& jp2Params)
{
    wopj_cparameters parameters;   /* compression parameters */
    ImagePtr image;

    opj_set_default_encoder_parameters(&parameters);
    parameters.decod_format = 17;
    parameters.cod_format = jp2Params.codecFormat;
    parameters.tcp_mct = 0;

    parameters.tcp_numlayers = 1; // set number of quality layers
    parameters.tcp_rates[0] = jp2Params.compressionRate;
    CodecPtr codec = opj_create_compress((OPJ_CODEC_FORMAT)jp2Params.codecFormat);

    rasterToOPJImage(mat, image, jp2Params);
    if (image.get()->color_space == OPJ_CLRSPC_SRGB) {
        parameters.tcp_mct = 1;
    }
    parameters.cp_disto_alloc = 1;

    opj_set_info_handler(codec, openjpeg_info, nullptr);
    opj_set_warning_handler(codec, openjpeg_warning, nullptr);
    opj_set_error_handler(codec, openjpeg_error, nullptr);

    if (!opj_setup_encoder(codec, &parameters, image)) {
        RAISE_RUNTIME_ERROR << "Failed to encode image: opj_setup_encoder.";
    }

    opj_memory_stream stream;
    stream.dataSize = bufferSize;
    stream.offset = 0;
    stream.pData = buffer;

    opj_stream_t* strm = opj_stream_create_default_memory_stream(&stream, OPJ_FALSE);

    if (!strm) {
        RAISE_RUNTIME_ERROR << "Cannot create default file stream.";
    }
    if (!opj_start_compress(codec, image, strm)) {
        RAISE_RUNTIME_ERROR << "Failed to encode image : opj_start_compress.";
    }
    if (!opj_encode(codec, strm)) {
        RAISE_RUNTIME_ERROR << "Failed to encode image : opj_encode.";
    }
    if (!opj_end_compress(codec, strm)) {
        RAISE_RUNTIME_ERROR << "Failed to encode image : opj_end_compress.";
    }
    opj_stream_destroy(strm);
    return (int)stream.offset;
}


// std::vector<unsigned char> encodeJp2000(const cv::Mat& input, int quality)
// {
//     // Check input image type
//     int depth = input.depth();
//     int channels = input.channels();
//     int colorSpace = OPJ_CLRSPC_UNKNOWN;
//     if (channels == 1) {
//         colorSpace = OPJ_CLRSPC_GRAY;
//     }
//     else if (channels == 3) {
//         colorSpace = OPJ_CLRSPC_SRGB;
//     }
//     else if (channels == 4) {
//         colorSpace = OPJ_CLRSPC_SYCC;
//     }
//     if (depth != CV_8U && depth != CV_16U && depth != CV_32F) {
//        RAISE_RUNTIME_ERROR << "Input image must be 8-bit, 16-bit or 32-bit float grayscale or 3/4-channel image";
//     }
//
//     // Convert input image to 8-bit BGR
//     cv::Mat bgr;
//     if (depth == CV_8U) {
//         if (channels == 1) {
//             cv::cvtColor(input, bgr, cv::COLOR_GRAY2BGR);
//         }
//         else {
//             input.convertTo(bgr, CV_8U);
//         }
//     }
//     else if (depth == CV_16U) {
//         input.convertTo(bgr, CV_8U, 255.0 / 65535.0);
//     }
//     else {
//         input.convertTo(bgr, CV_8U, 255.0);
//     }
//
//     // Create OpenJPEG codec
//     opj_cparameters_t parameters;
//     opj_set_default_encoder_parameters(&parameters);
//     parameters.tcp_numlayers = 1;
//     parameters.tcp_rates[0] = quality;
//     opj_codec_t* codec = opj_create_compress(OPJ_CODEC_J2K);
//
//     // Set OpenJPEG stream
//     opj_stream_t* stream = opj_stream_create_default_memory_stream();
//     opj_set_info_handler(codec, NULL, NULL);
//     opj_set_warning_handler(codec, NULL, NULL);
//     opj_setup_encoder(codec, &parameters, image);
//     opj_image_t* image = opj_image_create(bgr.size().width, bgr.size().height, 3, colorSpace);
//     image->x0 = 0;
//     image->y0 = 0;
//     image->x1 = bgr.size().width;
//     image->y1 = bgr.size().height;
//     for (int i = 0; i < channels; i++)
//     {
//         image->comps[i].data = bgr.data + i;
//         image->comps[i]. = channels;
//     }
//     opj_start_compress(codec, image, stream);
//
//     // Encode OpenJPEG stream
//     opj_encode(codec, stream);
//
//     // Get compressed buffer
//     OPJ_UINT32 compressedSize;
//     OPJ_BYTE* compressedData;
//     opj_end_compress(codec, stream);
//     opj_stream_memory_get_buffer(stream, &compressedData, &compressedSize, true);
//     std::vector<unsigned char> compressedBuffer(compressedData, compressedData + compressedSize);
//
//     // Clean up
//     opj_stream_destroy(stream);
//     opj_destroy_codec(codec);
//     opj_image_destroy(image);
//
//     return compressedBuffer;
// }
