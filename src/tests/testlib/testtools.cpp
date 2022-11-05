#include <gtest/gtest.h>
#include "testtools.hpp"


#include <fstream>
#include <numeric>
#include <boost/filesystem/path.hpp>
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/core/mat.hpp>
#include "slideio/core/tools/exceptions.hpp"
#include <png.h>

static const char* TEST_PATH_VARIABLE = "SLIDEIO_TEST_DATA_PATH";
static const char* PRIV_TEST_PATH_VARIABLE = "SLIDEIO_TEST_DATA_PRIV_PATH";
static const char* TEST_FULL_TEST_PATH_VARIABLE = "SLIDEIO_IMAGES_PATH";


bool TestTools::isPrivateTestEnabled()
{
    const char* var = getenv(PRIV_TEST_PATH_VARIABLE);
    return var != nullptr;
}

bool TestTools::isFullTestEnabled()
{
    const char* var = getenv(TEST_FULL_TEST_PATH_VARIABLE);
    return var != nullptr;
}

std::string TestTools::getTestImageDirectory(bool priv)
{
    const char *varName = priv ? PRIV_TEST_PATH_VARIABLE : TEST_PATH_VARIABLE;
    const char* var = getenv(varName);
    if(var==nullptr)
        throw std::runtime_error(
            std::string("Undefined environment variable: " + std::string(varName)));
    std::string testDirPath(var);
    return testDirPath;
}


std::string TestTools::getTestImagePath(const std::string& subfolder, const std::string& image, bool priv)
{
    std::string imagePath(getTestImageDirectory(priv));
    if(!subfolder.empty())
        imagePath += std::string("/") + subfolder;
    imagePath += std::string("/") +  image;
    return boost::filesystem::path(imagePath).lexically_normal().string();
}

std::string TestTools::getFullTestImagePath(const std::string& subfolder, const std::string& image)
{
    const char* varName = TEST_FULL_TEST_PATH_VARIABLE;
    const char* var = getenv(varName);
    if (var == nullptr)
        throw std::runtime_error(
            std::string("Undefined environment variable: " + std::string(varName)));
    std::string imagePath(var);
    if (!subfolder.empty())
        imagePath += std::string("/") + subfolder;
    imagePath += std::string("/") + image;
    return boost::filesystem::path(imagePath).lexically_normal().string();
}


void TestTools::readRawImage(std::string& path, cv::Mat& image)
{
    std::ifstream is;
    is.open(path, std::ios::binary);
    is.seekg(0, std::ios::end);
    auto length = is.tellg();
    is.seekg(0, std::ios::beg);
    is.read((char*)image.data, image.total() * image.elemSize());
    is.close();
}

void TestTools::compareRasters(cv::Mat& raster1, cv::Mat& raster2)
{
    cv::Mat diff = raster1 != raster2;
    // Equal if no elements disagree
    double min(1.), max(1.);
    cv::minMaxLoc(diff, &min, &max);
    EXPECT_EQ(min, 0);
    EXPECT_EQ(max, 0);
}

void TestTools::showRaster(cv::Mat& raster)
{
    cv::namedWindow("Display window", cv::WINDOW_AUTOSIZE);
    cv::imshow("Display window", raster);
    cv::waitKey(0);
}

void TestTools::writePNG(cv::Mat raster, const std::string& filePath)
{
    /* create file */
    FILE* fp = fopen(filePath.c_str(), "wb");
    if (!fp) {
        RAISE_RUNTIME_ERROR << "File " << filePath << " could not be opened for writing";
    }

    try {
        /* initialize stuff */
        png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

        if (!png_ptr) {
            RAISE_RUNTIME_ERROR << "png_create_write_struct failed";
        }

        png_infop info_ptr = png_create_info_struct(png_ptr);
        if (!info_ptr) {
            RAISE_RUNTIME_ERROR << "png_create_info_struct failed";
        }

        if (setjmp(png_jmpbuf(png_ptr))) {
            RAISE_RUNTIME_ERROR << "Error during init_io";
        }

        png_init_io(png_ptr, fp);


        /* write header */
        if (setjmp(png_jmpbuf(png_ptr))) {
            RAISE_RUNTIME_ERROR << "Error during writing header";
        }
        const int width = raster.cols;
        const int height = raster.rows;
        const int bitDepth = 8;
        const int colorType = (raster.channels() == 3) ? PNG_COLOR_TYPE_RGB : PNG_COLOR_TYPE_GRAY;
        png_set_IHDR(png_ptr, info_ptr, width, height,
            bitDepth, colorType, PNG_INTERLACE_NONE,
            PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

        png_write_info(png_ptr, info_ptr);


        /* write bytes */
        if (setjmp(png_jmpbuf(png_ptr))) {
            RAISE_RUNTIME_ERROR << "Error during writing bytes";
        }
        if (!raster.isContinuous()) {
            RAISE_RUNTIME_ERROR << "Continuous raster expected";
        }
        std::vector<uint8_t*> rows(raster.rows);
        const size_t stride = raster.elemSize() * raster.cols;
        size_t offset = 0;
        for (int row = 0; row < raster.rows; ++row, offset += stride) {
            rows[row] = raster.data + offset;
        }

        png_write_image(png_ptr, rows.data());


        /* end write */
        if (setjmp(png_jmpbuf(png_ptr))) {
            RAISE_RUNTIME_ERROR << "Error during end of write";
        }

        png_write_end(png_ptr, NULL);
    }
    catch (std::exception&) {
        if (fp)
            fclose(fp);
        throw;
    }

    if (fp)
        fclose(fp);
}

void TestTools::readPNG(const std::string& filePath, cv::OutputArray output)
{
    char header[8]; // 8 is the maximum size that can be checked

    /* open file and test for it being a png */
    FILE* fp = fopen(filePath.c_str(), "rb");
    if (!fp) {
        RAISE_RUNTIME_ERROR << "File " << filePath << " could not be opened for reading.";
    }

    try {
        fread(header, 1, 8, fp);

        if (png_sig_cmp(reinterpret_cast<png_const_bytep>(header), 0, 8)) {
            RAISE_RUNTIME_ERROR << "File " << filePath << "bis not recognized as a PNG file";
        }


        /* initialize stuff */
        png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

        if (!png_ptr) {
            RAISE_RUNTIME_ERROR << "[read_png_file] png_create_read_struct failed";
        }

        png_infop info_ptr = png_create_info_struct(png_ptr);
        if (!info_ptr) {
            RAISE_RUNTIME_ERROR << "[read_png_file] png_create_info_struct failed";
        }

        if (setjmp(png_jmpbuf(png_ptr))) {
            RAISE_RUNTIME_ERROR << "[read_png_file] Error during init_io";
        }

        png_init_io(png_ptr, fp);
        png_set_sig_bytes(png_ptr, 8);

        png_read_info(png_ptr, info_ptr);

        png_uint_32 width = png_get_image_width(png_ptr, info_ptr);
        png_uint_32 height = png_get_image_height(png_ptr, info_ptr);
        png_byte bitDepth = png_get_bit_depth(png_ptr, info_ptr);
        uint8_t dataType = (bitDepth > 8) ? CV_16U : CV_8U;

        png_read_update_info(png_ptr, info_ptr);
        int channels = png_get_channels(png_ptr, info_ptr);

        /* read file */
        if (setjmp(png_jmpbuf(png_ptr))) {
            RAISE_RUNTIME_ERROR << "[read_png_file] Error during read_image";
        }

        output.create(height, width, CV_MAKETYPE(dataType, channels));
        cv::Mat raster = output.getMat();

        std::vector<uint8_t*> rows(raster.rows);
        const size_t stride = raster.elemSize() * raster.cols;
        size_t offset = 0;
        for (int row = 0; row < raster.rows; ++row, offset += stride) {
            rows[row] = raster.data + offset;
        }

        png_read_image(png_ptr, rows.data());
    }
    catch (std::exception&) {
        if (fp) {
            fclose(fp);
        }
        throw;
    }

    fclose(fp);
}

