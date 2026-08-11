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
#include "slideio/drivers/svs/phtdescription.hpp"
#include "slideio/base/exceptions.hpp"
#include <type_traits>
#include <sstream>
#include <iomanip>
#include <cmath>


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

// --- Philips xml metadata generation ------------------------------------
// Values that createFakeXml does not take as a parameter. They are the ones a
// real Philips file carries (see Philips-3.tiff and Philips-4.tiff).
namespace phDefaults
{
	const int WIDTH = 91136;                 // size of the base zoom level
	const int HEIGHT = 68096;
	const int LEVELS = 9;                    // number of zoom levels of the pyramid
	const double PIXEL_SPACING = 0.00025;    // mm per pixel of the base zoom level
	const char* MANUFACTURER_NAME = "PHILIPS";
	const char* SOFTWARE_VERSIONS_VALUE = "\"1.6.6186\" \"20150402_R48\" \"4.0.3\"";
	const char* INTERFACE_VERSION = "5.0";
	const char* SOURCE_FILE_NAME = "%FILENAME%";
	const char* COMPRESSION = "01";
	const char* COMPRESSION_METHOD = "\"PHILIPS_TIFF_1_0\"";
	const char* COMPRESSION_RATIO = "\"3\"";
	const char* PHOTOMETRIC = "RGB";
	const int SAMPLES = 3;
	const int BITS = 8;
	// Stands for the base64 encoded jpeg of an auxiliary image.
	const char* IMAGE_DATA_VALUE = "/9j/4AAQSkZJRgABAQAAAQABAAD/2wBDAAgGBgcGBQgHBwcJ";
}

static std::string phIndent(int level) {
	return std::string(static_cast<size_t>(4 * level), ' ');
}

// Formats a double the way the philips software does: no trailing zeros.
static std::string phDouble(double value) {
	std::ostringstream stream;
	stream << std::setprecision(9) << value;
	return stream.str();
}

// Emits one <Attribute> element. The name, the group and the element ids are
// taken from the attribute constants so that the generated metadata cannot drift
// away from the definitions in phtdescription.hpp.
static std::string phAttribute(const PHTDescription::Attribute& attribute, const std::string& type,
	const std::string& value, int indent) {
	std::ostringstream stream;
	stream << phIndent(indent)
		<< "<Attribute Name=\"" << attribute.Name
		<< "\" Group=\"" << attribute.Group
		<< "\" Element=\"" << attribute.Element
		<< "\" PMSVR=\"" << type << "\">" << value << "</Attribute>\n";
	return stream.str();
}

static std::string phAttribute(const PHTDescription::Attribute& attribute, const std::string& type,
	int value, int indent) {
	return phAttribute(attribute, type, std::to_string(value), indent);
}

// Emits the opening lines of an attribute holding an array of data objects.
static std::string phOpenArray(const PHTDescription::Attribute& attribute, int indent) {
	std::ostringstream stream;
	stream << phIndent(indent)
		<< "<Attribute Name=\"" << attribute.Name
		<< "\" Group=\"" << attribute.Group
		<< "\" Element=\"" << attribute.Element
		<< "\" PMSVR=\"IDataObjectArray\">\n"
		<< phIndent(indent + 1) << "<Array>\n";
	return stream.str();
}

static std::string phCloseArray(int indent) {
	return phIndent(indent + 1) + "</Array>\n" + phIndent(indent) + "</Attribute>\n";
}

// Size of a zoom level: every level halves the previous one, down to one pixel.
// Non positive sizes are passed through unchanged so that a test can ask for a
// degenerate image.
static int phLevelSize(int size, int level) {
	return (size > 0) ? std::max(1, size >> level) : size;
}

// Exposes the protected SVSSlide members (phExtractImages/phCreateImageScene/
// phCreateAuxScenes) for unit testing.
class MockSVSSlide : public SVSSlide
{
public:
	static void phExtractImagesMock(const std::vector<TiffDirectory>& directories, std::vector<PHTLevel>& imagePyramid,
		std::map<std::string, int>& auxImages) {
		phExtractImages(directories, imagePyramid, auxImages);
	}
	void phCreateImageSceneMock(const std::vector<TiffDirectory>& directories, const std::vector<PHTLevel>& imagePyramid,
		libtiff::TIFF* hFile) {
		phCreateImageScene(directories, imagePyramid, hFile);
	}
	void phCreateAuxScenesMock(const std::vector<TiffDirectory>& directories,
		const std::map<std::string, int>& auxImages) {
		phCreateAuxScenes(directories, auxImages);
	}
	const static std::string fakeXML;

	// Builds the xml metadata of a philips tiff slide: a DPUfsImport root holding
	// one WSI scanned image with a pyramid of `levels` zoom levels, followed by one
	// scanned image per entry of `auxNames` (e.g. {"LABELIMAGE", "MACROIMAGE"}).
	// The base zoom level is `width` x `height`, every further level halves it and
	// doubles the pixel spacing. Everything that is not a parameter -- manufacturer,
	// software versions, pixel format, compression, base pixel spacing -- gets a
	// default value from the phDefaults namespace.
	static std::string createFakeXml(int width = phDefaults::WIDTH, int height = phDefaults::HEIGHT,
		int levels = phDefaults::LEVELS, const std::list<std::string>& auxNames = std::list<std::string>()) {
		std::ostringstream xml;
		xml << "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n";
		xml << "<DataObject ObjectType=\"DPUfsImport\">\n";
		xml << phAttribute(MANUFACTURER, "IString", phDefaults::MANUFACTURER_NAME, 1);
		xml << phAttribute(SOFTWARE_VERSIONS, "IStringArray", phDefaults::SOFTWARE_VERSIONS_VALUE, 1);
		xml << phAttribute(UFS_INTERFACE_VERSION, "IString", phDefaults::INTERFACE_VERSION, 1);
		xml << phOpenArray(SCANNED_IMAGES, 1);

		// The whole slide image and its pyramid.
		xml << phIndent(3) << "<DataObject ObjectType=\"" << SCANNED_IMAGE << "\">\n";
		xml << phAttribute(IMAGE_TYPE, "IString", WSI, 4);
		xml << phAttribute(PIXEL_TRANSFORMATION_METHOD, "IString", "0", 4);
		xml << phAttribute(SAMPLES_PER_PIXEL, "IUInt16", phDefaults::SAMPLES, 4);
		xml << phAttribute(PHOTOMETRIC_INTERPRETATION, "IString", phDefaults::PHOTOMETRIC, 4);
		xml << phAttribute(PLANAR_CONFIGURATION, "IUInt16", 0, 4);
		xml << phAttribute(BITS_ALLOCATED, "IUInt16", phDefaults::BITS, 4);
		xml << phAttribute(BITS_STORED, "IUInt16", phDefaults::BITS, 4);
		xml << phAttribute(HIGH_BIT, "IUInt16", phDefaults::BITS - 1, 4);
		xml << phAttribute(PIXEL_REPRESENTATION, "IUInt16", 0, 4);
		xml << phAttribute(LOSSY_IMAGE_COMPRESSION, "IString", phDefaults::COMPRESSION, 4);
		xml << phAttribute(LOSSY_IMAGE_COMPRESSION_METHOD, "IStringArray", phDefaults::COMPRESSION_METHOD, 4);
		xml << phAttribute(LOSSY_IMAGE_COMPRESSION_RATIO, "IDoubleArray", phDefaults::COMPRESSION_RATIO, 4);
		xml << phAttribute(IMAGE_RESOLUTION, "IDoubleArray", phSpacing(0), 4);
		xml << phOpenArray(LEVEL_SEQUENCE, 4);
		for (int level = 0; level < levels; ++level) {
			xml << phIndent(6) << "<DataObject ObjectType=\"" << PIXEL_DATA_REPRESENTATION << "\">\n";
			xml << phAttribute(IMAGE_RESOLUTION, "IDoubleArray", phSpacing(level), 7);
			xml << phAttribute(LEVEL_POSITION, "IDoubleArray", "\"0\" \"0\" \"0\"", 7);
			xml << phAttribute(LEVEL_COLUMNS, "IUInt32", phLevelSize(width, level), 7);
			xml << phAttribute(LEVEL_NUMBER, "IUInt16", level, 7);
			xml << phAttribute(LEVEL_ROWS, "IUInt32", phLevelSize(height, level), 7);
			xml << phIndent(6) << "</DataObject>\n";
		}
		xml << phCloseArray(4);
		xml << phAttribute(IMAGE_COLUMNS, "IUInt32", width, 4);
		xml << phAttribute(IMAGE_ROWS, "IUInt32", height, 4);
		xml << phAttribute(SOURCE_FILE, "IString", phDefaults::SOURCE_FILE_NAME, 4);
		xml << phIndent(3) << "</DataObject>\n";

		// The auxiliary images carry their raster inline instead of a pyramid.
		for (const std::string& auxName : auxNames) {
			xml << phIndent(3) << "<DataObject ObjectType=\"" << SCANNED_IMAGE << "\">\n";
			xml << phAttribute(IMAGE_TYPE, "IString", auxName, 4);
			xml << phAttribute(IMAGE_DATA, "IString", phDefaults::IMAGE_DATA_VALUE, 4);
			xml << phIndent(3) << "</DataObject>\n";
		}

		xml << phCloseArray(1);
		xml << "</DataObject>\n";
		return xml.str();
	}
private:
	// Pixel spacing of a zoom level, as a quoted pair of millimeter values.
	static std::string phSpacing(int level) {
		const std::string value = phDouble(phDefaults::PIXEL_SPACING * std::pow(2., level));
		return "\"" + value + "\" \"" + value + "\"";
	}
};

const std::string MockSVSSlide::fakeXML = "<?xml version=\"1.0\"?><DataObject ObjectType=\"DPUfsImport\"/>";

// The tiff directory indices of a pyramid, in level order.
static std::vector<int> phDirIndices(const std::vector<PHTLevel>& imagePyramid) {
	std::vector<int> indices;
	indices.reserve(imagePyramid.size());
	for (const PHTLevel& level : imagePyramid) {
		indices.push_back(level.dirIndex);
	}
	return indices;
}

// Builds a TiffDirectory carrying just the fields phExtractImages inspects: the
// description, the size and whether the directory is tiled. Philips stores the zoom
// levels of the pyramid tiled and the auxiliary images striped, so `tiled` is what
// tells the two apart.
static TiffDirectory makeDir(const std::string& description, int width, int height, bool tiled) {
	TiffDirectory dir;
	dir.description = description;
	dir.width = width;
	dir.height = height;
	dir.tiled = tiled;
	return dir;
}

// A zoom level: tiled, sized as philips stores it (padded up to the tile grid).
static TiffDirectory makeLevelDir(const std::string& description, int width, int height) {
	return makeDir(description, width, height, true);
}

// An auxiliary image: striped, named by its description.
static TiffDirectory makeAuxDir(const std::string& description, int width, int height) {
	return makeDir(description, width, height, false);
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
	// The extension is necessary but not sufficient: philips shares *.tif;*.tiff with
	// gdal and with ome-tiff, so the driver has to look into the file.
	SVSImageDriver driver(PHTIFF_DRIVER_ID);
	EXPECT_TRUE(driver.canOpenFile(TestTools::getFullTestImagePath("philips", "Philips-3.tiff")));
	EXPECT_FALSE(driver.canOpenFile(
		TestTools::getTestImagePath("gdal", "img_2448x2448_3x16bit_SRC_RGB_ducks.tif")));
	EXPECT_FALSE(driver.canOpenFile(TestTools::getTestImagePath("gdal", "multipage.tif")));
	EXPECT_FALSE(driver.canOpenFile(TestTools::getFullTestImagePath("ometiff", "00001_01.ome.tiff")));
	// A path that does not exist, and a file that is not a tiff at all.
	EXPECT_FALSE(driver.canOpenFile("/projects/no-such-file.tiff"));
	EXPECT_FALSE(driver.canOpenFile(TestTools::getTestImagePath("gdal", "colors.png")));

	// Extensions the driver does not serve are rejected without opening anything.
	const std::string disallowedSuffixes[] = {
		".ometif", ".ometiff", ".ometf2", ".ometf8", ".omebtf", ".svs", ".ndpi", ".qptiff"
	};
	for (const std::string& suffix : disallowedSuffixes) {
		EXPECT_FALSE(driver.canOpenFile("/projects/image" + suffix)) << suffix;
	}

	// The svs id decides by extension alone: an svs file carries no philips metadata.
	// This is also the only place where extension matching, and its case insensitivity,
	// is observable without a file, since no content test runs for the svs id.
	SVSImageDriver svsDriver(SVS_DRIVER_ID);
	EXPECT_TRUE(svsDriver.canOpenFile("/projects/image.svs"));
	EXPECT_TRUE(svsDriver.canOpenFile("/projects/image.SVS"));
	EXPECT_FALSE(svsDriver.canOpenFile("/projects/image.tiff"));
	EXPECT_TRUE(svsDriver.canOpenFile(TestTools::getTestImagePath("svs", "CMU-1-Small-Region.svs")));
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
	EXPECT_DOUBLE_EQ(std::get<0>(res), 0.000226891e-3);
	EXPECT_DOUBLE_EQ(std::get<1>(res), 0.000226907e-3);

	for (const auto& param : auxNames) {
		auto auxScene = slide->getAuxImage(std::get<0>(param));
		EXPECT_TRUE(auxScene != nullptr);
		auto rect = auxScene->getRect();
		EXPECT_EQ(std::get<2>(rect), std::get<1>(param));
		EXPECT_EQ(std::get<3>(rect), std::get<2>(param));	
	}
}

// The tiled directories form the pyramid, the striped ones are the auxiliary images,
// keyed by the name in their own description (the layout of Philips-3.tiff).
TEST_F(PhTiffImageDriverTests, phExtractImages_extractsAuxImages) {
	const std::string xml = MockSVSSlide::createFakeXml(131072, 100352, 3, { "MACROIMAGE", "LABELIMAGE" });
	const std::vector<TiffDirectory> directories = {
		makeLevelDir(xml, 131072, 100352),
		makeLevelDir("level=1 mag=22 quality=80", 65536, 50176),
		makeLevelDir("level=2 mag=11 quality=80", 32768, 25088),
		makeAuxDir("Macro", 791, 403),
		makeAuxDir("Label", 387, 403),
	};
	std::vector<PHTLevel> imagePyramid;
	std::map<std::string, int> auxImages;

	MockSVSSlide::phExtractImagesMock(directories, imagePyramid, auxImages);

	EXPECT_EQ((std::vector<int>{0, 1, 2}), phDirIndices(imagePyramid));
	EXPECT_EQ((std::vector<PHTLevel>{{0, 0}, {1, 1}, {2, 2}}), imagePyramid);
	ASSERT_EQ(2u, auxImages.size());
	EXPECT_EQ(3, auxImages.at("Macro"));
	EXPECT_EQ(4, auxImages.at("Label"));
}

// The philips metadata is not an index into the file: Philips-4.tiff declares a label
// image and a macro image but stores only one auxiliary directory, the macro. Reading
// the two in the declared order hands the macro raster out under the name of the label
// and drops the macro. The name has to come from the directory that holds the raster.
TEST_F(PhTiffImageDriverTests, phExtractImages_namesAuxImagesAfterTheirOwnDirectory) {
	const std::string xml = MockSVSSlide::createFakeXml(91136, 68096, 2, { "LABELIMAGE", "MACROIMAGE" });
	const std::vector<TiffDirectory> directories = {
		makeLevelDir(xml, 91136, 68096),
		// createFakeXml declares the levels unpadded, so the level directories follow it
		// here; the padded case belongs to the tile padding tests.
		makeLevelDir("level=1 mag=20 quality=80", 45568, 34048),
		// The only auxiliary directory of the file, and it is the macro image.
		makeAuxDir("Macro -offset=(0,0)-pixelsize=(0.0315,0.0315)-rois=((0,32000,36000,36000))", 1816, 821),
	};
	std::vector<PHTLevel> imagePyramid;
	std::map<std::string, int> auxImages;

	MockSVSSlide::phExtractImagesMock(directories, imagePyramid, auxImages);

	EXPECT_EQ((std::vector<int>{0, 1}), phDirIndices(imagePyramid));
	ASSERT_EQ(1u, auxImages.size());
	EXPECT_EQ(2, auxImages.at("Macro"));
	EXPECT_EQ(0u, auxImages.count("Label"));
}

// Auxiliary images are named after the image kinds slideio shares with the other
// drivers, whatever case the scanner wrote and whatever it appended to the kind.
TEST_F(PhTiffImageDriverTests, phExtractImages_usesCanonicalAuxImageNames) {
	const std::string xml = MockSVSSlide::createFakeXml(4096, 4096, 1, {});
	const std::vector<TiffDirectory> directories = {
		makeLevelDir(xml, 4096, 4096),
		makeAuxDir("MACRO -offset=(0,0)", 791, 403),
		makeAuxDir("label", 387, 403),
		makeAuxDir("Thumbnail", 256, 256),
	};
	std::vector<PHTLevel> imagePyramid;
	std::map<std::string, int> auxImages;

	MockSVSSlide::phExtractImagesMock(directories, imagePyramid, auxImages);

	ASSERT_EQ(3u, auxImages.size());
	EXPECT_EQ(1, auxImages.at("Macro"));
	EXPECT_EQ(2, auxImages.at("Label"));
	EXPECT_EQ(3, auxImages.at("Thumbnail"));
}

// An auxiliary image of an unknown kind keeps the leading word of its description
// rather than being dropped.
TEST_F(PhTiffImageDriverTests, phExtractImages_keepsUnknownAuxKinds) {
	const std::string xml = MockSVSSlide::createFakeXml(4096, 4096, 1, {});
	const std::vector<TiffDirectory> directories = {
		makeLevelDir(xml, 4096, 4096),
		makeAuxDir("Overview -offset=(0,0)", 791, 403),
		makeAuxDir("", 100, 100),          // nothing to name it after -> dropped
	};
	std::vector<PHTLevel> imagePyramid;
	std::map<std::string, int> auxImages;

	MockSVSSlide::phExtractImagesMock(directories, imagePyramid, auxImages);

	ASSERT_EQ(1u, auxImages.size());
	EXPECT_EQ(1, auxImages.at("Overview"));
}

// Two aux directories of the same kind must not clobber each other: the first
// occurrence wins and the duplicate is dropped.
TEST_F(PhTiffImageDriverTests, phExtractImages_duplicateAuxDescriptionKeepsFirst) {
	const std::string xml = MockSVSSlide::createFakeXml(4096, 4096, 1, { "MACROIMAGE", "MACROIMAGE" });
	const std::vector<TiffDirectory> directories = {
		makeLevelDir(xml, 4096, 4096),
		makeAuxDir("Macro", 791, 403),     // index 1, kept
		makeAuxDir("Macro", 400, 200),     // index 2, duplicate -> dropped
	};
	std::vector<PHTLevel> imagePyramid;
	std::map<std::string, int> auxImages;

	MockSVSSlide::phExtractImagesMock(directories, imagePyramid, auxImages);

	EXPECT_EQ((std::vector<int>{0}), phDirIndices(imagePyramid));
	ASSERT_EQ(1u, auxImages.size());
	EXPECT_EQ(1, auxImages.at("Macro"));
}

// A pyramid directory the philips metadata does not account for is left out of the
// pyramid: without a level number the area it covers is unknown.
TEST_F(PhTiffImageDriverTests, phExtractImages_ignoresUndeclaredPyramidDirectories) {
	const std::string xml = MockSVSSlide::createFakeXml(4096, 4096, 2, {});
	const std::vector<TiffDirectory> directories = {
		makeLevelDir(xml, 4096, 4096),
		makeLevelDir("level=1 mag=20 quality=80", 2048, 2048),
		makeLevelDir("level=2 mag=10 quality=80", 1024, 1024),  // not declared in the xml
	};
	std::vector<PHTLevel> imagePyramid;
	std::map<std::string, int> auxImages;

	MockSVSSlide::phExtractImagesMock(directories, imagePyramid, auxImages);

	EXPECT_EQ((std::vector<int>{0, 1}), phDirIndices(imagePyramid));
	EXPECT_TRUE(auxImages.empty());
}

// Empty input yields empty outputs and does not touch the caller's containers.
TEST_F(PhTiffImageDriverTests, phExtractImages_emptyInput) {
	const std::vector<TiffDirectory> directories;
	std::vector<PHTLevel> imagePyramid;
	std::map<std::string, int> auxImages;

	EXPECT_THROW(MockSVSSlide::phExtractImagesMock(directories, imagePyramid, auxImages), slideio::RuntimeError);
}

// phCreateImageScene builds a single tiled "Image" scene out of the directories
// referenced by the pyramid index list and appends it to the slide's scenes.
// A null TIFF handle is fine: scene geometry comes from the directories, and the
// handle is only dereferenced later during raster reads.
TEST_F(PhTiffImageDriverTests, phCreateImageScene_createsSingleImageScene) {
	const std::string xml = MockSVSSlide::createFakeXml(35840, 30720, 3, {});
	const std::vector<TiffDirectory> directories = {
		makeImageDir(xml, 35840, 30720),
		makeImageDir("level=1 mag=22 quality=80", 22528, 17920),
		makeImageDir("level=2 mag=11 quality=80", 11264, 9216),
	};
	const std::vector<PHTLevel> imagePyramid = { {0, 0}, {1, 1}, {2, 2} };

	MockSVSSlide slide;
	slide.phCreateImageSceneMock(directories, imagePyramid, nullptr);

	ASSERT_EQ(1, slide.getNumScenes());
	auto scene = slide.getScene(0);
	ASSERT_TRUE(scene != nullptr);
	EXPECT_EQ("Image", scene->getName());
	// The scene rect is taken from the first pyramid directory (the base level).
	const cv::Rect rect = scene->getRect();
	EXPECT_EQ(35840, rect.width);
	EXPECT_EQ(30720, rect.height);
	EXPECT_EQ(3, scene->getNumChannels());
}

// The scene is assembled from exactly the directories named by the pyramid list,
// in the given order: the first index becomes the base level that drives the
// scene rect. Here the pyramid references dir 2 first, then dir 0.
TEST_F(PhTiffImageDriverTests, phCreateImageScene_usesPyramidIndicesInOrder) {
	const std::vector<TiffDirectory> directories = {
		makeImageDir(MockSVSSlide::fakeXML, 45056, 35840),  // index 0
		makeImageDir("level=1 mag=22 quality=80", 22528, 17920), // index 1 (unused)
		makeImageDir("level=2 mag=11 quality=80", 11264, 9216),  // index 2
	};
	const std::vector<PHTLevel> imagePyramid = { {2, 0}, {0, 1} };  // base = dir 2

	MockSVSSlide slide;
	slide.phCreateImageSceneMock(directories, imagePyramid, nullptr);

	ASSERT_EQ(1, slide.getNumScenes());
	const cv::Rect rect = slide.getScene(0)->getRect();
	EXPECT_EQ(11264, rect.width);   // dir 2, first in the list
	EXPECT_EQ(9216, rect.height);
}

// Philips stores every zoom level padded up to a whole number of tiles, so the
// stored directory is larger than the image it holds: level 2 of Philips-4.tiff is
// a 23040x17408 directory carrying a 22784x17024 image. The padding is not image
// data. Each level must report the size of its content and the downsample the
// philips metadata assigns to it (2^-levelNumber), not the padded directory size --
// otherwise the scale of the level is off by the padding (1% at level 2, 44% at
// level 8) and every block read from it comes back shifted and stretched.
TEST_F(PhTiffImageDriverTests, phCreateImageScene_cropsTilePaddingOfZoomLevels) {
	const std::string xml = MockSVSSlide::createFakeXml(91136, 68096, 3, {});
	const std::vector<TiffDirectory> directories = {
		makeImageDir(xml, 91136, 68096),                          // level 0, not padded
		makeImageDir("level=1 mag=20 quality=80", 45568, 34304),   // holds 45568x34048
		makeImageDir("level=2 mag=10 quality=80", 23040, 17408),   // holds 22784x17024
	};
	const std::vector<PHTLevel> imagePyramid = { {0, 0}, {1, 1}, {2, 2} };

	MockSVSSlide slide;
	slide.phCreateImageSceneMock(directories, imagePyramid, nullptr);

	ASSERT_EQ(1, slide.getNumScenes());
	auto scene = slide.getScene(0);
	ASSERT_TRUE(scene != nullptr);
	// The base level is not padded, so the scene rect is unchanged.
	EXPECT_EQ(91136, scene->getRect().width);
	EXPECT_EQ(68096, scene->getRect().height);
	ASSERT_EQ(3, scene->getNumZoomLevels());

	const int widths[] = { 91136, 45568, 22784 };
	const int heights[] = { 68096, 34048, 17024 };
	for (int level = 0; level < 3; ++level) {
		const LevelInfo* info = scene->getZoomLevelInfo(level);
		ASSERT_TRUE(info != nullptr) << "level " << level;
		EXPECT_EQ(widths[level], info->getSize().width) << "level " << level;
		EXPECT_EQ(heights[level], info->getSize().height) << "level " << level;
		EXPECT_DOUBLE_EQ(1. / (1 << level), info->getScale()) << "level " << level;
	}
}

// The same padding on a real file: the levels of Philips-3.tiff are padded in
// height from level 3 down (level 8 is a 512x512 directory holding 512x392).
TEST_F(PhTiffImageDriverTests, zoomLevelsOfPhilips3ExcludeTilePadding) {
	const std::string filePath = TestTools::getFullTestImagePath("philips", "Philips-3.tiff");
	auto slide = slideio::openSlide(filePath, "PHTIFF");
	ASSERT_TRUE(slide != nullptr);
	auto scene = slide->getScene(0);
	ASSERT_TRUE(scene != nullptr);
	ASSERT_EQ(9, scene->getNumZoomLevels());

	const int widths[] = { 131072, 65536, 32768, 16384, 8192, 4096, 2048, 1024, 512 };
	const int heights[] = { 100352, 50176, 25088, 12544, 6272, 3136, 1568, 784, 392 };
	for (int level = 0; level < 9; ++level) {
		const LevelInfo* info = scene->getLevelInfo(level);
		ASSERT_TRUE(info != nullptr) << "level " << level;
		EXPECT_EQ(level, info->getLevel());
		EXPECT_EQ(widths[level], info->getSize().width) << "level " << level;
		EXPECT_EQ(heights[level], info->getSize().height) << "level " << level;
		EXPECT_DOUBLE_EQ(1. / (1 << level), info->getScale()) << "level " << level;
	}
}

// The mean absolute difference of two rasters of the same size and type.
static double phMeanAbsDiff(const cv::Mat& first, const cv::Mat& second) {
	cv::Mat diff;
	cv::absdiff(first, second, diff);
	return cv::mean(diff)[0];
}

// A read served from a padded zoom level has to map scene coordinates onto the
// content of the level and not onto its tile padding. The same region of
// Philips-3.tiff is read twice: once at zoom 1/4, which is served from level 2 (an
// unpadded level, hence a trustworthy reference), and once at zoom 1/64, which is
// served from level 6 (a 2048x2048 directory holding 2048x1568 pixels). Reduced to a
// common size the two rasters have to agree apart from jpeg noise and resampling.
//
// The region is deliberately far down the slide and rich in contrast: the padding
// error grows with the distance from the origin, and at y=70000 counting the padding
// as image data displaces the read by some 437 rows of level 6 -- a different part of
// the slide altogether. A whole slide read cannot serve as the probe here, because
// every level it could be served from is padded by the same proportion, so two such
// reads agree with each other while both being wrong.
TEST_F(PhTiffImageDriverTests, readFromPaddedZoomLevelMatchesUnpaddedLevel) {
	const std::string filePath = TestTools::getFullTestImagePath("philips", "Philips-3.tiff");
	SVSImageDriver driver("PHTIFF");
	auto slide = driver.openFile(filePath);
	ASSERT_TRUE(slide != nullptr);
	auto scene = slide->getScene(0);
	ASSERT_TRUE(scene != nullptr);
	const cv::Rect blockRect = { 20000, 70000, 8192, 4096 };
	const cv::Size probeSize = { 128, 64 };

	cv::Mat reference, probe;
	scene->readResampledBlock(blockRect, cv::Size(2048, 1024), reference);  // level 2
	scene->readResampledBlock(blockRect, probeSize, probe);                 // level 6
	ASSERT_EQ(probeSize, probe.size());
	// Guard the premise of the comparison: a flat region would match anything.
	cv::Scalar mean, stddev;
	cv::meanStdDev(reference, mean, stddev);
	ASSERT_GT(stddev[0], 20.) << "the reference region carries too little contrast to compare";

	cv::Mat referenceResized;
	cv::resize(reference, referenceResized, probeSize, 0., 0., cv::INTER_AREA);
	// Philips regenerates every level with quality 80 jpeg, so rasters of the same area
	// taken from different levels never match exactly: measured on this region, two
	// *unpadded* levels (0 and 2) already differ by 20. The threshold sits above that
	// inherent noise and far below the 66 that counting the padding as image data
	// produces -- it is a check of the coordinate mapping, not of codec fidelity.
	EXPECT_LT(phMeanAbsDiff(probe, referenceResized), 25.);
}

// phCreateAuxScenes turns each (description -> directory index) entry into a
// named auxiliary scene retrievable by name, and registers the names.
TEST_F(PhTiffImageDriverTests, phCreateAuxScenes_createsNamedAuxScenes) {
	const std::vector<TiffDirectory> directories = {
		makeImageDir(MockSVSSlide::fakeXML, 131072, 100352),
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
		makeImageDir(MockSVSSlide::fakeXML, 4096, 4096),
	};
	const std::map<std::string, int> auxImages;

	MockSVSSlide slide;
	slide.phCreateAuxScenesMock(directories, auxImages);

	EXPECT_EQ(0, slide.getNumAuxImages());
	EXPECT_TRUE(slide.getAuxImageNames().empty());
}

TEST_F(PhTiffImageDriverTests, openSlide2) {
	if (!TestTools::isFullTestEnabled())
	{
		GTEST_SKIP() << "Skip private test because full dataset is not enabled";
	}
	std::string filePath = TestTools::getFullTestImagePath("philips", "Philips-4.tiff");
	// The philips metadata of this file declares a label image and a macro image, but
	// the only auxiliary directory it stores is the macro (1816x821, described
	// "Macro -offset=(0,0)-pixelsize=(0.0315,0.0315)-rois=(...)"). There is no label.
	std::list<std::tuple<std::string, int, int>> auxNames = {
		{"Macro", 1816, 821}
	};
	std::string roiPaths[] = {
		TestTools::getFullTestImagePath("czi", "test/example_split (1).czi - ScanRegion0 (1, x=17583, y=3676, w=1000, h=1000).png"),
		TestTools::getFullTestImagePath("czi", "test/example_split (1).czi - ScanRegion0 (1, x=41169, y=4850, w=1000, h=1000).png"),
		TestTools::getFullTestImagePath("czi", "test/example_split (1).czi - ScanRegion0 (1, x=2668, y=1376, w=1000, h=1000).png"),
	};
    std::shared_ptr<Slide> slide = openSlide(filePath, "PHTIFF");
	ASSERT_FALSE(slide == nullptr);
	EXPECT_EQ(1, slide->getNumScenes());
	auto scene = slide->getScene(0);
	ASSERT_TRUE(scene != nullptr);
	EXPECT_EQ("Image", scene->getName());
	auto rect = scene->getRect();
	EXPECT_EQ(91136, std::get<2>(rect));
	EXPECT_EQ(68096, std::get<3>(rect));
	EXPECT_EQ(3, scene->getNumChannels());
	EXPECT_EQ(1, slide->getNumAuxImages());
	EXPECT_NEAR(std::get<0>(scene->getResolution()), 0.25e-6, 1e-9);
	EXPECT_NEAR(std::get<1>(scene->getResolution()), 0.25e-6, 1e-9);

	for (const auto& param : auxNames) {
		auto auxScene = slide->getAuxImage(std::get<0>(param));
		EXPECT_TRUE(auxScene != nullptr);
		auto rect = auxScene->getRect();
		EXPECT_EQ(std::get<2>(rect), std::get<1>(param));
		EXPECT_EQ(std::get<3>(rect), std::get<2>(param));
	}

}

// The auxiliary images of a slide are the ones the file stores, not the ones its
// metadata declares. The four philips test files cover three layouts: both a macro and
// a label directory (Philips-3), a macro alone (Philips-2 and Philips-4, the latter
// declaring a label image it does not store) and none at all (Philips-1, whose macro
// and label live in the metadata as embedded jpeg, which the driver does not read yet).
TEST_F(PhTiffImageDriverTests, auxImagesOfTheTestFiles) {
	if (!TestTools::isFullTestEnabled()) {
		GTEST_SKIP() << "Skip private test because full dataset is not enabled";
	}
	const std::list<std::pair<std::string, std::list<std::string>>> expected = {
		{"Philips-1.tiff", {}},
		{"Philips-2.tiff", {"Macro"}},
		{"Philips-3.tiff", {"Label", "Macro"}},
		{"Philips-4.tiff", {"Macro"}},
	};
	for (const auto& param : expected) {
		const std::string filePath = TestTools::getFullTestImagePath("philips", param.first);
		auto slide = slideio::openSlide(filePath, "PHTIFF");
		ASSERT_TRUE(slide != nullptr) << param.first;
		EXPECT_EQ(1, slide->getNumScenes()) << param.first;
		// getAuxImageNames is sorted, the expectations are spelled in the same order.
		EXPECT_EQ(param.second, slide->getAuxImageNames()) << param.first;
		for (const std::string& name : param.second) {
			auto auxScene = slide->getAuxImage(name);
			EXPECT_TRUE(auxScene != nullptr) << param.first << " " << name;
		}
	}
}

// Detection precedence for tiff files. The ome-tiff row is the regression guard: its
// name matches the *.tiff pattern of the philips driver too, and it must keep going to
// the ome-tiff driver, which precedes philips in the order.
TEST_F(PhTiffImageDriverTests, findDriver) {
	const std::list<std::pair<std::string, std::string>> expected = {
		{"PHTIFF", TestTools::getFullTestImagePath("philips", "Philips-3.tiff")},
		{"GDAL", TestTools::getTestImagePath("gdal", "img_2448x2448_3x16bit_SRC_RGB_ducks.tif")},
		{"GDAL", TestTools::getTestImagePath("gdal", "multipage.tif")},
		{"OMETIFF", TestTools::getFullTestImagePath("ometiff", "00001_01.ome.tiff")},
		{"SVS", TestTools::getTestImagePath("svs", "CMU-1-Small-Region.svs")},
	};
	for (const auto& param : expected) {
		auto driver = ImageDriverManager::findDriver(param.second);
		ASSERT_TRUE(driver != nullptr) << param.second;
		EXPECT_EQ(param.first, driver->getID()) << param.second;
	}
}

// The whole point of the detection: a philips file opens without naming a driver, and
// it opens as a philips slide. The public Slide class exposes no driver id, so the
// assertions are on what only the philips driver produces -- gdal would hand back a
// single flat scene with no auxiliary images and no resolution.
TEST_F(PhTiffImageDriverTests, openSlideWithoutDriverId) {
	const std::string filePath = TestTools::getFullTestImagePath("philips", "Philips-3.tiff");
	auto slide = slideio::openSlide(filePath);
	ASSERT_TRUE(slide != nullptr);
	EXPECT_EQ(1, slide->getNumScenes());
	EXPECT_EQ((std::list<std::string>{"Label", "Macro"}), slide->getAuxImageNames());
	auto scene = slide->getScene(0);
	ASSERT_TRUE(scene != nullptr);
	const auto res = scene->getResolution();
	EXPECT_DOUBLE_EQ(0.000226891e-3, std::get<0>(res));
	EXPECT_DOUBLE_EQ(0.000226907e-3, std::get<1>(res));
}

// ---------------------------------------------------------------------------
// PHTDescription
// ---------------------------------------------------------------------------

// A structurally faithful miniature of the Philips metadata (see Philips-3.tiff
// and Philips-4.tiff): a DPUfsImport root holding an array of DPScannedImage
// objects, the WSI one of which holds an array of PixelDataRepresentation zoom
// levels. It reproduces the quirks the parser has to survive:
//  - empty <Attribute/> elements carrying no Name/Group/Element,
//  - both xml attribute orderings (Name-first at the root, Element-first below),
//  - a lowercase hexadecimal element id ("0x115e"),
//  - quoted, blank separated values of the array types.
static const std::string phSampleXML = R"xml(<?xml version="1.0" encoding="UTF-8" ?>
<DataObject ObjectType="DPUfsImport">
    <Attribute Name="DICOM_MANUFACTURER" Group="0x0008" Element="0x0070" PMSVR="IString">PHILIPS</Attribute>
    <Attribute/>
    <Attribute Name="PIM_DP_UFS_INTERFACE_VERSION" Group="0x301D" Element="0x1001" PMSVR="IString">5.0</Attribute>
    <Attribute Name="PIM_DP_SCANNED_IMAGES" Group="0x301D" Element="0x1003" PMSVR="IDataObjectArray">
        <Array>
            <DataObject ObjectType="DPScannedImage">
                <Attribute/>
                <Attribute Element="0x1004" Group="0x301D" Name="PIM_DP_IMAGE_TYPE" PMSVR="IString">WSI</Attribute>
                <Attribute Element="0x0030" Group="0x0028" Name="DICOM_PIXEL_SPACING" PMSVR="IDoubleArray">"0.00025" "0.00026"</Attribute>
                <Attribute Element="0x1007" Group="0x301D" Name="PIM_DP_IMAGE_COLUMNS" PMSVR="IUInt32">91136</Attribute>
                <Attribute Element="0x1006" Group="0x301D" Name="PIM_DP_IMAGE_ROWS" PMSVR="IUInt32">68096</Attribute>
                <Attribute Element="0x8B01" Group="0x1001" Name="PIIM_PIXEL_DATA_REPRESENTATION_SEQUENCE" PMSVR="IDataObjectArray">
                    <Array>
                        <DataObject ObjectType="PixelDataRepresentation">
                            <Attribute Element="0x0030" Group="0x0028" Name="DICOM_PIXEL_SPACING" PMSVR="IDoubleArray">"0.00025" "0.00025"</Attribute>
                            <Attribute Element="0x100B" Group="0x101D" Name="PIIM_DP_PIXEL_DATA_REPRESENTATION_POSITION" PMSVR="IDoubleArray">"0" "1.5" "-2"</Attribute>
                            <Attribute Element="0x115e" Group="0x2001" Name="PIIM_PIXEL_DATA_REPRESENTATION_COLUMNS" PMSVR="IUInt32">91136</Attribute>
                            <Attribute Element="0x115D" Group="0x2001" Name="PIIM_PIXEL_DATA_REPRESENTATION_ROWS" PMSVR="IUInt32">68096</Attribute>
                            <Attribute Element="0x8B02" Group="0x1001" Name="PIIM_PIXEL_DATA_REPRESENTATION_NUMBER" PMSVR="IUInt16">0</Attribute>
                        </DataObject>
                        <DataObject ObjectType="PixelDataRepresentation">
                            <Attribute Element="0x0030" Group="0x0028" Name="DICOM_PIXEL_SPACING" PMSVR="IDoubleArray">"0.0005" "0.0005"</Attribute>
                            <Attribute Element="0x100B" Group="0x101D" Name="PIIM_DP_PIXEL_DATA_REPRESENTATION_POSITION" PMSVR="IDoubleArray">"0" "0" "0"</Attribute>
                            <Attribute Element="0x115E" Group="0x2001" Name="PIIM_PIXEL_DATA_REPRESENTATION_COLUMNS" PMSVR="IUInt32">45568</Attribute>
                            <Attribute Element="0x115D" Group="0x2001" Name="PIIM_PIXEL_DATA_REPRESENTATION_ROWS" PMSVR="IUInt32">34304</Attribute>
                            <Attribute Element="0x8B02" Group="0x1001" Name="PIIM_PIXEL_DATA_REPRESENTATION_NUMBER" PMSVR="IUInt16">1</Attribute>
                        </DataObject>
                    </Array>
                </Attribute>
            </DataObject>
            <DataObject ObjectType="DPScannedImage">
                <Attribute/>
                <Attribute Element="0x1004" Group="0x301D" Name="PIM_DP_IMAGE_TYPE" PMSVR="IString">LABELIMAGE</Attribute>
                <Attribute Element="0x1005" Group="0x301D" Name="PIM_DP_IMAGE_DATA" PMSVR="IString">QUJD</Attribute>
            </DataObject>
            <DataObject ObjectType="DPScannedImage">
                <Attribute Element="0x1004" Group="0x301D" Name="PIM_DP_IMAGE_TYPE" PMSVR="IString">MACROIMAGE</Attribute>
            </DataObject>
        </Array>
    </Attribute>
</DataObject>)xml";

class PHTDescriptionTests : public ::testing::Test {
protected:
	static void SetUpTestSuite() {
		ImageDriverManager::setLogLevel("FATAL");
	}
	// The single WSI image of phSampleXML.
	static const tinyxml2::XMLElement* wsiImage(PHTDescription& description) {
		for (const tinyxml2::XMLElement* image : description.getObjectList(description.getRoot(), SCANNED_IMAGE)) {
			if (description.getAttributeText(image, IMAGE_TYPE) == WSI) {
				return image;
			}
		}
		return nullptr;
	}
};

TEST_F(PHTDescriptionTests, constructorParsesValidXml) {
	PHTDescription description(phSampleXML);
	tinyxml2::XMLElement* root = description.getRoot();
	ASSERT_TRUE(root != nullptr);
	EXPECT_STREQ("DataObject", root->Name());
	EXPECT_STREQ("DPUfsImport", root->Attribute("ObjectType"));
}

TEST_F(PHTDescriptionTests, constructorThrowsOnMalformedXml) {
	EXPECT_THROW(PHTDescription description("this is not xml at all"), slideio::RuntimeError);
	EXPECT_THROW(PHTDescription description("<DataObject><Attribute></DataObject>"), slideio::RuntimeError);
}

TEST_F(PHTDescriptionTests, constructorThrowsOnEmptyDescription) {
	EXPECT_THROW(PHTDescription description(""), slideio::RuntimeError);
}

TEST_F(PHTDescriptionTests, getRootReturnsTheSameElement) {
	PHTDescription description(phSampleXML);
	EXPECT_EQ(description.getRoot(), description.getRoot());
}

TEST_F(PHTDescriptionTests, getObjectListReturnsScannedImagesInDocumentOrder) {
	PHTDescription description(phSampleXML);
	const std::vector<tinyxml2::XMLElement*> images =
		description.getObjectList(description.getRoot(), SCANNED_IMAGE);
	ASSERT_EQ(3u, images.size());
	EXPECT_EQ(WSI, description.getAttributeText(images[0], IMAGE_TYPE));
	EXPECT_EQ("LABELIMAGE", description.getAttributeText(images[1], IMAGE_TYPE));
	EXPECT_EQ("MACROIMAGE", description.getAttributeText(images[2], IMAGE_TYPE));
}

TEST_F(PHTDescriptionTests, getObjectListReturnsZoomLevels) {
	PHTDescription description(phSampleXML);
	const tinyxml2::XMLElement* image = wsiImage(description);
	ASSERT_TRUE(image != nullptr);
	const std::vector<tinyxml2::XMLElement*> levels =
		description.getObjectList(image, PIXEL_DATA_REPRESENTATION);
	ASSERT_EQ(2u, levels.size());
	EXPECT_EQ(0, description.getAttributeInt(levels[0], LEVEL_NUMBER));
	EXPECT_EQ(91136, description.getAttributeInt(levels[0], LEVEL_COLUMNS));
	EXPECT_EQ(68096, description.getAttributeInt(levels[0], LEVEL_ROWS));
	EXPECT_EQ(1, description.getAttributeInt(levels[1], LEVEL_NUMBER));
	EXPECT_EQ(45568, description.getAttributeInt(levels[1], LEVEL_COLUMNS));
	EXPECT_EQ(34304, description.getAttributeInt(levels[1], LEVEL_ROWS));
}

TEST_F(PHTDescriptionTests, getObjectListReturnsEmptyListForUnknownObjectType) {
	PHTDescription description(phSampleXML);
	EXPECT_TRUE(description.getObjectList(description.getRoot(), "NoSuchObject").empty());
}

// The search does not descend into nested objects: zoom levels belong to the
// scanned image, not to the root.
TEST_F(PHTDescriptionTests, getObjectListDoesNotSearchNestedObjects) {
	PHTDescription description(phSampleXML);
	EXPECT_TRUE(description.getObjectList(description.getRoot(), PIXEL_DATA_REPRESENTATION).empty());
	const tinyxml2::XMLElement* image = wsiImage(description);
	ASSERT_TRUE(image != nullptr);
	EXPECT_TRUE(description.getObjectList(image, SCANNED_IMAGE).empty());
}

TEST_F(PHTDescriptionTests, getObjectListThrowsOnNullParent) {
	PHTDescription description(phSampleXML);
	EXPECT_THROW(description.getObjectList(nullptr, SCANNED_IMAGE), slideio::RuntimeError);
}

TEST_F(PHTDescriptionTests, getAttributeTextReadsStringValues) {
	PHTDescription description(phSampleXML);
	EXPECT_EQ("PHILIPS", description.getAttributeText(description.getRoot(), MANUFACTURER));
	EXPECT_EQ("5.0", description.getAttributeText(description.getRoot(), UFS_INTERFACE_VERSION));
}

TEST_F(PHTDescriptionTests, getAttributeTextThrowsOnMissingAttribute) {
	PHTDescription description(phSampleXML);
	// The root object carries no barcode and no image type of its own.
	EXPECT_THROW(description.getAttributeText(description.getRoot(), UFS_BARCODE), slideio::RuntimeError);
	EXPECT_THROW(description.getAttributeText(description.getRoot(), IMAGE_TYPE), slideio::RuntimeError);
}

TEST_F(PHTDescriptionTests, getAttributeIntReadsIntegerValues) {
	PHTDescription description(phSampleXML);
	const tinyxml2::XMLElement* image = wsiImage(description);
	ASSERT_TRUE(image != nullptr);
	EXPECT_EQ(91136, description.getAttributeInt(image, IMAGE_COLUMNS));
	EXPECT_EQ(68096, description.getAttributeInt(image, IMAGE_ROWS));
}

TEST_F(PHTDescriptionTests, getAttributeIntThrowsOnNonNumericValue) {
	PHTDescription description(phSampleXML);
	const tinyxml2::XMLElement* image = wsiImage(description);
	ASSERT_TRUE(image != nullptr);
	// PIM_DP_IMAGE_TYPE holds "WSI".
	EXPECT_THROW(description.getAttributeInt(image, IMAGE_TYPE), slideio::RuntimeError);
	// DICOM_PIXEL_SPACING holds a list of doubles and must not be truncated to an int.
	EXPECT_THROW(description.getAttributeInt(image, IMAGE_RESOLUTION), slideio::RuntimeError);
}

TEST_F(PHTDescriptionTests, getAttributeIntThrowsOnMissingAttribute) {
	PHTDescription description(phSampleXML);
	EXPECT_THROW(description.getAttributeInt(description.getRoot(), IMAGE_COLUMNS), slideio::RuntimeError);
}

TEST_F(PHTDescriptionTests, getAttributeDoubleListReadsQuotedValues) {
	PHTDescription description(phSampleXML);
	const tinyxml2::XMLElement* image = wsiImage(description);
	ASSERT_TRUE(image != nullptr);
	const std::vector<double> spacing = description.getAttributeDoubleList(image, IMAGE_RESOLUTION);
	ASSERT_EQ(2u, spacing.size());
	EXPECT_DOUBLE_EQ(0.00025, spacing[0]);
	EXPECT_DOUBLE_EQ(0.00026, spacing[1]);
}

// The position of a zoom level is a triplet and may contain negative values.
TEST_F(PHTDescriptionTests, getAttributeDoubleListReadsTripletsAndNegativeValues) {
	PHTDescription description(phSampleXML);
	const tinyxml2::XMLElement* image = wsiImage(description);
	ASSERT_TRUE(image != nullptr);
	const std::vector<tinyxml2::XMLElement*> levels =
		description.getObjectList(image, PIXEL_DATA_REPRESENTATION);
	ASSERT_EQ(2u, levels.size());
	const std::vector<double> position = description.getAttributeDoubleList(levels[0], LEVEL_POSITION);
	ASSERT_EQ(3u, position.size());
	EXPECT_DOUBLE_EQ(0., position[0]);
	EXPECT_DOUBLE_EQ(1.5, position[1]);
	EXPECT_DOUBLE_EQ(-2., position[2]);
}

TEST_F(PHTDescriptionTests, getAttributeDoubleListThrowsOnMissingAttribute) {
	PHTDescription description(phSampleXML);
	EXPECT_THROW(description.getAttributeDoubleList(description.getRoot(), LEVEL_POSITION), slideio::RuntimeError);
}

TEST_F(PHTDescriptionTests, getAttributeDoubleListThrowsOnNonNumericValue) {
	PHTDescription description(phSampleXML);
	const tinyxml2::XMLElement* image = wsiImage(description);
	ASSERT_TRUE(image != nullptr);
	EXPECT_THROW(description.getAttributeDoubleList(image, IMAGE_TYPE), slideio::RuntimeError);
}

TEST_F(PHTDescriptionTests, hasAttributeDetectsPresenceAndAbsence) {
	PHTDescription description(phSampleXML);
	EXPECT_TRUE(description.hasAttribute(description.getRoot(), MANUFACTURER));
	EXPECT_FALSE(description.hasAttribute(description.getRoot(), UFS_BARCODE));

	const std::vector<tinyxml2::XMLElement*> images =
		description.getObjectList(description.getRoot(), SCANNED_IMAGE);
	ASSERT_EQ(3u, images.size());
	EXPECT_FALSE(description.hasAttribute(images[0], IMAGE_DATA));   // WSI: pixels live in the tiff
	EXPECT_TRUE(description.hasAttribute(images[1], IMAGE_DATA));    // label: embedded jpeg
	EXPECT_FALSE(description.hasAttribute(images[2], IMAGE_DATA));   // macro: absent here
}

TEST_F(PHTDescriptionTests, hasAttributeReturnsFalseForNullElement) {
	PHTDescription description(phSampleXML);
	EXPECT_FALSE(description.hasAttribute(nullptr, MANUFACTURER));
}

// The name alone does not identify an attribute: the group and the element ids
// have to match as well.
TEST_F(PHTDescriptionTests, hasAttributeMatchesGroupAndElement) {
	PHTDescription description(phSampleXML);
	const PHTDescription::Attribute wrongGroup = { MANUFACTURER.Name, "0x9999", MANUFACTURER.Element };
	const PHTDescription::Attribute wrongElement = { MANUFACTURER.Name, MANUFACTURER.Group, "0x9999" };
	const PHTDescription::Attribute wrongName = { "DICOM_NO_SUCH_TAG", MANUFACTURER.Group, MANUFACTURER.Element };
	EXPECT_FALSE(description.hasAttribute(description.getRoot(), wrongGroup));
	EXPECT_FALSE(description.hasAttribute(description.getRoot(), wrongElement));
	EXPECT_FALSE(description.hasAttribute(description.getRoot(), wrongName));
}

// Scanner software writes the hexadecimal ids in either case. The columns of the
// first zoom level of phSampleXML are tagged "0x115e", the constant says "0x115E".
TEST_F(PHTDescriptionTests, attributeLookupIgnoresHexIdCase) {
	PHTDescription description(phSampleXML);
	const tinyxml2::XMLElement* image = wsiImage(description);
	ASSERT_TRUE(image != nullptr);
	const std::vector<tinyxml2::XMLElement*> levels =
		description.getObjectList(image, PIXEL_DATA_REPRESENTATION);
	ASSERT_FALSE(levels.empty());
	EXPECT_TRUE(description.hasAttribute(levels[0], LEVEL_COLUMNS));
	EXPECT_EQ(91136, description.getAttributeInt(levels[0], LEVEL_COLUMNS));
}

// Empty <Attribute/> elements are present in the metadata of some scanners and
// must neither match a lookup nor crash it.
TEST_F(PHTDescriptionTests, emptyAttributeElementsAreIgnored) {
	PHTDescription description("<DataObject ObjectType=\"DPUfsImport\"><Attribute/><Attribute/></DataObject>");
	EXPECT_FALSE(description.hasAttribute(description.getRoot(), MANUFACTURER));
	EXPECT_THROW(description.getAttributeText(description.getRoot(), MANUFACTURER), slideio::RuntimeError);
	EXPECT_TRUE(description.getObjectList(description.getRoot(), SCANNED_IMAGE).empty());
}

TEST_F(PHTDescriptionTests, isNotCopyableButMovable) {
	static_assert(!std::is_copy_constructible<PHTDescription>::value,
		"PHTDescription owns the xml document and must not be copy constructible");
	static_assert(!std::is_copy_assignable<PHTDescription>::value,
		"PHTDescription owns the xml document and must not be copy assignable");
	static_assert(std::is_move_constructible<PHTDescription>::value, "PHTDescription must be move constructible");
	static_assert(std::is_move_assignable<PHTDescription>::value, "PHTDescription must be move assignable");
	static_assert(!std::is_convertible<const std::string&, PHTDescription>::value,
		"the constructor of PHTDescription must be explicit");
}

TEST_F(PHTDescriptionTests, moveKeepsTheDocumentUsable) {
	PHTDescription source(phSampleXML);
	PHTDescription moved(std::move(source));
	EXPECT_EQ("PHILIPS", moved.getAttributeText(moved.getRoot(), MANUFACTURER));
	EXPECT_EQ(3u, moved.getObjectList(moved.getRoot(), SCANNED_IMAGE).size());

	PHTDescription assigned("<DataObject ObjectType=\"DPUfsImport\"/>");
	assigned = std::move(moved);
	EXPECT_EQ("PHILIPS", assigned.getAttributeText(assigned.getRoot(), MANUFACTURER));
}

// A moved-from description holds no document: it must report an error instead of
// dereferencing a null pointer.
TEST_F(PHTDescriptionTests, movedFromDescriptionThrowsInsteadOfCrashing) {
	PHTDescription source(phSampleXML);
	PHTDescription moved(std::move(source));
	EXPECT_THROW(source.getRoot(), slideio::RuntimeError);
}

// The metadata generated by MockSVSSlide::createFakeXml has to be readable by
// the parser it is meant to feed.
TEST_F(PHTDescriptionTests, createFakeXmlDefaultsDescribeAWholeSlideImage) {
	PHTDescription description(MockSVSSlide::createFakeXml());
	EXPECT_EQ("PHILIPS", description.getAttributeText(description.getRoot(), MANUFACTURER));
	EXPECT_EQ("5.0", description.getAttributeText(description.getRoot(), UFS_INTERFACE_VERSION));

	const std::vector<tinyxml2::XMLElement*> images =
		description.getObjectList(description.getRoot(), SCANNED_IMAGE);
	ASSERT_EQ(1u, images.size());
	EXPECT_EQ(WSI, description.getAttributeText(images[0], IMAGE_TYPE));
	EXPECT_EQ(91136, description.getAttributeInt(images[0], IMAGE_COLUMNS));
	EXPECT_EQ(68096, description.getAttributeInt(images[0], IMAGE_ROWS));
	EXPECT_EQ(3, description.getAttributeInt(images[0], SAMPLES_PER_PIXEL));
	EXPECT_EQ(8, description.getAttributeInt(images[0], BITS_ALLOCATED));
	EXPECT_EQ(7, description.getAttributeInt(images[0], HIGH_BIT));
	EXPECT_EQ("RGB", description.getAttributeText(images[0], PHOTOMETRIC_INTERPRETATION));
	EXPECT_EQ(9u, description.getObjectList(images[0], PIXEL_DATA_REPRESENTATION).size());
}

// Every level halves the size of the previous one and doubles its pixel spacing.
TEST_F(PHTDescriptionTests, createFakeXmlBuildsTheRequestedPyramid) {
	PHTDescription description(MockSVSSlide::createFakeXml(1024, 512, 3));
	const std::vector<tinyxml2::XMLElement*> images =
		description.getObjectList(description.getRoot(), SCANNED_IMAGE);
	ASSERT_EQ(1u, images.size());
	const std::vector<tinyxml2::XMLElement*> levels =
		description.getObjectList(images[0], PIXEL_DATA_REPRESENTATION);
	ASSERT_EQ(3u, levels.size());

	const int widths[] = { 1024, 512, 256 };
	const int heights[] = { 512, 256, 128 };
	const double spacing[] = { 0.00025, 0.0005, 0.001 };
	for (int level = 0; level < 3; ++level) {
		EXPECT_EQ(level, description.getAttributeInt(levels[level], LEVEL_NUMBER));
		EXPECT_EQ(widths[level], description.getAttributeInt(levels[level], LEVEL_COLUMNS));
		EXPECT_EQ(heights[level], description.getAttributeInt(levels[level], LEVEL_ROWS));
		const std::vector<double> resolution = description.getAttributeDoubleList(levels[level], IMAGE_RESOLUTION);
		ASSERT_EQ(2u, resolution.size());
		EXPECT_DOUBLE_EQ(spacing[level], resolution[0]);
		EXPECT_DOUBLE_EQ(spacing[level], resolution[1]);
		EXPECT_EQ(3u, description.getAttributeDoubleList(levels[level], LEVEL_POSITION).size());
	}
}

// Auxiliary images follow the whole slide image and carry their raster inline.
TEST_F(PHTDescriptionTests, createFakeXmlAppendsAuxiliaryImages) {
	PHTDescription description(MockSVSSlide::createFakeXml(1024, 1024, 2, { "LABELIMAGE", "MACROIMAGE" }));
	const std::vector<tinyxml2::XMLElement*> images =
		description.getObjectList(description.getRoot(), SCANNED_IMAGE);
	ASSERT_EQ(3u, images.size());
	EXPECT_EQ(WSI, description.getAttributeText(images[0], IMAGE_TYPE));
	EXPECT_FALSE(description.hasAttribute(images[0], IMAGE_DATA));

	EXPECT_EQ("LABELIMAGE", description.getAttributeText(images[1], IMAGE_TYPE));
	EXPECT_TRUE(description.hasAttribute(images[1], IMAGE_DATA));
	EXPECT_FALSE(description.getAttributeText(images[1], IMAGE_DATA).empty());
	EXPECT_TRUE(description.getObjectList(images[1], PIXEL_DATA_REPRESENTATION).empty());

	EXPECT_EQ("MACROIMAGE", description.getAttributeText(images[2], IMAGE_TYPE));
	EXPECT_TRUE(description.hasAttribute(images[2], IMAGE_DATA));
}

// A slide without a pyramid is still valid metadata.
TEST_F(PHTDescriptionTests, createFakeXmlSupportsAnEmptyPyramid) {
	PHTDescription description(MockSVSSlide::createFakeXml(256, 256, 0));
	const std::vector<tinyxml2::XMLElement*> images =
		description.getObjectList(description.getRoot(), SCANNED_IMAGE);
	ASSERT_EQ(1u, images.size());
	EXPECT_TRUE(description.getObjectList(images[0], PIXEL_DATA_REPRESENTATION).empty());
	EXPECT_EQ(256, description.getAttributeInt(images[0], IMAGE_COLUMNS));
}

// Levels never collapse to a zero size, however deep the pyramid is.
TEST_F(PHTDescriptionTests, createFakeXmlClampsLevelSizeToOnePixel) {
	PHTDescription description(MockSVSSlide::createFakeXml(4, 2, 5));
	const std::vector<tinyxml2::XMLElement*> images =
		description.getObjectList(description.getRoot(), SCANNED_IMAGE);
	ASSERT_EQ(1u, images.size());
	const std::vector<tinyxml2::XMLElement*> levels =
		description.getObjectList(images[0], PIXEL_DATA_REPRESENTATION);
	ASSERT_EQ(5u, levels.size());
	EXPECT_EQ(1, description.getAttributeInt(levels[4], LEVEL_COLUMNS));
	EXPECT_EQ(1, description.getAttributeInt(levels[4], LEVEL_ROWS));
}

// The description of the first tiff directory is what identifies a philips file:
// the extension *.tif says nothing, gdal and ome-tiff use it too.
TEST_F(PHTDescriptionTests, isPhilipsDescriptionAcceptsPhilipsMetadata) {
	EXPECT_TRUE(PHTDescription::isPhilipsDescription(phSampleXML));
	EXPECT_TRUE(PHTDescription::isPhilipsDescription(MockSVSSlide::createFakeXml()));
	EXPECT_TRUE(PHTDescription::isPhilipsDescription(MockSVSSlide::fakeXML));
}

TEST_F(PHTDescriptionTests, isPhilipsDescriptionRejectsOtherDescriptions) {
	// The description of an ome-tiff file.
	EXPECT_FALSE(PHTDescription::isPhilipsDescription(
		"<?xml version=\"1.0\"?><OME xmlns=\"http://www.openmicroscopy.org/Schemas/OME/2016-06\">"
		"<Image ID=\"Image:0\"/></OME>"));
	// The description of an aperio svs file.
	EXPECT_FALSE(PHTDescription::isPhilipsDescription(
		"Aperio Image Library v11.2.1\r\n46920x33014 [0,100 46000x32914] (256x256)"
		" JPEG/RGB Q=30|AppMag = 20"));
	// The description of a zoom level of a philips file, which is not the metadata.
	EXPECT_FALSE(PHTDescription::isPhilipsDescription("level=1 mag=22 quality=80"));
	EXPECT_FALSE(PHTDescription::isPhilipsDescription(""));
	EXPECT_FALSE(PHTDescription::isPhilipsDescription("   \r\n\t"));
	// The marker alone, in text that is not xml.
	EXPECT_FALSE(PHTDescription::isPhilipsDescription("DPUfsImport"));
	// Xml, but not the philips import object.
	EXPECT_FALSE(PHTDescription::isPhilipsDescription(
		"<DataObject ObjectType=\"DPScannedImage\">DPUfsImport</DataObject>"));
	// The philips import object, but not as the root element.
	EXPECT_FALSE(PHTDescription::isPhilipsDescription(
		"<Wrapper><DataObject ObjectType=\"DPUfsImport\"/></Wrapper>"));
	// Malformed xml.
	EXPECT_FALSE(PHTDescription::isPhilipsDescription("<DataObject ObjectType=\"DPUfsImport\">"));
}

// A description may carry a utf-8 byte order mark.
TEST_F(PHTDescriptionTests, isPhilipsDescriptionAcceptsBomPrefixedMetadata) {
	EXPECT_TRUE(PHTDescription::isPhilipsDescription("\xEF\xBB\xBF" + MockSVSSlide::fakeXML));
}
