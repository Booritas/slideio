#include <iostream>
#include <openjpeg.h>

#include "format_defs.h"
#include "slideio/base/exceptions.hpp"
#include "slideio/core/imagedrivermanager.hpp"
#include "opj_wrappers.h"

static void error_callback(const char* msg, void* )
{
    RAISE_RUNTIME_ERROR << msg;
}
static void warning_callback(const char* msg, void* )
{
    SLIDEIO_LOG(WARNING) << msg;
}
static void info_callback(const char* msg, void* )
{
    SLIDEIO_LOG(INFO) << msg;
}

struct ConvertJ2KParameters
{
    int subSamplingDX;
    int subSamplingDY;
};

template <typename Type>
void convertTo32bitChannels(Type* data, int width, int height, int numChannels, int32_t** channels)
{
    const int pixelSize = numChannels;
    const int stride = pixelSize * width;
    Type* line = data;
    int channelShift = 0;
    for (int y = 0; y < height; ++y) {
        Type* pixel = line;
        for (int x = 0; x < width; ++x) {
            for (int channelIndex = 0; channelIndex < numChannels; ++channelIndex) {
                int32_t* channel = channels[channelIndex];
                channel[channelShift] = static_cast<int32_t>(pixel[channelIndex]);
            }
            pixel += pixelSize;
            channelShift++;
        }
        line += stride;
    }
}

void rasterToOPJImage(const cv::Mat& mat, ImagePtr& image, const ConvertJ2KParameters& params)
{
    const int numChannels = mat.channels();
    std::vector<opj_image_cmptparm_t> channelParameters(numChannels);
    const int type = mat.type();
    const int depth = type & CV_MAT_DEPTH_MASK;
    const int bitDepth = static_cast<int>(8 * mat.elemSize() / numChannels);
    const int width = mat.cols;
    const int height = mat.rows;
    int sign = 0;

    switch(depth) {
    case CV_8S:
    case CV_16S:
    case CV_32S:
        sign = 1;
    }

    for (auto & parameter : channelParameters) {
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
    if(numChannels == 1) {
        colorSpace = OPJ_CLRSPC_GRAY;
    }
    else if(numChannels == 3 && bitDepth == 8) {
        colorSpace = OPJ_CLRSPC_SRGB;
    }

    image = opj_image_create(numChannels, channelParameters.data(),
        colorSpace);

    uint8_t* data = mat.data;
    std::vector<int32_t*> channelData(numChannels);
    for(int channelIndex=0; channelIndex<numChannels; ++channelIndex) {
        channelData[channelIndex] = image.get()->comps[channelIndex].data;
    }

    switch (depth) {
    case CV_8U:  
        convertTo32bitChannels(data, mat.cols,
            mat.rows, numChannels, channelData.data());
    case CV_8S:
        convertTo32bitChannels(reinterpret_cast<int8_t*>(data), mat.cols,
            mat.rows, numChannels, channelData.data());
    case CV_16U:
        convertTo32bitChannels(reinterpret_cast<uint16_t*>(data), mat.cols,
            mat.rows, numChannels, channelData.data());
    case CV_16S:
        convertTo32bitChannels(reinterpret_cast<int16_t*>(data), mat.cols,
            mat.rows, numChannels, channelData.data());
    case CV_32S:
        convertTo32bitChannels(reinterpret_cast<int32_t*>(data), mat.cols,
            mat.rows, numChannels, channelData.data());
    default:
        RAISE_RUNTIME_ERROR << "Unsupported type for Jpeg2000 conversion: " << depth;
    }
}

void compressRaster(const cv::Mat& mat, const std::string& targetPath, const ConvertJ2KParameters& params)
{
    wopj_cparameters parameters;   /* compression parameters */
    strncpy(parameters.outfile, targetPath.c_str(), OPJ_PATH_LEN);
    ImagePtr image;

    CodecPtr codec = opj_create_compress(OPJ_CODEC_J2K);

    rasterToOPJImage(mat, image, params);
    opj_set_info_handler(codec, info_callback, nullptr);
    opj_set_warning_handler(codec, warning_callback, nullptr);
    opj_set_error_handler(codec, error_callback, nullptr);

    if (!opj_setup_encoder(codec, &parameters, image)) {
        RAISE_RUNTIME_ERROR << "Failed to encode image: opj_setup_encoder.";
    }


    StreamPtr stream = opj_stream_create_default_file_stream(parameters.outfile, OPJ_FALSE);
    if (!stream.get()) {
        RAISE_RUNTIME_ERROR << "Cannot create default file stream.";
    }
    if(!opj_start_compress(codec, image, stream)) {
        RAISE_RUNTIME_ERROR << "Failed to encode image : opj_start_compress.";
    }
    if(!opj_encode(codec, stream)){
        RAISE_RUNTIME_ERROR << "Failed to encode image : opj_encode.";
    }
    if(!opj_end_compress(codec, stream)) {
        RAISE_RUNTIME_ERROR << "Failed to encode image : opj_end_compress.";
    }

}

void convertScene(const CVScenePtr& scene, const std::string& targetPath, const ConvertJ2KParameters& params)
{
    auto sceneRect = scene->getRect();
    sceneRect.x = sceneRect.y = 0;
    cv::Mat sourceRaster;
    scene->readBlock(sceneRect, sourceRaster);
    compressRaster(sourceRaster, targetPath, params);
}

void compress(const std::string& sourcePath, const std::string& driver, const std::string& targetPath, const ConvertJ2KParameters& params)
{
    CVSlidePtr slide = slideio::ImageDriverManager::openSlide(sourcePath, driver);
    CVScenePtr scene = slide->getScene(0);
    convertScene(scene, targetPath, params);
}

int main()
{
    const std::string sourcePath("d:/Projects/slideio/slideio_extra/testdata/cv/slideio/gdal/img_2448x2448_3x8bit_SRC_RGB_ducks.png");
    const std::string targetPath("d:/Temp/a.jp2");
    ConvertJ2KParameters params = { 1,1 };
    compress(sourcePath, "GDAL", targetPath, params);
    return 0;
    
}
