// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <gtest/gtest.h>
#include "slideio/base/exceptions.hpp"
#include "slideio/imagetools/tifffiles.hpp"
#include "tests/testlib/testtools.hpp"
#include "memorystream.hpp"
#include <fstream>
#include <list>
#include <vector>

namespace {

std::vector<uint8_t> readFileBytes(const std::string& path)
{
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    EXPECT_TRUE(in.is_open()) << "Cannot open test file: " << path;
    const std::streamsize sz = in.tellg();
    in.seekg(0);
    std::vector<uint8_t> bytes(static_cast<size_t>(sz));
    in.read(reinterpret_cast<char*>(bytes.data()), sz);
    return bytes;
}

} // namespace

class TIFFFilesTest : public ::testing::Test {
protected:
    void SetUp() override {
    }

    void TearDown() override {
    }
    std::list<std::string> testFiles = {
        TestTools::getFullTestImagePath("svs","CMU-1-Small-Region.svs"),
        TestTools::getFullTestImagePath("ometiff","Subresolutions/retina_large.ome.tiff"),
        TestTools::getFullTestImagePath("ometiff","Subresolutions/Leica-2.ome.tiff"),
        TestTools::getFullTestImagePath("ometiff","Subresolutions/Leica-1.ome.tiff"),
        TestTools::getFullTestImagePath("ometiff","SPIM-ModuloAlongZ.ome.tiff"),
        TestTools::getFullTestImagePath("ometiff","LAMBDA-ModuloAlongZ-ModuloAlongT.ome.tiff")
    };
    slideio::TIFFFiles tiffFiles;
};

TEST_F(TIFFFilesTest, GetOrOpen_FileNotFound) {
    EXPECT_THROW(tiffFiles.getOrOpen("nonexistent_file.tiff"), slideio::RuntimeError);
}

TEST_F(TIFFFilesTest, GetOrOpen_FileOpenedSuccessfully) {
	std::string testFilePath = testFiles.front();
    libtiff::TIFF* tiff1 = tiffFiles.getOrOpen(testFilePath);
    ASSERT_NE(tiff1, nullptr);
    libtiff::TIFF* tiff2 = tiffFiles.getOrOpen(testFilePath);
	ASSERT_EQ(tiff1, tiff2);
	EXPECT_EQ(tiffFiles.getNumberOfOpenFiles(), 1);
    tiffFiles.close(testFilePath);
	EXPECT_EQ(tiffFiles.getNumberOfOpenFiles(), 0);
}

TEST_F(TIFFFilesTest, Close_FileClosedSuccessfully) {
    std::string testFilePath = testFiles.front();
    tiffFiles.getOrOpen(testFilePath);
    tiffFiles.close(testFilePath);
    EXPECT_EQ(0, tiffFiles.getNumberOfOpenFiles());
}

TEST_F(TIFFFilesTest, CloseAll_AllFilesClosedSuccessfully) {
	for (const auto& filePath : testFiles) {
		tiffFiles.getOrOpen(filePath);
	}
	EXPECT_EQ(testFiles.size(), tiffFiles.getNumberOfOpenFiles());
    EXPECT_EQ(testFiles.size(), tiffFiles.getOpenFileCounter());

    for (const auto& filePath : testFiles) {
        tiffFiles.getOrOpen(filePath);
    }
    EXPECT_EQ(testFiles.size(), tiffFiles.getNumberOfOpenFiles());
    EXPECT_EQ(testFiles.size(), tiffFiles.getOpenFileCounter());


    tiffFiles.close(testFiles.front());
    EXPECT_EQ(testFiles.size()-1, tiffFiles.getNumberOfOpenFiles());
    EXPECT_EQ(testFiles.size()-1, tiffFiles.getOpenFileCounter());
    tiffFiles.closeAll();
    EXPECT_EQ(0, tiffFiles.getNumberOfOpenFiles());
    EXPECT_EQ(0, tiffFiles.getOpenFileCounter());
}

// A single-file OME-TIFF references itself by filename in its XML metadata. Over
// a stream, the OME-TIFF driver resolves that self-reference with siblingUri(),
// which strips the presigned query string -- so getOrOpen() sees the query-less
// URI while the main stream URI still carries "?X-Amz-...". The two must be
// recognized as the same object (compared ignoring the query string) and served
// from the stream, not misclassified as a sibling (multi-file) and rejected.
TEST_F(TIFFFilesTest, GetOrOpen_StreamSelfReferenceIgnoresQueryString) {
    const std::string svsPath = TestTools::getTestImagePath("svs", "CMU-1-Small-Region.svs");
    const std::vector<uint8_t> bytes = readFileBytes(svsPath);
    const std::string mainUri =
        "https://bucket.s3.eu-central-1.amazonaws.com/CMU-1-Small-Region.svs"
        "?X-Amz-Algorithm=AWS4-HMAC-SHA256&X-Amz-Signature=deadbeef";
    const std::string selfRef =
        "https://bucket.s3.eu-central-1.amazonaws.com/CMU-1-Small-Region.svs";

    auto stream = std::make_shared<slideio::tests::MemoryStream>(bytes, mainUri);
    slideio::TIFFFiles files;
    files.setMainStream(mainUri, stream);

    libtiff::TIFF* tiff = files.getOrOpen(selfRef);
    EXPECT_NE(tiff, nullptr);
    EXPECT_EQ(1, files.getNumberOfOpenFiles());
}

// A genuinely different referenced file over a stream is a multi-file OME-TIFF,
// which is unsupported in v1. The guard must still reject it.
TEST_F(TIFFFilesTest, GetOrOpen_StreamRejectsDifferentSibling) {
    const std::string svsPath = TestTools::getTestImagePath("svs", "CMU-1-Small-Region.svs");
    const std::vector<uint8_t> bytes = readFileBytes(svsPath);
    const std::string mainUri =
        "https://bucket.s3.eu-central-1.amazonaws.com/main.ome.tiff?X-Amz-Signature=deadbeef";
    const std::string sibling =
        "https://bucket.s3.eu-central-1.amazonaws.com/other.ome.tiff";

    auto stream = std::make_shared<slideio::tests::MemoryStream>(bytes, mainUri);
    slideio::TIFFFiles files;
    files.setMainStream(mainUri, stream);

    EXPECT_THROW(files.getOrOpen(sibling), slideio::RuntimeError);
}