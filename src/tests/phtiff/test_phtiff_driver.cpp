#include <gtest/gtest.h>
#include "tests/testlib/testtools.hpp"
#include <string>
#include <list>
#include <map>
#include <vector>
#include <algorithm>
#include <tinyxml2.h>
#include <opencv2/imgproc.hpp>
#include <slideio/slideio/imagedrivermanager.hpp>
#include "slideio/core/tools/tools.hpp"
#include "slideio/drivers/svs/svsimagedriver.hpp"
#include "slideio/imagetools/smallimage.hpp"
#include "slideio/slideio/slideio.hpp"
#include "slideio/drivers/svs/svsimagedriver.hpp"
#include "slideio/drivers/svs/svsslide.hpp"


namespace slideio
{
    class Slide;
}

using namespace slideio;

// struct ZoomLevelInfo
// {
// 	int level;
// 	Size size;
// 	double scale;
// 	double magnification;
// 	Size tileSize;
// };

// struct SceneInfo
// {
// 	std::string name;
// 	cv::Rect rect;
// 	int numChannels;
// 	int numZSlices;
// 	int numTFrames;
// 	double magnification;
// 	Resolution res;
// 	DataType dt;
// 	Compression compression;
// 	int levels = 0;
// 	int levelInfoIndex = -1;
// 	double zResolution = 0.0;
// 	double tResolution = 0.0;
// };

// Exposes the protected SVSSlide members (phExtractImages/phCreateImageScene/
// phCreateAuxScenes) for unit testing.
class MockSVSSlide : public SVSSlide
{
public:
	static void phExtractImagesMock(const std::vector<TiffDirectory>& directories, std::list<int>& imagePyramid,
		std::map<std::string, int>& auxImages) {
		phExtractImages(directories, imagePyramid, auxImages);
	}
	void phCreateImageSceneMock(const std::vector<TiffDirectory>& directories, const std::list<int>& imagePyramid,
		libtiff::TIFF* hFile) {
		phCreateImageScene(directories, imagePyramid, hFile);
	}
	void phCreateAuxScenesMock(const std::vector<TiffDirectory>& directories,
		const std::map<std::string, int>& auxImages) {
		phCreateAuxScenes(directories, auxImages);
	}
};

// Builds a TiffDirectory carrying just the fields phExtractImages inspects.
static TiffDirectory makeDir(const std::string& description, int width) {
	TiffDirectory dir;
	dir.description = description;
	dir.width = width;
	return dir;
}

// Builds a TiffDirectory with enough raster fields set that scene construction
// (SVSTiledScene/SVSSmallScene) produces sane geometry without a real file.
static TiffDirectory makeImageDir(const std::string& description, int width, int height) {
	TiffDirectory dir;
	dir.description = description;
	dir.width = width;
	dir.height = height;
	dir.channels = 3;
	dir.bitsPerSample = 8;
	dir.tileWidth = 256;
	dir.tileHeight = 256;
	return dir;
}


class PhTiffImageDriverTests : public ::testing::Test {
protected:
	static void SetUpTestSuite() {
		ImageDriverManager::setLogLevel("WARNING");
		std::cerr << "SetUpTestSuite: Running before all tests\n";
	}
	static void TearDownTestSuite() {
	}
};


TEST_F(PhTiffImageDriverTests, canOpenFile) {
    const std::string allowedSuffixes[] = { ".tif",".tiff" };
    const std::string disallowedSuffixes[] = { ".ometif",".ometiff", ".ometf2", ".ometf8", ".omebtf" };
    SVSImageDriver driver("PHTIFF");
	for(std::string suffix : allowedSuffixes) {
		std::string filePath = "/projects/ometiff" + suffix;
		EXPECT_TRUE(driver.canOpenFile(filePath));
	}
	for (std::string suffix : allowedSuffixes) {
        std::transform(suffix.begin(), suffix.end(), suffix.begin(),
            [](unsigned char c) { return std::toupper(c); });
		std::string filePath = "/projects/ometiff" + suffix;
		EXPECT_TRUE(driver.canOpenFile(filePath));
	}
	for (std::string suffix : disallowedSuffixes) {
		std::string filePath = "/projects/ometiff" + suffix;
		EXPECT_FALSE(driver.canOpenFile(filePath));
	}
}

TEST_F(PhTiffImageDriverTests, openSlide) {
	std::string filePath = TestTools::getFullTestImagePath("philips", "Philips-3.tiff");
	auto slide = slideio::openSlide(filePath, "PHTIFF");
	std::list<std::tuple<std::string,int,int>> auxNames = {
	    {"Macro", 791, 403},
	    {"Label", 387, 403}
	};
	ASSERT_TRUE(slide != nullptr);
	EXPECT_EQ(slide->getNumScenes(), 1);
	EXPECT_EQ(slide->getAuxImageNames().size(), auxNames.size());
	auto scene = slide->getScene(0);
	ASSERT_TRUE(scene != nullptr);
	auto sceneRect = scene->getRect();
	EXPECT_EQ(std::get<2>(sceneRect), 131072);
	EXPECT_EQ(std::get<3>(sceneRect), 100352);
	EXPECT_EQ(scene->getNumChannels(), 3);
	EXPECT_EQ(scene->getChannelDataType(0), slideio::DataType::DT_Byte);
	EXPECT_EQ(scene->getCompression(), slideio::Compression::Jpeg);
	auto res = scene->getResolution();
	EXPECT_DOUBLE_EQ(std::get<0>(res), 0.000226891);
	EXPECT_DOUBLE_EQ(std::get<1>(res), 0.000226907);

	for (const auto& param : auxNames) {
		auto auxScene = slide->getAuxImage(std::get<0>(param));
		EXPECT_TRUE(auxScene != nullptr);
		auto rect = auxScene->getRect();
		EXPECT_EQ(std::get<2>(rect), std::get<1>(param));
		EXPECT_EQ(std::get<3>(rect), std::get<2>(param));	
	}
}

// Layout mirrors a real Philips TIFF (see Philips-1.tiff): dir 0 is the base
// image whose description is the "<?xml ..." metadata blob, dirs 1..N are the
// "level=..." pyramid tiers with strictly decreasing width, and there are no
// auxiliary images. The pyramid must come out ordered by decreasing width, so
// directory indices 0..N in order.
TEST_F(PhTiffImageDriverTests, phExtractImages_pyramidOrderedByWidth) {
	const std::vector<TiffDirectory> directories = {
		makeDir("<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<DataObject>", 45056),
		makeDir("level=1 mag=22 quality=80", 22528),
		makeDir("level=2 mag=11 quality=80", 11264),
		makeDir("level=3 mag=5.5 quality=80", 5632),
		makeDir("level=4 mag=2.75 quality=80", 3072),
	};
	std::list<int> imagePyramid;
	std::map<std::string, int> auxImages;

	MockSVSSlide::phExtractImagesMock(directories, imagePyramid, auxImages);

	EXPECT_EQ((std::list<int>{0, 1, 2, 3, 4}), imagePyramid);
	EXPECT_TRUE(auxImages.empty());
}

// The pyramid is sorted by decreasing width regardless of the directory order
// in the file. Descriptions are provided out of width order here.
TEST_F(PhTiffImageDriverTests, phExtractImages_sortsUnorderedInputByDescendingWidth) {
	const std::vector<TiffDirectory> directories = {
		makeDir("level=3 mag=5.5 quality=80", 5632),   // index 0, smallest
		makeDir("<?xml version=\"1.0\"?>", 45056),     // index 1, largest
		makeDir("level=1 mag=22 quality=80", 22528),   // index 2
		makeDir("level=2 mag=11 quality=80", 11264),   // index 3
	};
	std::list<int> imagePyramid;
	std::map<std::string, int> auxImages;

	MockSVSSlide::phExtractImagesMock(directories, imagePyramid, auxImages);

	// Widths 45056 > 22528 > 11264 > 5632 -> indices 1, 2, 3, 0.
	EXPECT_EQ((std::list<int>{1, 2, 3, 0}), imagePyramid);
	EXPECT_TRUE(auxImages.empty());
}

// Trailing non-pyramid directories ("Macro", "Label" in Philips-3.tiff) are
// auxiliary images: keyed by description, mapped to their directory index, and
// kept out of the pyramid. "Label" is the regression case for the old
// find_first_of bug -- it contains 'l' and 'e', so a character-set search would
// wrongly classify it as a "level" pyramid tier.
TEST_F(PhTiffImageDriverTests, phExtractImages_extractsAuxImages) {
	const std::vector<TiffDirectory> directories = {
		makeDir("<?xml version=\"1.0\"?>", 131072),
		makeDir("level=1 mag=22 quality=80", 65536),
		makeDir("level=2 mag=11 quality=80", 32768),
		makeDir("Macro", 791),
		makeDir("Label", 387),
	};
	std::list<int> imagePyramid;
	std::map<std::string, int> auxImages;

	MockSVSSlide::phExtractImagesMock(directories, imagePyramid, auxImages);

	EXPECT_EQ((std::list<int>{0, 1, 2}), imagePyramid);
	ASSERT_EQ(2u, auxImages.size());
	EXPECT_EQ(3, auxImages.at("Macro"));
	EXPECT_EQ(4, auxImages.at("Label"));
}

// A description containing the "level" substring is a pyramid tier even without
// XML; the "<?xml" base image is a pyramid tier even without "level".
TEST_F(PhTiffImageDriverTests, phExtractImages_classifiesBySubstring) {
	const std::vector<TiffDirectory> directories = {
		makeDir("level=5 mag=1.375 quality=80", 1536),
		makeDir("<?xml version=\"1.0\"?>", 4096),
		makeDir("Thumbnail", 256),
	};
	std::list<int> imagePyramid;
	std::map<std::string, int> auxImages;

	MockSVSSlide::phExtractImagesMock(directories, imagePyramid, auxImages);

	EXPECT_EQ((std::list<int>{1, 0}), imagePyramid);
	ASSERT_EQ(1u, auxImages.size());
	EXPECT_EQ(2, auxImages.at("Thumbnail"));
}

// A non-pyramid directory with an empty description is not a named aux image;
// it is skipped rather than inserted under an empty key.
TEST_F(PhTiffImageDriverTests, phExtractImages_skipsEmptyAuxDescription) {
	const std::vector<TiffDirectory> directories = {
		makeDir("<?xml version=\"1.0\"?>", 4096),
		makeDir("", 256),          // empty description -> skipped
		makeDir("Label", 128),
	};
	std::list<int> imagePyramid;
	std::map<std::string, int> auxImages;

	MockSVSSlide::phExtractImagesMock(directories, imagePyramid, auxImages);

	EXPECT_EQ((std::list<int>{0}), imagePyramid);
	ASSERT_EQ(1u, auxImages.size());
	EXPECT_EQ(2, auxImages.at("Label"));
	EXPECT_EQ(0u, auxImages.count(""));
}

// Two aux directories sharing a description must not clobber each other: the
// first occurrence wins and the duplicate is dropped.
TEST_F(PhTiffImageDriverTests, phExtractImages_duplicateAuxDescriptionKeepsFirst) {
	const std::vector<TiffDirectory> directories = {
		makeDir("<?xml version=\"1.0\"?>", 4096),
		makeDir("Macro", 791),     // index 1, kept
		makeDir("Macro", 400),     // index 2, duplicate -> dropped
	};
	std::list<int> imagePyramid;
	std::map<std::string, int> auxImages;

	MockSVSSlide::phExtractImagesMock(directories, imagePyramid, auxImages);

	EXPECT_EQ((std::list<int>{0}), imagePyramid);
	ASSERT_EQ(1u, auxImages.size());
	EXPECT_EQ(1, auxImages.at("Macro"));
}

// Empty input yields empty outputs and does not touch the caller's containers.
TEST_F(PhTiffImageDriverTests, phExtractImages_emptyInput) {
	const std::vector<TiffDirectory> directories;
	std::list<int> imagePyramid;
	std::map<std::string, int> auxImages;

	MockSVSSlide::phExtractImagesMock(directories, imagePyramid, auxImages);

	EXPECT_TRUE(imagePyramid.empty());
	EXPECT_TRUE(auxImages.empty());
}

// phCreateImageScene builds a single tiled "Image" scene out of the directories
// referenced by the pyramid index list and appends it to the slide's scenes.
// A null TIFF handle is fine: scene geometry comes from the directories, and the
// handle is only dereferenced later during raster reads.
TEST_F(PhTiffImageDriverTests, phCreateImageScene_createsSingleImageScene) {
	const std::vector<TiffDirectory> directories = {
		makeImageDir("<?xml version=\"1.0\"?>", 45056, 35840),
		makeImageDir("level=1 mag=22 quality=80", 22528, 17920),
		makeImageDir("level=2 mag=11 quality=80", 11264, 9216),
	};
	const std::list<int> imagePyramid = { 0, 1, 2 };

	MockSVSSlide slide;
	slide.phCreateImageSceneMock(directories, imagePyramid, nullptr);

	ASSERT_EQ(1, slide.getNumScenes());
	auto scene = slide.getScene(0);
	ASSERT_TRUE(scene != nullptr);
	EXPECT_EQ("Image", scene->getName());
	// The scene rect is taken from the first pyramid directory (the base level).
	const cv::Rect rect = scene->getRect();
	EXPECT_EQ(45056, rect.width);
	EXPECT_EQ(35840, rect.height);
	EXPECT_EQ(3, scene->getNumChannels());
}

// The scene is assembled from exactly the directories named by the pyramid list,
// in the given order: the first index becomes the base level that drives the
// scene rect. Here the pyramid references dir 2 first, then dir 0.
TEST_F(PhTiffImageDriverTests, phCreateImageScene_usesPyramidIndicesInOrder) {
	const std::vector<TiffDirectory> directories = {
		makeImageDir("<?xml version=\"1.0\"?>", 45056, 35840),  // index 0
		makeImageDir("level=1 mag=22 quality=80", 22528, 17920), // index 1 (unused)
		makeImageDir("level=2 mag=11 quality=80", 11264, 9216),  // index 2
	};
	const std::list<int> imagePyramid = { 2, 0 };  // base = dir 2

	MockSVSSlide slide;
	slide.phCreateImageSceneMock(directories, imagePyramid, nullptr);

	ASSERT_EQ(1, slide.getNumScenes());
	const cv::Rect rect = slide.getScene(0)->getRect();
	EXPECT_EQ(11264, rect.width);   // dir 2, first in the list
	EXPECT_EQ(9216, rect.height);
}

// phCreateAuxScenes turns each (description -> directory index) entry into a
// named auxiliary scene retrievable by name, and registers the names.
TEST_F(PhTiffImageDriverTests, phCreateAuxScenes_createsNamedAuxScenes) {
	const std::vector<TiffDirectory> directories = {
		makeImageDir("<?xml version=\"1.0\"?>", 131072, 100352),
		makeImageDir("Macro", 791, 403),
		makeImageDir("Label", 387, 403),
	};
	const std::map<std::string, int> auxImages = { {"Macro", 1}, {"Label", 2} };

	MockSVSSlide slide;
	slide.phCreateAuxScenesMock(directories, auxImages);

	EXPECT_EQ(2, slide.getNumAuxImages());
	const std::list<std::string>& names = slide.getAuxImageNames();
	EXPECT_NE(std::find(names.begin(), names.end(), "Macro"), names.end());
	EXPECT_NE(std::find(names.begin(), names.end(), "Label"), names.end());

	auto macro = slide.getAuxImage("Macro");
	ASSERT_TRUE(macro != nullptr);
	EXPECT_EQ("Macro", macro->getName());
	EXPECT_EQ(791, macro->getRect().width);
	EXPECT_EQ(403, macro->getRect().height);

	auto label = slide.getAuxImage("Label");
	ASSERT_TRUE(label != nullptr);
	EXPECT_EQ("Label", label->getName());
	EXPECT_EQ(387, label->getRect().width);
}

// An empty aux map creates no auxiliary scenes.
TEST_F(PhTiffImageDriverTests, phCreateAuxScenes_emptyMapCreatesNothing) {
	const std::vector<TiffDirectory> directories = {
		makeImageDir("<?xml version=\"1.0\"?>", 4096, 4096),
	};
	const std::map<std::string, int> auxImages;

	MockSVSSlide slide;
	slide.phCreateAuxScenesMock(directories, auxImages);

	EXPECT_EQ(0, slide.getNumAuxImages());
	EXPECT_TRUE(slide.getAuxImageNames().empty());
}

