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
#include "slideio/drivers/svs/phtiffimagedriver.hpp"
#include "slideio/imagetools/smallimage.hpp"
#include "slideio/slideio/slideio.hpp"
#include "slideio/drivers/svs/svsimagedriver.hpp"
#include "slideio/drivers/svs/svsslide.hpp"
#include "slideio/drivers/svs/phtiffslide.hpp"
#include "slideio/drivers/svs/phtdescription.hpp"
#include "slideio/drivers/svs/phtmetadata.hpp"
#include "slideio/base/exceptions.hpp"
#include <type_traits>
#include <sstream>
#include <locale>
#include <filesystem>
#include <iomanip>
#include <cmath>


namespace slideio
{
    class Slide;
}

using namespace slideio;
using namespace slideio::phtiff;

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
// value is string_view, not const std::string&: WSI (now slideio::phtiff::WSI, a
// string_view constant) is passed straight through as an attribute value below, and a
// string_view has no implicit conversion to std::string.
static std::string phAttribute(const PHTDescription::Attribute& attribute, const std::string& type,
	std::string_view value, int indent) {
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

// Exposes SVSSlide::buildMetadataTree for unit testing the Aperio path on its own. The
// Aperio/philips split is by C++ type now rather than by a driver id string, so this
// mock derives from SVSSlide -- not PHTIFFSlide -- to keep proving the type that used
// to be tested by driver id: PHTIFFSlide::buildMetadataTree is an unconditional
// override, so a mock deriving from it would parse every description as philips xml
// regardless of the id given to metadataTreeOf.
class MockSVSSlide : public SVSSlide
{
public:
	// The metadata tree SVSSlide builds for a slide of the given driver carrying
	// `rawMetadata`. Both members are protected in CVSlide, so setting them here needs
	// no test hook in the production class.
	static Metadata metadataTreeOf(const std::string& driverId, const std::string& rawMetadata,
		MetadataFormat format) {
		MockSVSSlide slide;
		slide.setDriverId(driverId);
		slide.m_rawMetadata = rawMetadata;
		slide.m_metadataFormat = format;
		return slide.buildMetadataTree().freeze();
	}
};

// Exposes the protected PHTIFFSlide members (extractImages/createImageScene/
// createAuxScenes/init) for unit testing.
class MockPHTIFFSlide : public PHTIFFSlide
{
public:
	static void extractImagesMock(const std::vector<TiffDirectory>& directories, std::vector<PHTLevel>& imagePyramid,
		std::map<std::string, int>& auxImages) {
		// extractImages is a member now, not a static: it needs an instance to call
		// through. The metadata it used to parse for itself is now built once by init()
		// and passed in; rebuilding it here from the same directories keeps every
		// existing caller of this mock unchanged.
		const PHTMetadata metadata = directories.empty() ? PHTMetadata()
			: readPHTMetadata(directories.front().description);
		MockPHTIFFSlide slide;
		slide.extractImages(directories, metadata, imagePyramid, auxImages);
	}
	void createImageSceneMock(const std::vector<TiffDirectory>& directories, const std::vector<PHTLevel>& imagePyramid,
		libtiff::TIFF* hFile) {
		// These tests exercise scene geometry, not the philips metadata, so an empty
		// PHTMetadata is enough: processImageDescription tolerates a slide with no whole
		// slide image declared.
		createImageScene(directories, PHTMetadata(), imagePyramid, hFile);
	}
	void createAuxScenesMock(const std::vector<TiffDirectory>& directories,
		const std::map<std::string, int>& auxImages) {
		createAuxScenes(directories, auxImages);
	}
	void initMock(const std::vector<TiffDirectory>& directories, libtiff::TIFF* hFile) {
		// init takes ownership of the handle through the keeper, so the test gives
		// it one to take. A null handle is what these tests pass: the scenes are built
		// from the directories and nothing reads a raster.
		TIFFKeeper keeper(hFile);
		init(directories, keeper);
	}
	// The metadata tree of a philips slide whose tiff description is `description`. Sets
	// the raw metadata directly rather than routing through a driver id: dispatch is by
	// type now, so this proves the type selects the philips behaviour rather than a
	// string does.
	static Metadata phMetadataTree(const std::string& description) {
		MockPHTIFFSlide slide;
		slide.m_rawMetadata = description;
		slide.m_metadataFormat = MetadataFormat::XML;
		return slide.buildMetadataTree().freeze();
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
		int levels = phDefaults::LEVELS, const std::list<std::string>& auxNames = std::list<std::string>(),
		const std::string& barcode = std::string()) {
		std::ostringstream xml;
		xml << "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n";
		xml << "<DataObject ObjectType=\"DPUfsImport\">\n";
		xml << phAttribute(MANUFACTURER, "IString", phDefaults::MANUFACTURER_NAME, 1);
		xml << phAttribute(SOFTWARE_VERSIONS, "IStringArray", phDefaults::SOFTWARE_VERSIONS_VALUE, 1);
		xml << phAttribute(UFS_INTERFACE_VERSION, "IString", phDefaults::INTERFACE_VERSION, 1);
		// Not every philips file carries a barcode, so it is only emitted on request.
		if (!barcode.empty()) {
			xml << phAttribute(UFS_BARCODE, "IString", barcode, 1);
		}
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

const std::string MockPHTIFFSlide::fakeXML = "<?xml version=\"1.0\"?><DataObject ObjectType=\"DPUfsImport\"/>";

// The tiff directory indices of a pyramid, in level order.
static std::vector<int> phDirIndices(const std::vector<PHTLevel>& imagePyramid) {
	std::vector<int> indices;
	indices.reserve(imagePyramid.size());
	for (const PHTLevel& level : imagePyramid) {
		indices.push_back(level.dirIndex);
	}
	return indices;
}

// Builds a TiffDirectory carrying just the fields extractImages inspects: the
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


// The entries of a metadata array node, as strings.
static std::vector<std::string> phStringArray(const Metadata& node) {
	std::vector<std::string> values;
	for (size_t index = 0; index < node.size(); ++index) {
		values.push_back(node[index].asString());
	}
	return values;
}

// The entries of a metadata array node, as numbers.
static std::vector<double> phDoubleArray(const Metadata& node) {
	std::vector<double> values;
	for (size_t index = 0; index < node.size(); ++index) {
		values.push_back(node[index].asDouble());
	}
	return values;
}

// The image node of the given philips image type, e.g. "WSI". type is string_view, not
// const std::string&, so that slideio::phtiff::WSI (a string_view constant) can be
// passed straight through.
static Metadata phImageOfType(const Metadata& tree, std::string_view type) {
	const Metadata images = tree["images"];
	for (size_t index = 0; index < images.size(); ++index) {
		if (images[index]["type"].asString() == type) {
			return images[index];
		}
	}
	return Metadata();
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

// The two drivers claim different files. Before the split a single class served both
// and getFileSpecs treated any id that was not SVS as philips, so a third format added
// to it would silently have inherited the philips pattern.
TEST_F(PhTiffImageDriverTests, driversClaimTheirOwnFileSpecs) {
	EXPECT_EQ("*.svs", SVSImageDriver().getFileSpecs());
	EXPECT_EQ("*.tif;*.tiff", PHTIFFImageDriver().getFileSpecs());
	EXPECT_EQ("SVS", SVSImageDriver().getID());
	EXPECT_EQ("PHTIFF", PHTIFFImageDriver().getID());
}

// An svs file is identified by its extension alone and must not be content sniffed;
// a philips file is a *.tif that only its metadata identifies.
TEST_F(PhTiffImageDriverTests, onlyThePhilipsDriverSniffsContent) {
	if (!TestTools::isFullTestEnabled()) {
		GTEST_SKIP() << "Skip private test because full dataset is not enabled";
	}
	const std::string philips = TestTools::getFullTestImagePath("philips", "Philips-4.tiff");
	const std::string plainTiff = TestTools::getTestImagePath("svs", "CMU-1-Small-Region-page-1.tif");
	EXPECT_TRUE(PHTIFFImageDriver().canOpenFile(philips));
	EXPECT_FALSE(PHTIFFImageDriver().canOpenFile(plainTiff));
	EXPECT_FALSE(SVSImageDriver().canOpenFile(philips)) << "wrong extension for the svs driver";
}


// The extension contract needs no files at all, so it stays covered on a machine
// without the private dataset: extensions the driver does not serve are rejected
// without opening anything, and the svs id decides by extension alone (an svs file
// carries no philips metadata, so no content check ever runs for that id). This is
// also the only place where extension matching, and its case insensitivity, is
// observable without a file.
TEST_F(PhTiffImageDriverTests, canOpenFileByExtension) {
	PHTIFFImageDriver driver;
	const std::string disallowedSuffixes[] = {
		".ometif", ".ometiff", ".ometf2", ".ometf8", ".omebtf", ".svs", ".ndpi", ".qptiff"
	};
	for (const std::string& suffix : disallowedSuffixes) {
		EXPECT_FALSE(driver.canOpenFile("/projects/image" + suffix)) << suffix;
	}

	SVSImageDriver svsDriver;
	EXPECT_TRUE(svsDriver.canOpenFile("/projects/image.svs"));
	EXPECT_TRUE(svsDriver.canOpenFile("/projects/image.SVS"));
	EXPECT_FALSE(svsDriver.canOpenFile("/projects/image.tiff"));
}

// The extension is necessary but not sufficient: philips shares *.tif;*.tiff with
// gdal and with ome-tiff, so the driver has to look into the file.
TEST_F(PhTiffImageDriverTests, canOpenFileByContent) {
	if (!TestTools::isFullTestEnabled()) {
		GTEST_SKIP() << "Skip private test because full dataset is not enabled";
	}
	PHTIFFImageDriver driver;
	EXPECT_TRUE(driver.canOpenFile(TestTools::getFullTestImagePath("philips", "Philips-3.tiff")));
	EXPECT_FALSE(driver.canOpenFile(
		TestTools::getTestImagePath("gdal", "img_2448x2448_3x16bit_SRC_RGB_ducks.tif")));
	EXPECT_FALSE(driver.canOpenFile(TestTools::getTestImagePath("gdal", "multipage.tif")));
	EXPECT_FALSE(driver.canOpenFile(TestTools::getFullTestImagePath("ometiff", "00001_01.ome.tiff")));
	// A path that does not exist, and a file that is not a tiff at all.
	EXPECT_FALSE(driver.canOpenFile("/projects/no-such-file.tiff"));
	EXPECT_FALSE(driver.canOpenFile(TestTools::getTestImagePath("gdal", "colors.png")));

	SVSImageDriver svsDriver;
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
	const std::string xml = MockPHTIFFSlide::createFakeXml(131072, 100352, 3, { "MACROIMAGE", "LABELIMAGE" });
	const std::vector<TiffDirectory> directories = {
		makeLevelDir(xml, 131072, 100352),
		makeLevelDir("level=1 mag=22 quality=80", 65536, 50176),
		makeLevelDir("level=2 mag=11 quality=80", 32768, 25088),
		makeAuxDir("Macro", 791, 403),
		makeAuxDir("Label", 387, 403),
	};
	std::vector<PHTLevel> imagePyramid;
	std::map<std::string, int> auxImages;

	MockPHTIFFSlide::extractImagesMock(directories, imagePyramid, auxImages);

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
	const std::string xml = MockPHTIFFSlide::createFakeXml(91136, 68096, 2, { "LABELIMAGE", "MACROIMAGE" });
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

	MockPHTIFFSlide::extractImagesMock(directories, imagePyramid, auxImages);

	EXPECT_EQ((std::vector<int>{0, 1}), phDirIndices(imagePyramid));
	ASSERT_EQ(1u, auxImages.size());
	EXPECT_EQ(2, auxImages.at("Macro"));
	EXPECT_EQ(0u, auxImages.count("Label"));
}

// Auxiliary images are named after the image kinds slideio shares with the other
// drivers, whatever case the scanner wrote and whatever it appended to the kind.
TEST_F(PhTiffImageDriverTests, phExtractImages_usesCanonicalAuxImageNames) {
	const std::string xml = MockPHTIFFSlide::createFakeXml(4096, 4096, 1, {});
	const std::vector<TiffDirectory> directories = {
		makeLevelDir(xml, 4096, 4096),
		makeAuxDir("MACRO -offset=(0,0)", 791, 403),
		makeAuxDir("label", 387, 403),
		makeAuxDir("Thumbnail", 256, 256),
	};
	std::vector<PHTLevel> imagePyramid;
	std::map<std::string, int> auxImages;

	MockPHTIFFSlide::extractImagesMock(directories, imagePyramid, auxImages);

	ASSERT_EQ(3u, auxImages.size());
	EXPECT_EQ(1, auxImages.at("Macro"));
	EXPECT_EQ(2, auxImages.at("Label"));
	EXPECT_EQ(3, auxImages.at("Thumbnail"));
}

// An auxiliary image of an unknown kind keeps the leading word of its description
// rather than being dropped.
TEST_F(PhTiffImageDriverTests, phExtractImages_keepsUnknownAuxKinds) {
	const std::string xml = MockPHTIFFSlide::createFakeXml(4096, 4096, 1, {});
	const std::vector<TiffDirectory> directories = {
		makeLevelDir(xml, 4096, 4096),
		makeAuxDir("Overview -offset=(0,0)", 791, 403),
		makeAuxDir("", 100, 100),          // nothing to name it after -> dropped
	};
	std::vector<PHTLevel> imagePyramid;
	std::map<std::string, int> auxImages;

	MockPHTIFFSlide::extractImagesMock(directories, imagePyramid, auxImages);

	ASSERT_EQ(1u, auxImages.size());
	EXPECT_EQ(1, auxImages.at("Overview"));
}

// Two aux directories of the same kind must not clobber each other: the first
// occurrence wins and the duplicate is dropped.
TEST_F(PhTiffImageDriverTests, phExtractImages_duplicateAuxDescriptionKeepsFirst) {
	const std::string xml = MockPHTIFFSlide::createFakeXml(4096, 4096, 1, { "MACROIMAGE", "MACROIMAGE" });
	const std::vector<TiffDirectory> directories = {
		makeLevelDir(xml, 4096, 4096),
		makeAuxDir("Macro", 791, 403),     // index 1, kept
		makeAuxDir("Macro", 400, 200),     // index 2, duplicate -> dropped
	};
	std::vector<PHTLevel> imagePyramid;
	std::map<std::string, int> auxImages;

	MockPHTIFFSlide::extractImagesMock(directories, imagePyramid, auxImages);

	EXPECT_EQ((std::vector<int>{0}), phDirIndices(imagePyramid));
	ASSERT_EQ(1u, auxImages.size());
	EXPECT_EQ(1, auxImages.at("Macro"));
}

// A pyramid directory the philips metadata does not account for is left out of the
// pyramid: without a level number the area it covers is unknown.
TEST_F(PhTiffImageDriverTests, phExtractImages_ignoresUndeclaredPyramidDirectories) {
	const std::string xml = MockPHTIFFSlide::createFakeXml(4096, 4096, 2, {});
	const std::vector<TiffDirectory> directories = {
		makeLevelDir(xml, 4096, 4096),
		makeLevelDir("level=1 mag=20 quality=80", 2048, 2048),
		makeLevelDir("level=2 mag=10 quality=80", 1024, 1024),  // not declared in the xml
	};
	std::vector<PHTLevel> imagePyramid;
	std::map<std::string, int> auxImages;

	MockPHTIFFSlide::extractImagesMock(directories, imagePyramid, auxImages);

	EXPECT_EQ((std::vector<int>{0, 1}), phDirIndices(imagePyramid));
	EXPECT_TRUE(auxImages.empty());
}

// A tiled directory the metadata does not account for must not shift the level numbers
// of the directories that come after it. Levels are matched to directories by declared
// size, not by position, so the interloper (a tiled directory of a size no declared
// level has) is dropped on its own and the real levels 1 and 2 still find their own
// directories even though they no longer sit at the positions a positional zip expects.
TEST_F(PhTiffImageDriverTests, phExtractImages_undeclaredDirectoryDoesNotShiftLaterLevels) {
	const std::string xml = MockPHTIFFSlide::createFakeXml(4096, 4096, 3, {});
	const std::vector<TiffDirectory> directories = {
		makeLevelDir(xml, 4096, 4096),                            // level 0
		makeLevelDir("interloper", 3000, 3000),                   // not declared in the xml
		makeLevelDir("level=1 mag=20 quality=80", 2048, 2048),    // level 1
		makeLevelDir("level=2 mag=10 quality=80", 1024, 1024),    // level 2
	};
	std::vector<PHTLevel> imagePyramid;
	std::map<std::string, int> auxImages;

	MockPHTIFFSlide::extractImagesMock(directories, imagePyramid, auxImages);

	EXPECT_EQ((std::vector<int>{0, 2, 3}), phDirIndices(imagePyramid));
	EXPECT_EQ((std::vector<PHTLevel>{{0, 0}, {2, 1}, {3, 2}}), imagePyramid);
}

// Two declared levels can share a size -- a small slide whose base already sits on the
// tile grid comes out 1x1 at both of its lower levels. If an UNDECLARED tiled directory
// of that same size sits between their real directories, matching by size alone lets the
// interloper claim the second declared level, be marked corroborated, and get cropped as
// if it were real, while the real directory is dropped. The directories name their own
// level, so the pairing does not have to be guessed from size.
TEST_F(PhTiffImageDriverTests, phExtractImages_sameSizedInterloperDoesNotClaimADeclaredLevel) {
	const std::string xml = MockPHTIFFSlide::createFakeXml(2, 2, 3, {});
	const std::vector<TiffDirectory> directories = {
		makeLevelDir(xml, 2, 2),                                 // declared level 0
		makeLevelDir("level=1 mag=20 quality=80", 1, 1),         // declared level 1
		makeLevelDir("interloper", 1, 1),                        // undeclared, same size
		makeLevelDir("level=2 mag=10 quality=80", 1, 1),         // declared level 2
	};
	std::vector<PHTLevel> imagePyramid;
	std::map<std::string, int> auxImages;

	MockPHTIFFSlide::extractImagesMock(directories, imagePyramid, auxImages);

	EXPECT_EQ((std::vector<int>{0, 1, 3}), phDirIndices(imagePyramid))
		<< "the interloper at index 2 must not take level 2's place";
	EXPECT_EQ((std::vector<PHTLevel>{{0, 0}, {1, 1}, {3, 2}}), imagePyramid);
}

// createFakeXml always writes LEVEL_COLUMNS/LEVEL_ROWS for every level, so a level with
// no declared size has to be built by hand here rather than through createFakeXml. Hand
// building is chosen over post-processing createFakeXml's output because the levels are
// simple enough (two levels, no aux images) that composing them from the same
// phAttribute/phOpenArray/phCloseArray helpers createFakeXml itself uses is shorter and
// less fragile than stripping lines back out of a generated string.
static std::string createFakeXmlWithSizelessLevel(int width, int height) {
	std::ostringstream xml;
	xml << "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n";
	xml << "<DataObject ObjectType=\"DPUfsImport\">\n";
	xml << phAttribute(MANUFACTURER, "IString", phDefaults::MANUFACTURER_NAME, 1);
	xml << phOpenArray(SCANNED_IMAGES, 1);
	xml << phIndent(3) << "<DataObject ObjectType=\"" << SCANNED_IMAGE << "\">\n";
	xml << phAttribute(IMAGE_TYPE, "IString", WSI, 4);
	xml << phOpenArray(LEVEL_SEQUENCE, 4);
	// Level 0: declared with a size, matches its directory exactly.
	xml << phIndent(6) << "<DataObject ObjectType=\"" << PIXEL_DATA_REPRESENTATION << "\">\n";
	xml << phAttribute(LEVEL_NUMBER, "IUInt16", 0, 7);
	xml << phAttribute(LEVEL_COLUMNS, "IUInt32", width, 7);
	xml << phAttribute(LEVEL_ROWS, "IUInt32", height, 7);
	xml << phIndent(6) << "</DataObject>\n";
	// Level 1: no LEVEL_COLUMNS/LEVEL_ROWS at all -- some scanners omit the size.
	xml << phIndent(6) << "<DataObject ObjectType=\"" << PIXEL_DATA_REPRESENTATION << "\">\n";
	xml << phAttribute(LEVEL_NUMBER, "IUInt16", 1, 7);
	xml << phIndent(6) << "</DataObject>\n";
	xml << phCloseArray(4);
	xml << phAttribute(IMAGE_COLUMNS, "IUInt32", width, 4);
	xml << phAttribute(IMAGE_ROWS, "IUInt32", height, 4);
	xml << phIndent(3) << "</DataObject>\n";
	xml << phCloseArray(1);
	xml << "</DataObject>\n";
	return xml.str();
}

// The mismatch of the previous two tests, the other way round: the metadata declares a
// zoom level the file does not store. This is the level side of the Philips-4.tiff
// failure, where the metadata declares an auxiliary image the file does not store.
//
// The missing level is deliberately in the middle of the pyramid, because that is the
// case where the answer differs: the file stores levels 0 and 2, so pairing the two
// directories with the declared levels by position gives the second directory level 1's
// number instead of level 2's. The crop then divides by 2 rather than by 4 and the level
// reports twice the size and scale it covers -- the wrong-scale defect finding 1 was
// about. Matching by declared size gives the directory its own level number and drops
// the declaration the file does not back.
TEST_F(PhTiffImageDriverTests, phExtractImages_ignoresADeclaredLevelTheFileDoesNotStore) {
	const std::string xml = MockPHTIFFSlide::createFakeXml(4096, 4096, 3, {});
	const std::vector<TiffDirectory> directories = {
		makeLevelDir(xml, 4096, 4096),                          // declared level 0
		// The xml declares a level 1 of 2048x2048 that the file does not store. This is
		// the declared level 2.
		makeLevelDir("level=2 mag=10 quality=80", 1024, 1024),
	};
	std::vector<PHTLevel> imagePyramid;
	std::map<std::string, int> auxImages;

	MockPHTIFFSlide::extractImagesMock(directories, imagePyramid, auxImages);

	EXPECT_EQ((std::vector<int>{0, 1}), phDirIndices(imagePyramid));
	EXPECT_EQ((std::vector<PHTLevel>{{0, 0}, {1, 2}}), imagePyramid)
		<< "the stored directory must keep its own level number, not the next declared one";
	EXPECT_TRUE(auxImages.empty());
}

// A level the metadata declares no size for cannot be matched by size, so extraction
// falls back to pairing it with whatever tiled directory is left, by position, and marks
// the pairing unverified.
TEST_F(PhTiffImageDriverTests, phExtractImages_levelWithoutDeclaredSizeFallsBackToPosition) {
	const std::string xml = createFakeXmlWithSizelessLevel(4096, 4096);
	const std::vector<TiffDirectory> directories = {
		makeLevelDir(xml, 4096, 4096),
		makeLevelDir("level=1 mag=20 quality=80", 2048, 2048),
	};
	std::vector<PHTLevel> imagePyramid;
	std::map<std::string, int> auxImages;

	MockPHTIFFSlide::extractImagesMock(directories, imagePyramid, auxImages);

	ASSERT_EQ(2u, imagePyramid.size());
	EXPECT_EQ((std::vector<int>{0, 1}), phDirIndices(imagePyramid));
	EXPECT_TRUE(imagePyramid[0].corroborated) << "level 0 was matched by size";
	EXPECT_FALSE(imagePyramid[1].corroborated) << "level 1 has no declared size to match";
}

// Empty input yields empty outputs and does not touch the caller's containers.
TEST_F(PhTiffImageDriverTests, phExtractImages_emptyInput) {
	const std::vector<TiffDirectory> directories;
	std::vector<PHTLevel> imagePyramid;
	std::map<std::string, int> auxImages;

	EXPECT_THROW(MockPHTIFFSlide::extractImagesMock(directories, imagePyramid, auxImages), slideio::RuntimeError);
}

// createImageScene builds a single tiled "Image" scene out of the directories
// referenced by the pyramid index list and appends it to the slide's scenes.
// A null TIFF handle is fine: scene geometry comes from the directories, and the
// handle is only dereferenced later during raster reads.
TEST_F(PhTiffImageDriverTests, phCreateImageScene_createsSingleImageScene) {
	const std::string xml = MockPHTIFFSlide::createFakeXml(35840, 30720, 3, {});
	const std::vector<TiffDirectory> directories = {
		makeImageDir(xml, 35840, 30720),
		makeImageDir("level=1 mag=22 quality=80", 22528, 17920),
		makeImageDir("level=2 mag=11 quality=80", 11264, 9216),
	};
	const std::vector<PHTLevel> imagePyramid = { {0, 0}, {1, 1}, {2, 2} };

	MockPHTIFFSlide slide;
	slide.createImageSceneMock(directories, imagePyramid, nullptr);

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
		makeImageDir("level=1 mag=22 quality=80", 45056, 35840),  // index 0
		makeImageDir("level=2 mag=11 quality=80", 22528, 17920),  // index 1 (unused)
		// The base of the pyramid below, so it carries the slide metadata: philips keeps
		// its xml in the description of the base level's directory.
		makeImageDir(MockPHTIFFSlide::fakeXML, 11264, 9216),         // index 2
	};
	const std::vector<PHTLevel> imagePyramid = { {2, 0}, {0, 1} };  // base = dir 2

	MockPHTIFFSlide slide;
	slide.createImageSceneMock(directories, imagePyramid, nullptr);

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
	const std::string xml = MockPHTIFFSlide::createFakeXml(91136, 68096, 3, {});
	std::vector<TiffDirectory> directories = {
		makeImageDir(xml, 91136, 68096),                          // level 0, not padded
		makeImageDir("level=1 mag=20 quality=80", 45568, 34304),   // holds 45568x34048
		makeImageDir("level=2 mag=10 quality=80", 23040, 17408),   // holds 22784x17024
	};
	// These sizes are Philips-4.tiff's, and philips pads every level to a 512 pixel tile
	// grid. makeImageDir defaults to 256, under which the level 1 padding is a whole tile
	// rather than part of one -- a file that cannot exist, and one the crop would read
	// wrongly. The tile size has to match the grid the dimensions came from.
	for (TiffDirectory& dir : directories) {
		dir.tileWidth = 512;
		dir.tileHeight = 512;
	}
	// corroborated is spelled out here (PHTLevel now defaults it to false) because this
	// test is exactly about the crop that only a corroborated level receives.
	const std::vector<PHTLevel> imagePyramid = { {0, 0, true}, {1, 1, true}, {2, 2, true} };

	MockPHTIFFSlide slide;
	slide.createImageSceneMock(directories, imagePyramid, nullptr);

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

// phCropLevelPadding rewrites a level's dimensions but not its tile layout, so the crop
// is only safe while the content occupies the same number of tiles as the stored raster.
// Philips pads each level to its own tile grid, which makes that true on every real file
// -- but if it were ever false the tile indices would skew silently: wrong pixels, no
// error. Here level 1 is stored 1024 wide with 512-pixel tiles (2 tile columns) while its
// content is 512 (1 tile column), so cropping would change the tile count and must be
// refused, leaving the level at its stored size.
TEST_F(PhTiffImageDriverTests, phCreateImageScene_refusesACropThatWouldChangeTheTileCount) {
	const std::string xml = MockPHTIFFSlide::createFakeXml(1024, 1024, 2, {});
	std::vector<TiffDirectory> directories = {
		makeImageDir(xml, 1024, 1024),
		// Stored twice the size of its content, which no real philips file does -- the
		// point is the guard, not the layout.
		makeImageDir("level=1 mag=20 quality=80", 1024, 1024),
	};
	directories[1].tileWidth = 512;
	directories[1].tileHeight = 512;
	const std::vector<PHTLevel> imagePyramid = { {0, 0, true}, {1, 1, true} };

	MockPHTIFFSlide slide;
	slide.createImageSceneMock(directories, imagePyramid, nullptr);

	ASSERT_EQ(1, slide.getNumScenes());
	const LevelInfo* info = slide.getScene(0)->getZoomLevelInfo(1);
	ASSERT_TRUE(info != nullptr);
	EXPECT_EQ(1024, info->getSize().width)
		<< "content 512 is 1 tile but stored 1024 is 2, so the crop must be refused";
	EXPECT_EQ(1024, info->getSize().height);
}

// A level marked uncorroborated -- its directory was paired by position, not by a
// matching declared size -- must keep its stored size. Cropping it would apply
// ceil(base/2^level) on the strength of a level number extraction could not verify,
// which is exactly the wrong-scale defect the crop exists to remove.
TEST_F(PhTiffImageDriverTests, phCreateImageScene_doesNotCropAnUncorroboratedLevel) {
	const std::string xml = MockPHTIFFSlide::createFakeXml(4096, 4096, 2, {});
	const std::vector<TiffDirectory> directories = {
		makeImageDir(xml, 4096, 4096),                            // level 0, corroborated
		makeImageDir("level=1 mag=20 quality=80", 2100, 2100),    // stored, padded
	};
	const std::vector<PHTLevel> imagePyramid = { {0, 0, true}, {1, 1, false} };

	MockPHTIFFSlide slide;
	slide.createImageSceneMock(directories, imagePyramid, nullptr);

	ASSERT_EQ(1, slide.getNumScenes());
	auto scene = slide.getScene(0);
	ASSERT_TRUE(scene != nullptr);
	ASSERT_EQ(2, scene->getNumZoomLevels());
	// A corroborated level 0 is unaffected either way since it is not padded here.
	const LevelInfo* info = scene->getZoomLevelInfo(1);
	ASSERT_TRUE(info != nullptr);
	EXPECT_EQ(2100, info->getSize().width) << "the uncorroborated level must not be cropped";
	EXPECT_EQ(2100, info->getSize().height) << "the uncorroborated level must not be cropped";
}

// A base width that does not divide by the level's downsample is the only case where the
// rounding rule is observable, and no real philips file has one: every real base is a
// multiple of the 512 tile grid. Rounding UP is what keeps the level able to hold the
// whole slide -- 4099 pixels halve to 2050, and a level of 2049 would drop the last
// column. This test exists because that decision is otherwise never exercised.
TEST_F(PhTiffImageDriverTests, phCreateImageScene_roundsAContentSizeUpNotDown) {
	const std::string xml = MockPHTIFFSlide::createFakeXml(4099, 4099, 2, {});
	std::vector<TiffDirectory> directories = {
		makeImageDir(xml, 4099, 4099),
		// Padded to the tile grid by philips; the content is ceil(4099/2) = 2050.
		makeImageDir("level=1 mag=20 quality=80", 2560, 2560),
	};
	directories[1].tileWidth = 512;
	directories[1].tileHeight = 512;
	const std::vector<PHTLevel> imagePyramid = { {0, 0, true}, {1, 1, true} };

	MockPHTIFFSlide slide;
	slide.createImageSceneMock(directories, imagePyramid, nullptr);

	ASSERT_EQ(1, slide.getNumScenes());
	const LevelInfo* info = slide.getScene(0)->getZoomLevelInfo(1);
	ASSERT_TRUE(info != nullptr);
	EXPECT_EQ(2050, info->getSize().width) << "ceil(4099/2), not floor";
	EXPECT_EQ(2050, info->getSize().height);
}

// The same padding on a real file: the levels of Philips-3.tiff are padded in
// height from level 3 down (level 8 is a 512x512 directory holding 512x392).
TEST_F(PhTiffImageDriverTests, zoomLevelsOfPhilips3ExcludeTilePadding) {
	if (!TestTools::isFullTestEnabled()) {
		GTEST_SKIP() << "Skip private test because full dataset is not enabled";
	}
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
	if (!TestTools::isFullTestEnabled()) {
		GTEST_SKIP() << "Skip private test because full dataset is not enabled";
	}
	const std::string filePath = TestTools::getFullTestImagePath("philips", "Philips-3.tiff");
	PHTIFFImageDriver driver;
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

// createAuxScenes turns each (description -> directory index) entry into a
// named auxiliary scene retrievable by name, and registers the names.
TEST_F(PhTiffImageDriverTests, phCreateAuxScenes_createsNamedAuxScenes) {
	const std::vector<TiffDirectory> directories = {
		makeImageDir(MockPHTIFFSlide::fakeXML, 131072, 100352),
		makeImageDir("Macro", 791, 403),
		makeImageDir("Label", 387, 403),
	};
	const std::map<std::string, int> auxImages = { {"Macro", 1}, {"Label", 2} };

	MockPHTIFFSlide slide;
	slide.createAuxScenesMock(directories, auxImages);

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
		makeImageDir(MockPHTIFFSlide::fakeXML, 4096, 4096),
	};
	const std::map<std::string, int> auxImages;

	MockPHTIFFSlide slide;
	slide.createAuxScenesMock(directories, auxImages);

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

// What the four real files agree on. The exact values differ per file -- the level
// count, the software versions, whether a barcode was read -- so the assertions are
// on the structure the driver promises for any philips slide.
TEST_F(PhTiffImageDriverTests, metadataOfTheTestFiles) {
	if (!TestTools::isFullTestEnabled()) {
		GTEST_SKIP() << "Skip private test because full dataset is not enabled";
	}
	const std::string fileNames[] = {"Philips-1.tiff", "Philips-2.tiff", "Philips-3.tiff", "Philips-4.tiff"};
	for (const std::string& fileName : fileNames) {
		const std::string filePath = TestTools::getFullTestImagePath("philips", fileName);
		auto slide = slideio::openSlide(filePath, "PHTIFF");
		ASSERT_TRUE(slide != nullptr) << fileName;
		EXPECT_EQ(MetadataFormat::XML, slide->getMetadataFormat()) << fileName;

		const Metadata tree = slide->getMetadata();
		// DICOM_MANUFACTURER names the scanner the slide was acquired on, not the
		// vendor of the file format: of the four test files only Philips-4 says
		// "PHILIPS", while Philips-1 and Philips-3 say "Hamamatsu" and Philips-2 says
		// "3D Histech". Only its presence can be asserted across the set.
		EXPECT_FALSE(tree["manufacturer"].asString().empty()) << fileName;
		// The regression the quote stripping of getAttributeText caused: the versions
		// came back as one string with the interior quotes still in it.
		const std::vector<std::string> versions = phStringArray(tree["softwareVersions"]);
		EXPECT_FALSE(versions.empty()) << fileName;
		for (const std::string& version : versions) {
			EXPECT_EQ(std::string::npos, version.find('"')) << fileName << ": " << version;
		}

		const Metadata wsi = phImageOfType(tree, WSI);
		ASSERT_FALSE(wsi.isNull()) << fileName;
		auto scene = slide->getScene(0);
		ASSERT_TRUE(scene != nullptr) << fileName;
		const auto rect = scene->getRect();
		EXPECT_EQ(std::get<2>(rect), wsi["size"]["columns"].asInt()) << fileName;
		EXPECT_EQ(std::get<3>(rect), wsi["size"]["rows"].asInt()) << fileName;
		EXPECT_EQ(3, wsi["pixelFormat"]["samplesPerPixel"].asInt()) << fileName;

		// The declared pyramid: numbered from 0 upwards, one level per declaration.
		const Metadata levels = wsi["levels"];
		ASSERT_GT(levels.size(), 0u) << fileName;
		for (size_t index = 0; index < levels.size(); ++index) {
			EXPECT_EQ(static_cast<int64_t>(index), levels[index]["number"].asInt()) << fileName;
		}
		// The metadata is in millimeters, the scene resolution in meters.
		const std::vector<double> spacing = phDoubleArray(wsi["pixelSpacing"]);
		ASSERT_EQ(2u, spacing.size()) << fileName;
		EXPECT_NEAR(std::get<0>(scene->getResolution()), spacing[0] * 1.e-3, 1e-12) << fileName;

		// The scene describes its own tiff directory, not the slide.
		EXPECT_EQ(MetadataFormat::JSON, scene->getMetadataFormat()) << fileName;
		EXPECT_EQ(std::get<2>(rect), scene->getMetadata()["width"].asInt()) << fileName;
	}
}

// The magnification of the four real files, derived from the zoom level descriptions.
// Philips-2 reports 41x and Philips-1 and Philips-3 report 44x: the files say so, and
// the value is not rounded to a nominal one.
TEST_F(PhTiffImageDriverTests, magnificationOfTheTestFiles) {
	if (!TestTools::isFullTestEnabled()) {
		GTEST_SKIP() << "Skip private test because full dataset is not enabled";
	}
	const std::list<std::pair<std::string, double>> expected = {
		{"Philips-1.tiff", 44.},
		{"Philips-2.tiff", 41.},
		{"Philips-3.tiff", 44.},
		{"Philips-4.tiff", 40.},
	};
	for (const auto& param : expected) {
		const std::string filePath = TestTools::getFullTestImagePath("philips", param.first);
		auto slide = slideio::openSlide(filePath, "PHTIFF");
		ASSERT_TRUE(slide != nullptr) << param.first;
		auto scene = slide->getScene(0);
		ASSERT_TRUE(scene != nullptr) << param.first;
		EXPECT_DOUBLE_EQ(param.second, scene->getMagnification()) << param.first;

		// Every zoom level halves the magnification of the one above it, which is what
		// the level descriptions of the file say to six significant digits.
		const int levels = scene->getNumZoomLevels();
		ASSERT_GT(levels, 1) << param.first;
		for (int level = 0; level < levels; ++level) {
			const LevelInfo* info = scene->getLevelInfo(level);
			ASSERT_TRUE(info != nullptr) << param.first << " level " << level;
			EXPECT_NEAR(param.second / std::pow(2., level), info->getMagnification(), 1e-9)
				<< param.first << " level " << level;
		}
	}
}

// Philips-3.tiff is the one test file whose scanner read a barcode. Only its
// presence is asserted: the value identifies the slide.
TEST_F(PhTiffImageDriverTests, metadataCarriesTheBarcodeOfPhilips3) {
	if (!TestTools::isFullTestEnabled()) {
		GTEST_SKIP() << "Skip private test because full dataset is not enabled";
	}
	const std::string filePath = TestTools::getFullTestImagePath("philips", "Philips-3.tiff");
	auto slide = slideio::openSlide(filePath, "PHTIFF");
	ASSERT_TRUE(slide != nullptr);
	const Metadata tree = slide->getMetadata();
	ASSERT_TRUE(tree.contains("barcode"));
	EXPECT_FALSE(tree["barcode"].asString().empty());
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

// Detection precedence for tiff files. The philips row is the order guard: philips must
// precede gdal, which claims *.tif;*.tiff by extension alone with no content check, and
// would otherwise take the file first. The ome-tiff row asserts routing rather than
// order -- it holds regardless of where OMETIFF and PHTIFF sit relative to each other,
// because PHTIFF's content check rejects OME-XML metadata.
TEST_F(PhTiffImageDriverTests, findDriver) {
	if (!TestTools::isFullTestEnabled()) {
		GTEST_SKIP() << "Skip private test because full dataset is not enabled";
	}
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
// assertions are on what only the philips driver produces -- gdal would hand back one
// scene per tiff directory, with no auxiliary images and no resolution.
TEST_F(PhTiffImageDriverTests, openSlideWithoutDriverId) {
	if (!TestTools::isFullTestEnabled()) {
		GTEST_SKIP() << "Skip private test because full dataset is not enabled";
	}
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
// Metadata
// ---------------------------------------------------------------------------

// A two level philips file: the pyramid directories are tiled, and directory 0
// carries the metadata of the whole slide.
static std::vector<TiffDirectory> phFakePyramid(const std::string& xml, int width, int height) {
	std::vector<TiffDirectory> directories = {
		makeImageDir(xml, width, height),
		makeImageDir("level=1 mag=20 quality=80", width / 2, height / 2),
	};
	directories[0].tiled = true;
	directories[1].tiled = true;
	return directories;
}

// The xml of tiff directory 0 is the only place the philips metadata lives, so it
// is what the slide hands back as its raw metadata.
TEST_F(PhTiffImageDriverTests, initPhTiffKeepsTheDescriptionAsTheSlideMetadata) {
	const std::string xml = MockPHTIFFSlide::createFakeXml(1024, 768, 2);
	MockPHTIFFSlide slide;
	slide.setDriverId(PHTIFF_DRIVER_ID);
	slide.initMock(phFakePyramid(xml, 1024, 768), nullptr);

	EXPECT_EQ(xml, slide.getRawMetadata());
	EXPECT_EQ(MetadataFormat::XML, slide.getMetadataFormat());
}

// The whole point of the raw metadata: getMetadata() reaches the philips tree
// through it without the caller knowing which driver opened the file.
TEST_F(PhTiffImageDriverTests, initPhTiffMakesTheMetadataTreeAvailable) {
	const std::string xml = MockPHTIFFSlide::createFakeXml(1024, 768, 2);
	MockPHTIFFSlide slide;
	slide.setDriverId(PHTIFF_DRIVER_ID);
	slide.initMock(phFakePyramid(xml, 1024, 768), nullptr);

	const Metadata tree = slide.getMetadata();
	EXPECT_EQ("PHILIPS", tree["manufacturer"].asString());
	EXPECT_EQ(1024, phImageOfType(tree, WSI)["size"]["columns"].asInt());
}

// openFile creates the tiff handle and hands it to the scene that reads from it. If it
// is handed over before the philips parse -- which is the step most likely to fail,
// since it is the one that reads the file's own metadata -- then a failed open leaves
// nobody owning the handle. The leak is observable: libtiff opens the file without
// FILE_SHARE_DELETE, so a leaked handle keeps the file undeletable.
TEST_F(PhTiffImageDriverTests, aFailedOpenDoesNotLeaveTheFileOpen) {
	const std::string source = TestTools::getTestImagePath("svs", "CMU-1-Small-Region-page-1.tif");
	const std::filesystem::path copy =
		std::filesystem::temp_directory_path() / "phtiff-failed-open-leak-check.tif";
	std::error_code ignored;
	std::filesystem::remove(copy, ignored);
	ASSERT_NO_THROW(std::filesystem::copy_file(source, copy));

	// The description of this file is aperio text, not philips xml, so the philips parse
	// raises and the open never reaches a scene that could take the handle.
	EXPECT_THROW(PHTIFFSlide::openFile(copy.string()), slideio::RuntimeError);

	std::error_code error;
	const bool removed = std::filesystem::remove(copy, error);
	EXPECT_TRUE(removed) << "the file could not be deleted, so the tiff handle is still open: "
		<< error.message();
	std::filesystem::remove(copy, ignored);
}

// Removes the `occurrence`-th (0 based) <Attribute> element with the given name from
// the metadata, producing the description of a file whose scanner left that attribute
// out. Philips files do vary in which attributes they carry, and the objects a file
// does declare are not all complete.
static std::string phRemoveAttribute(const std::string& xml, std::string_view name, int occurrence) {
	const std::string needle = "Name=\"" + std::string(name) + "\"";
	size_t position = 0;
	for (int found = 0; found <= occurrence; ++found) {
		position = xml.find(needle, (found == 0) ? 0 : position + needle.size());
		if (position == std::string::npos) {
			return xml;
		}
	}
	const size_t lineBegin = xml.rfind('\n', position);
	const size_t lineEnd = xml.find('\n', position);
	if (lineBegin == std::string::npos || lineEnd == std::string::npos) {
		return xml;
	}
	return xml.substr(0, lineBegin) + xml.substr(lineEnd);
}

TEST_F(PhTiffImageDriverTests, readPHTMetadataReadsTheImagesAndTheirLevels) {
	const PHTMetadata metadata = readPHTMetadata(
		MockPHTIFFSlide::createFakeXml(1024, 768, 3, {"MACROIMAGE"}));
	ASSERT_EQ(2u, metadata.images.size());
	const PHTImageDeclaration* wsi = metadata.wholeSlideImage();
	ASSERT_TRUE(wsi != nullptr);
	EXPECT_EQ(1024, wsi->size.width);
	EXPECT_EQ(768, wsi->size.height);
	EXPECT_DOUBLE_EQ(phDefaults::PIXEL_SPACING, wsi->spacing.x);
	ASSERT_EQ(3u, wsi->levels.size());
	EXPECT_EQ(1, wsi->levels[1].number);
	EXPECT_EQ(512, wsi->levels[1].declaredSize.width);
	EXPECT_TRUE(metadata.images[1].levels.empty()) << "an auxiliary image has no pyramid";
}

// The skip-with-warning behaviour the robustness work added lives in the parse now.
TEST_F(PhTiffImageDriverTests, readPHTMetadataSkipsIncompleteDeclarations) {
	const std::string noType = phRemoveAttribute(
		MockPHTIFFSlide::createFakeXml(1024, 768, 2, {"MACROIMAGE"}), IMAGE_TYPE.Name, 1);
	EXPECT_EQ(1u, readPHTMetadata(noType).images.size());

	const std::string noNumber = phRemoveAttribute(
		MockPHTIFFSlide::createFakeXml(1024, 768, 2), LEVEL_NUMBER.Name, 1);
	// Bind the metadata to a local: wholeSlideImage() returns a pointer into it, so
	// calling it on the temporary would leave a dangling pointer.
	const PHTMetadata metadata = readPHTMetadata(noNumber);
	const PHTImageDeclaration* wsi = metadata.wholeSlideImage();
	ASSERT_TRUE(wsi != nullptr);
	EXPECT_EQ(1u, wsi->levels.size());
}

// Replaces the text of the `occurrence`-th (0 based) <Attribute> with the given name.
// Used to plant a value of the wrong type, which is what a scanner writing an empty or
// corrupt field produces in practice.
static std::string phSetAttributeValue(const std::string& xml, std::string_view name,
	int occurrence, const std::string& value) {
	const std::string needle = "Name=\"" + std::string(name) + "\"";
	size_t position = 0;
	for (int found = 0; found <= occurrence; ++found) {
		position = xml.find(needle, (found == 0) ? 0 : position + needle.size());
		if (position == std::string::npos) {
			return xml;
		}
	}
	const size_t open = xml.find('>', position);
	const size_t close = xml.find("</Attribute>", open);
	if (open == std::string::npos || close == std::string::npos) {
		return xml;
	}
	return xml.substr(0, open + 1) + value + xml.substr(close);
}

// A level number that is present but not a number cannot place the level, exactly as a
// missing one cannot. It costs the caller that level, not the slide.
TEST_F(PhTiffImageDriverTests, readPHTMetadataSkipsALevelWhoseNumberIsNotANumber) {
	const std::string xml = phSetAttributeValue(
		MockPHTIFFSlide::createFakeXml(1024, 768, 2), LEVEL_NUMBER.Name, 1, "not a number");
	const PHTMetadata metadata = readPHTMetadata(xml);
	const PHTImageDeclaration* wsi = metadata.wholeSlideImage();
	ASSERT_TRUE(wsi != nullptr);
	EXPECT_EQ(1u, wsi->levels.size()) << "the level with the unreadable number is dropped";
	EXPECT_EQ(0, wsi->levels[0].number);
}

// A size that will not parse costs the level its size, not its place in the pyramid: a
// level with no declared size is already handled, by positional fallback.
TEST_F(PhTiffImageDriverTests, readPHTMetadataKeepsALevelWhoseSizeIsNotANumber) {
	const std::string xml = phSetAttributeValue(
		MockPHTIFFSlide::createFakeXml(1024, 768, 2), LEVEL_COLUMNS.Name, 1, "");
	const PHTMetadata metadata = readPHTMetadata(xml);
	const PHTImageDeclaration* wsi = metadata.wholeSlideImage();
	ASSERT_TRUE(wsi != nullptr);
	ASSERT_EQ(2u, wsi->levels.size());
	EXPECT_EQ(0, wsi->levels[1].declaredSize.width) << "unreadable size left at its default";
	EXPECT_EQ(1, wsi->levels[1].number) << "the level itself survives";
}

// The same for the image's own dimensions.
TEST_F(PhTiffImageDriverTests, readPHTMetadataKeepsAnImageWhoseSizeIsNotANumber) {
	const std::string xml = phSetAttributeValue(
		MockPHTIFFSlide::createFakeXml(1024, 768, 2), IMAGE_COLUMNS.Name, 0, "wide");
	const PHTMetadata metadata = readPHTMetadata(xml);
	const PHTImageDeclaration* wsi = metadata.wholeSlideImage();
	ASSERT_TRUE(wsi != nullptr);
	EXPECT_EQ(0, wsi->size.width) << "unreadable size left at its default";
	EXPECT_EQ(WSI, wsi->type) << "the image itself survives";
}

TEST_F(PhTiffImageDriverTests, readPHTMetadataRaisesOnADescriptionItCannotParse) {
	EXPECT_THROW(readPHTMetadata("this is not xml at all"), slideio::RuntimeError);
}

// A scanned image the metadata declares without naming its type cannot be classified,
// but it says nothing about the other images in the file. Skipping it costs the caller
// nothing; raising costs the caller the slide.
TEST_F(PhTiffImageDriverTests, imageSceneSkipsAScannedImageWithoutAType) {
	const std::string xml = phRemoveAttribute(
		MockPHTIFFSlide::createFakeXml(1024, 768, 2, {"MACROIMAGE"}), IMAGE_TYPE.Name, 1);
	MockPHTIFFSlide slide;
	slide.setDriverId(PHTIFF_DRIVER_ID);
	slide.initMock(phFakePyramid(xml, 1024, 768), nullptr);

	auto scene = slide.getScene(0);
	ASSERT_TRUE(scene != nullptr);
	// The whole slide image is still described, so its resolution still arrives.
	EXPECT_DOUBLE_EQ(phDefaults::PIXEL_SPACING * 1.e-3, scene->getResolution().x);
}

// A zoom level the metadata declares without a level number cannot be placed in the
// pyramid: the level number is what says how much of the slide the level covers. The
// level is dropped and the rest of the pyramid is kept.
TEST_F(PhTiffImageDriverTests, phExtractImagesSkipsAZoomLevelWithoutANumber) {
	const std::string xml = phRemoveAttribute(
		MockPHTIFFSlide::createFakeXml(1024, 768, 2), LEVEL_NUMBER.Name, 1);
	MockPHTIFFSlide slide;
	slide.setDriverId(PHTIFF_DRIVER_ID);
	slide.initMock(phFakePyramid(xml, 1024, 768), nullptr);

	auto scene = slide.getScene(0);
	ASSERT_TRUE(scene != nullptr);
	EXPECT_EQ(1, scene->getNumZoomLevels()) << "the level without a number is dropped";
	EXPECT_EQ(1024, scene->getRect().width);
}

// Philips names the magnification of every zoom level but the base, whose directory
// carries the xml metadata instead. The scene takes the magnification of the slide
// from the first level that names one, scaled back up to the base.
TEST_F(PhTiffImageDriverTests, imageSceneMagnificationComesFromTheZoomLevels) {
	const std::string xml = MockPHTIFFSlide::createFakeXml(1024, 768, 2);
	MockPHTIFFSlide slide;
	slide.setDriverId(PHTIFF_DRIVER_ID);
	slide.initMock(phFakePyramid(xml, 1024, 768), nullptr);

	auto scene = slide.getScene(0);
	ASSERT_TRUE(scene != nullptr);
	EXPECT_DOUBLE_EQ(40., scene->getMagnification());
}

// Each zoom level reports the magnification it covers: a level is 2^-level of the
// base, and the philips level descriptions say the same thing.
TEST_F(PhTiffImageDriverTests, imageSceneZoomLevelsReportTheirOwnMagnification) {
	const std::string xml = MockPHTIFFSlide::createFakeXml(1024, 768, 3);
	std::vector<TiffDirectory> directories = {
		makeImageDir(xml, 1024, 768),
		makeImageDir("level=1 mag=20 quality=80", 512, 384),
		makeImageDir("level=2 mag=10 quality=80", 256, 192),
	};
	for (TiffDirectory& dir : directories) {
		dir.tiled = true;
	}
	MockPHTIFFSlide slide;
	slide.setDriverId(PHTIFF_DRIVER_ID);
	slide.initMock(directories, nullptr);

	auto scene = slide.getScene(0);
	ASSERT_TRUE(scene != nullptr);
	ASSERT_TRUE(scene->getZoomLevelInfo(0) != nullptr);
	EXPECT_DOUBLE_EQ(40., scene->getZoomLevelInfo(0)->getMagnification());
	EXPECT_DOUBLE_EQ(20., scene->getZoomLevelInfo(1)->getMagnification());
	EXPECT_DOUBLE_EQ(10., scene->getZoomLevelInfo(2)->getMagnification());
}

// A pyramid whose levels name no magnification leaves it at 0 rather than inventing
// one: an unknown magnification and a 0x magnification are the same thing to a caller.
TEST_F(PhTiffImageDriverTests, imageSceneMagnificationStaysZeroWhenNoLevelNamesOne) {
	const std::string xml = MockPHTIFFSlide::createFakeXml(1024, 768, 2);
	std::vector<TiffDirectory> directories = {
		makeImageDir(xml, 1024, 768),
		makeImageDir("quality=80", 512, 384),
	};
	for (TiffDirectory& dir : directories) {
		dir.tiled = true;
	}
	MockPHTIFFSlide slide;
	slide.setDriverId(PHTIFF_DRIVER_ID);
	slide.initMock(directories, nullptr);

	EXPECT_DOUBLE_EQ(0., slide.getScene(0)->getMagnification());
}

// The philips branch of the scene left the scene metadata empty while the aperio
// branch filled it from the tiff directory. The image scene of a philips slide
// describes its directory the same way, so that a caller reading scene metadata
// does not have to know which flavour of tiff it opened.
TEST_F(PhTiffImageDriverTests, imageSceneDescribesItsTiffDirectory) {
	const std::string xml = MockPHTIFFSlide::createFakeXml(1024, 768, 2);
	MockPHTIFFSlide slide;
	slide.setDriverId(PHTIFF_DRIVER_ID);
	slide.initMock(phFakePyramid(xml, 1024, 768), nullptr);

	auto scene = slide.getScene(0);
	ASSERT_TRUE(scene != nullptr);
	EXPECT_EQ(MetadataFormat::JSON, scene->getMetadataFormat());
	const Metadata tree = scene->getMetadata();
	EXPECT_EQ(1024, tree["width"].asInt());
	EXPECT_EQ(768, tree["height"].asInt());
}

// The scene metadata describes the directory the scene reads from, not the slide:
// the 844 KB philips description belongs to the slide and must not be duplicated
// into every scene.
TEST_F(PhTiffImageDriverTests, imageSceneMetadataIsNotTheSlideDescription) {
	const std::string xml = MockPHTIFFSlide::createFakeXml(1024, 768, 2);
	MockPHTIFFSlide slide;
	slide.setDriverId(PHTIFF_DRIVER_ID);
	slide.initMock(phFakePyramid(xml, 1024, 768), nullptr);

	const std::string sceneMetadata = slide.getScene(0)->getRawMetadata();
	EXPECT_FALSE(sceneMetadata.empty());
	EXPECT_NE(xml, sceneMetadata);
}

TEST_F(PhTiffImageDriverTests, metadataTreeReadsTheSlideAttributes) {
	const Metadata tree = MockPHTIFFSlide::phMetadataTree(MockPHTIFFSlide::createFakeXml());
	EXPECT_EQ("PHILIPS", tree["manufacturer"].asString());
	EXPECT_EQ("5.0", tree["interfaceVersion"].asString());
	EXPECT_EQ((std::vector<std::string>{"1.6.6186", "20150402_R48", "4.0.3"}),
		phStringArray(tree["softwareVersions"]));
}

// A philips file only carries a barcode if the scanner read one, so the key has to
// be absent rather than empty when the attribute is missing.
TEST_F(PhTiffImageDriverTests, metadataTreeOmitsAttributesTheFileDoesNotCarry) {
	const Metadata tree = MockPHTIFFSlide::phMetadataTree(MockPHTIFFSlide::createFakeXml());
	EXPECT_FALSE(tree.contains("barcode"));
}

TEST_F(PhTiffImageDriverTests, metadataTreeReadsTheBarcode) {
	const std::string xml = MockPHTIFFSlide::createFakeXml(phDefaults::WIDTH, phDefaults::HEIGHT,
		phDefaults::LEVELS, {}, "MDAwMTIzNA==");
	const Metadata tree = MockPHTIFFSlide::phMetadataTree(xml);
	EXPECT_EQ("MDAwMTIzNA==", tree["barcode"].asString());
}

TEST_F(PhTiffImageDriverTests, metadataTreeDescribesEveryScannedImage) {
	const std::string xml = MockPHTIFFSlide::createFakeXml(phDefaults::WIDTH, phDefaults::HEIGHT,
		phDefaults::LEVELS, {"LABELIMAGE", "MACROIMAGE"});
	const Metadata tree = MockPHTIFFSlide::phMetadataTree(xml);
	const Metadata images = tree["images"];
	ASSERT_EQ(3u, images.size());
	EXPECT_EQ((std::vector<std::string>{"WSI", "LABELIMAGE", "MACROIMAGE"}),
		(std::vector<std::string>{images[size_t(0)]["type"].asString(),
			images[size_t(1)]["type"].asString(), images[size_t(2)]["type"].asString()}));
}

TEST_F(PhTiffImageDriverTests, metadataTreeReadsTheSizeAndResolutionOfTheWholeSlideImage) {
	const Metadata wsi = phImageOfType(MockPHTIFFSlide::phMetadataTree(MockPHTIFFSlide::createFakeXml()), WSI);
	ASSERT_FALSE(wsi.isNull());
	EXPECT_EQ(phDefaults::WIDTH, wsi["size"]["columns"].asInt());
	EXPECT_EQ(phDefaults::HEIGHT, wsi["size"]["rows"].asInt());
	EXPECT_EQ("%FILENAME%", wsi["sourceFile"].asString());
	const std::vector<double> spacing = phDoubleArray(wsi["pixelSpacing"]);
	ASSERT_EQ(2u, spacing.size());
	EXPECT_DOUBLE_EQ(phDefaults::PIXEL_SPACING, spacing[0]);
	EXPECT_DOUBLE_EQ(phDefaults::PIXEL_SPACING, spacing[1]);
}

TEST_F(PhTiffImageDriverTests, metadataTreeReadsThePixelFormat) {
	const Metadata wsi = phImageOfType(MockPHTIFFSlide::phMetadataTree(MockPHTIFFSlide::createFakeXml()), WSI);
	ASSERT_FALSE(wsi.isNull());
	const Metadata format = wsi["pixelFormat"];
	EXPECT_EQ(phDefaults::SAMPLES, format["samplesPerPixel"].asInt());
	EXPECT_EQ(phDefaults::PHOTOMETRIC, format["photometricInterpretation"].asString());
	EXPECT_EQ(0, format["planarConfiguration"].asInt());
	EXPECT_EQ(phDefaults::BITS, format["bitsAllocated"].asInt());
	EXPECT_EQ(phDefaults::BITS, format["bitsStored"].asInt());
	EXPECT_EQ(phDefaults::BITS - 1, format["highBit"].asInt());
	EXPECT_EQ(0, format["pixelRepresentation"].asInt());
}

TEST_F(PhTiffImageDriverTests, metadataTreeReadsTheCompression) {
	const Metadata wsi = phImageOfType(MockPHTIFFSlide::phMetadataTree(MockPHTIFFSlide::createFakeXml()), WSI);
	ASSERT_FALSE(wsi.isNull());
	const Metadata compression = wsi["compression"];
	EXPECT_EQ("01", compression["lossy"].asString());
	EXPECT_EQ((std::vector<std::string>{"PHILIPS_TIFF_1_0"}), phStringArray(compression["method"]));
	EXPECT_EQ((std::vector<double>{3.}), phDoubleArray(compression["ratio"]));
}

TEST_F(PhTiffImageDriverTests, metadataTreeReadsTheZoomLevels) {
	const Metadata wsi = phImageOfType(MockPHTIFFSlide::phMetadataTree(MockPHTIFFSlide::createFakeXml()), WSI);
	ASSERT_FALSE(wsi.isNull());
	const Metadata levels = wsi["levels"];
	ASSERT_EQ(static_cast<size_t>(phDefaults::LEVELS), levels.size());
	const Metadata second = levels[size_t(1)];
	EXPECT_EQ(1, second["number"].asInt());
	EXPECT_EQ(phDefaults::WIDTH / 2, second["columns"].asInt());
	EXPECT_EQ(phDefaults::HEIGHT / 2, second["rows"].asInt());
	EXPECT_EQ((std::vector<double>{0., 0., 0.}), phDoubleArray(second["position"]));
	const std::vector<double> spacing = phDoubleArray(second["pixelSpacing"]);
	ASSERT_EQ(2u, spacing.size());
	EXPECT_DOUBLE_EQ(2. * phDefaults::PIXEL_SPACING, spacing[0]);
}

// The auxiliary images of Philips-1.tiff and Philips-3.tiff exist only as base64
// jpeg inside the metadata. That raster belongs in an image scene, not in a
// metadata tree a caller is expected to print.
TEST_F(PhTiffImageDriverTests, metadataTreeOmitsTheEmbeddedRaster) {
	const std::string xml = MockPHTIFFSlide::createFakeXml(phDefaults::WIDTH, phDefaults::HEIGHT,
		phDefaults::LEVELS, {"MACROIMAGE"});
	const Metadata tree = MockPHTIFFSlide::phMetadataTree(xml);
	const Metadata macro = phImageOfType(tree, "MACROIMAGE");
	ASSERT_FALSE(macro.isNull());
	EXPECT_FALSE(macro.contains("imageData"));
	EXPECT_EQ(std::string::npos, tree.toJson().find(phDefaults::IMAGE_DATA_VALUE));
}

// An image the metadata declares without a pyramid gets no empty levels array.
TEST_F(PhTiffImageDriverTests, metadataTreeGivesNoLevelsToAnAuxiliaryImage) {
	const std::string xml = MockPHTIFFSlide::createFakeXml(phDefaults::WIDTH, phDefaults::HEIGHT,
		phDefaults::LEVELS, {"MACROIMAGE"});
	const Metadata macro = phImageOfType(MockPHTIFFSlide::phMetadataTree(xml), "MACROIMAGE");
	ASSERT_FALSE(macro.isNull());
	EXPECT_FALSE(macro.contains("levels"));
}

// A description that is not philips metadata must not take getMetadata() down: the
// slide falls back to the generic xml handling, which reports the parse error.
TEST_F(PhTiffImageDriverTests, metadataTreeFallsBackWhenTheDescriptionCannotBeParsed) {
	const Metadata tree = MockPHTIFFSlide::phMetadataTree("this is not xml at all");
	EXPECT_TRUE(tree.contains("#error"));
}

// The philips branch must not disturb the aperio one. The "SVS" argument is inert:
// SVSSlide::buildMetadataTree is unconditional now, so no id branch selects the Aperio
// parse any more. What this test still proves is that the Aperio parse reaches an
// SVSSlide-derived slide at all -- not that some id branch returns it.
TEST_F(PhTiffImageDriverTests, metadataTreeOfAnSvsSlideStillParsesAperioMetadata) {
	const Metadata tree = MockSVSSlide::metadataTreeOf("SVS",
		"Aperio Image Library v11.0\r\n46000x32914 [0,100 46000x32914] (240x240) JPEG/RGB Q=30"
		"|AppMag = 20|MPP = 0.4990", MetadataFormat::Text);
	EXPECT_EQ("Aperio Image Library v11.0", tree["application"].asString());
	EXPECT_EQ("20", tree["properties"]["AppMag"].asString());
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
		for (const tinyxml2::XMLElement* image : description.getObjectList(description.getRoot(), SCANNED_IMAGES, SCANNED_IMAGE)) {
			if (description.getAttributeText(image, IMAGE_TYPE) == WSI) {
				return image;
			}
		}
		return nullptr;
	}
};

TEST_F(PHTDescriptionTests, constructorParsesValidXml) {
	PHTDescription description(phSampleXML);
	const tinyxml2::XMLElement* root = description.getRoot();
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
	const std::vector<const tinyxml2::XMLElement*> images =
		description.getObjectList(description.getRoot(), SCANNED_IMAGES, SCANNED_IMAGE);
	ASSERT_EQ(3u, images.size());
	EXPECT_EQ(WSI, description.getAttributeText(images[0], IMAGE_TYPE));
	EXPECT_EQ("LABELIMAGE", description.getAttributeText(images[1], IMAGE_TYPE));
	EXPECT_EQ("MACROIMAGE", description.getAttributeText(images[2], IMAGE_TYPE));
}

TEST_F(PHTDescriptionTests, getObjectListReturnsZoomLevels) {
	PHTDescription description(phSampleXML);
	const tinyxml2::XMLElement* image = wsiImage(description);
	ASSERT_TRUE(image != nullptr);
	const std::vector<const tinyxml2::XMLElement*> levels =
		description.getObjectList(image, LEVEL_SEQUENCE, PIXEL_DATA_REPRESENTATION);
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
	EXPECT_TRUE(description.getObjectList(description.getRoot(), SCANNED_IMAGES, "NoSuchObject").empty());
}

// The search does not descend into nested objects: zoom levels belong to the
// scanned image, not to the root.
TEST_F(PHTDescriptionTests, getObjectListDoesNotSearchNestedObjects) {
	PHTDescription description(phSampleXML);
	EXPECT_TRUE(description.getObjectList(description.getRoot(), LEVEL_SEQUENCE, PIXEL_DATA_REPRESENTATION).empty());
	const tinyxml2::XMLElement* image = wsiImage(description);
	ASSERT_TRUE(image != nullptr);
	EXPECT_TRUE(description.getObjectList(image, SCANNED_IMAGES, SCANNED_IMAGE).empty());
}

TEST_F(PHTDescriptionTests, getObjectListThrowsOnNullParent) {
	PHTDescription description(phSampleXML);
	EXPECT_THROW(description.getObjectList(nullptr, SCANNED_IMAGES, SCANNED_IMAGE), slideio::RuntimeError);
}

// Two arrays of the same object type under different attributes. The scanned images of
// a philips file are the ones its PIM_DP_SCANNED_IMAGES attribute declares; objects of
// the same type held by another attribute belong to that attribute, not to this one.
static const std::string phTwoArraysXML = R"xml(<?xml version="1.0" encoding="UTF-8" ?>
<DataObject ObjectType="DPUfsImport">
    <Attribute Name="PIM_DP_SCANNED_IMAGES" Group="0x301D" Element="0x1003" PMSVR="IDataObjectArray">
        <Array>
            <DataObject ObjectType="DPScannedImage">
                <Attribute Name="PIM_DP_IMAGE_TYPE" Group="0x301D" Element="0x1004" PMSVR="IString">WSI</Attribute>
            </DataObject>
        </Array>
    </Attribute>
    <Attribute Name="PIM_DP_OTHER_IMAGES" Group="0x301D" Element="0x9999" PMSVR="IDataObjectArray">
        <Array>
            <DataObject ObjectType="DPScannedImage">
                <Attribute Name="PIM_DP_IMAGE_TYPE" Group="0x301D" Element="0x1004" PMSVR="IString">DECOY</Attribute>
            </DataObject>
        </Array>
    </Attribute>
</DataObject>)xml";

// The same file with the declared attribute missing: the objects are only reachable by
// scanning every attribute for an array.
static const std::string phUndeclaredArrayXML = R"xml(<?xml version="1.0" encoding="UTF-8" ?>
<DataObject ObjectType="DPUfsImport">
    <Attribute Name="PIM_DP_OTHER_IMAGES" Group="0x301D" Element="0x9999" PMSVR="IDataObjectArray">
        <Array>
            <DataObject ObjectType="DPScannedImage">
                <Attribute Name="PIM_DP_IMAGE_TYPE" Group="0x301D" Element="0x1004" PMSVR="IString">DECOY</Attribute>
            </DataObject>
        </Array>
    </Attribute>
</DataObject>)xml";

TEST_F(PHTDescriptionTests, getObjectListReadsTheArrayOfTheDeclaredAttribute) {
	PHTDescription description(phTwoArraysXML);
	const std::vector<const tinyxml2::XMLElement*> images =
		description.getObjectList(description.getRoot(), SCANNED_IMAGES, SCANNED_IMAGE);
	ASSERT_EQ(1u, images.size());
	EXPECT_EQ(WSI, description.getAttributeText(images[0], IMAGE_TYPE));
}

// A file that holds its objects under an attribute this code does not know is still
// read rather than reported empty: the precise lookup is an improvement on the scan,
// not a new way to lose data.
TEST_F(PHTDescriptionTests, getObjectListFallsBackToScanningWhenTheAttributeIsAbsent) {
	PHTDescription description(phUndeclaredArrayXML);
	const std::vector<const tinyxml2::XMLElement*> images =
		description.getObjectList(description.getRoot(), SCANNED_IMAGES, SCANNED_IMAGE);
	ASSERT_EQ(1u, images.size());
	EXPECT_EQ("DECOY", description.getAttributeText(images[0], IMAGE_TYPE));
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

// The pixel spacing is read from a file and must not depend on the locale the
// embedding application happens to have set. Under a comma decimal locale a stream
// honouring the global locale stops "0.00025" at the point, leaving the rest of the
// value unread -- which the eof check then reports as a malformed value, so the slide
// does not open at all.
TEST_F(PHTDescriptionTests, getAttributeDoubleListIsIndependentOfTheHostLocale) {
	const std::locale original = std::locale();
	bool imbued = false;
	for (const char* name : {"de-DE", "de_DE.UTF-8", "German_Germany.1252"}) {
		try {
			std::locale::global(std::locale(name));
			imbued = true;
			break;
		}
		catch (const std::runtime_error&) {
		}
	}
	if (!imbued) {
		GTEST_SKIP() << "No comma decimal locale is installed on this machine";
	}
	std::vector<double> spacing;
	try {
		PHTDescription description(phSampleXML);
		const tinyxml2::XMLElement* image = wsiImage(description);
		ASSERT_TRUE(image != nullptr);
		spacing = description.getAttributeDoubleList(image, IMAGE_RESOLUTION);
	}
	catch (...) {
		std::locale::global(original);
		throw;
	}
	std::locale::global(original);
	ASSERT_EQ(2u, spacing.size());
	EXPECT_DOUBLE_EQ(0.00025, spacing[0]);
	EXPECT_DOUBLE_EQ(0.00026, spacing[1]);
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
	const std::vector<const tinyxml2::XMLElement*> levels =
		description.getObjectList(image, LEVEL_SEQUENCE, PIXEL_DATA_REPRESENTATION);
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

// The metadata of a slide whose string array attributes carry the values a real
// philips file stores in them. getAttributeText cannot read these: it strips the
// outer quotes only, so DICOM_SOFTWARE_VERSIONS comes back as one string with the
// interior quotes still in it.
static const std::string phStringArrayXML = R"xml(<?xml version="1.0" encoding="UTF-8" ?>
<DataObject ObjectType="DPUfsImport">
    <Attribute Name="DICOM_MANUFACTURER" Group="0x0008" Element="0x0070" PMSVR="IString">PHILIPS</Attribute>
    <Attribute Name="DICOM_SOFTWARE_VERSIONS" Group="0x0018" Element="0x1020" PMSVR="IStringArray">"1.6.6186" "20150402_R48" "4.0.3"</Attribute>
    <Attribute Name="DICOM_LOSSY_IMAGE_COMPRESSION_METHOD" Group="0x0028" Element="0x2114" PMSVR="IStringArray">"PHILIPS_TIFF_1_0"</Attribute>
    <Attribute Name="PIM_DP_SOURCE_FILE" Group="0x301D" Element="0x1000" PMSVR="IStringArray"></Attribute>
</DataObject>)xml";

TEST_F(PHTDescriptionTests, getAttributeTextListSplitsQuotedValues) {
	PHTDescription description(phStringArrayXML);
	const std::vector<std::string> versions =
		description.getAttributeTextList(description.getRoot(), SOFTWARE_VERSIONS);
	EXPECT_EQ((std::vector<std::string>{"1.6.6186", "20150402_R48", "4.0.3"}), versions);
}

TEST_F(PHTDescriptionTests, getAttributeTextListReadsASingleQuotedValue) {
	PHTDescription description(phStringArrayXML);
	const std::vector<std::string> method =
		description.getAttributeTextList(description.getRoot(), LOSSY_IMAGE_COMPRESSION_METHOD);
	EXPECT_EQ((std::vector<std::string>{"PHILIPS_TIFF_1_0"}), method);
}

// Not every array valued attribute is quoted: a scanner that stores a plain string
// where the array is expected must still be readable.
TEST_F(PHTDescriptionTests, getAttributeTextListReadsAnUnquotedValueAsOneEntry) {
	PHTDescription description(phStringArrayXML);
	const std::vector<std::string> manufacturer =
		description.getAttributeTextList(description.getRoot(), MANUFACTURER);
	EXPECT_EQ((std::vector<std::string>{"PHILIPS"}), manufacturer);
}

TEST_F(PHTDescriptionTests, getAttributeTextListReturnsNothingForAnEmptyValue) {
	PHTDescription description(phStringArrayXML);
	EXPECT_TRUE(description.getAttributeTextList(description.getRoot(), SOURCE_FILE).empty());
}

TEST_F(PHTDescriptionTests, getAttributeTextListThrowsOnMissingAttribute) {
	PHTDescription description(phStringArrayXML);
	EXPECT_THROW(description.getAttributeTextList(description.getRoot(), UFS_BARCODE), slideio::RuntimeError);
}

TEST_F(PHTDescriptionTests, hasAttributeDetectsPresenceAndAbsence) {
	PHTDescription description(phSampleXML);
	EXPECT_TRUE(description.hasAttribute(description.getRoot(), MANUFACTURER));
	EXPECT_FALSE(description.hasAttribute(description.getRoot(), UFS_BARCODE));

	const std::vector<const tinyxml2::XMLElement*> images =
		description.getObjectList(description.getRoot(), SCANNED_IMAGES, SCANNED_IMAGE);
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
	const std::vector<const tinyxml2::XMLElement*> levels =
		description.getObjectList(image, LEVEL_SEQUENCE, PIXEL_DATA_REPRESENTATION);
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
	EXPECT_TRUE(description.getObjectList(description.getRoot(), SCANNED_IMAGES, SCANNED_IMAGE).empty());
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
	EXPECT_EQ(3u, moved.getObjectList(moved.getRoot(), SCANNED_IMAGES, SCANNED_IMAGE).size());

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

// The metadata generated by MockPHTIFFSlide::createFakeXml has to be readable by
// the parser it is meant to feed.
TEST_F(PHTDescriptionTests, createFakeXmlDefaultsDescribeAWholeSlideImage) {
	PHTDescription description(MockPHTIFFSlide::createFakeXml());
	EXPECT_EQ("PHILIPS", description.getAttributeText(description.getRoot(), MANUFACTURER));
	EXPECT_EQ("5.0", description.getAttributeText(description.getRoot(), UFS_INTERFACE_VERSION));

	const std::vector<const tinyxml2::XMLElement*> images =
		description.getObjectList(description.getRoot(), SCANNED_IMAGES, SCANNED_IMAGE);
	ASSERT_EQ(1u, images.size());
	EXPECT_EQ(WSI, description.getAttributeText(images[0], IMAGE_TYPE));
	EXPECT_EQ(91136, description.getAttributeInt(images[0], IMAGE_COLUMNS));
	EXPECT_EQ(68096, description.getAttributeInt(images[0], IMAGE_ROWS));
	EXPECT_EQ(3, description.getAttributeInt(images[0], SAMPLES_PER_PIXEL));
	EXPECT_EQ(8, description.getAttributeInt(images[0], BITS_ALLOCATED));
	EXPECT_EQ(7, description.getAttributeInt(images[0], HIGH_BIT));
	EXPECT_EQ("RGB", description.getAttributeText(images[0], PHOTOMETRIC_INTERPRETATION));
	EXPECT_EQ(9u, description.getObjectList(images[0], LEVEL_SEQUENCE, PIXEL_DATA_REPRESENTATION).size());
}

// Every level halves the size of the previous one and doubles its pixel spacing.
TEST_F(PHTDescriptionTests, createFakeXmlBuildsTheRequestedPyramid) {
	PHTDescription description(MockPHTIFFSlide::createFakeXml(1024, 512, 3));
	const std::vector<const tinyxml2::XMLElement*> images =
		description.getObjectList(description.getRoot(), SCANNED_IMAGES, SCANNED_IMAGE);
	ASSERT_EQ(1u, images.size());
	const std::vector<const tinyxml2::XMLElement*> levels =
		description.getObjectList(images[0], LEVEL_SEQUENCE, PIXEL_DATA_REPRESENTATION);
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
	PHTDescription description(MockPHTIFFSlide::createFakeXml(1024, 1024, 2, { "LABELIMAGE", "MACROIMAGE" }));
	const std::vector<const tinyxml2::XMLElement*> images =
		description.getObjectList(description.getRoot(), SCANNED_IMAGES, SCANNED_IMAGE);
	ASSERT_EQ(3u, images.size());
	EXPECT_EQ(WSI, description.getAttributeText(images[0], IMAGE_TYPE));
	EXPECT_FALSE(description.hasAttribute(images[0], IMAGE_DATA));

	EXPECT_EQ("LABELIMAGE", description.getAttributeText(images[1], IMAGE_TYPE));
	EXPECT_TRUE(description.hasAttribute(images[1], IMAGE_DATA));
	EXPECT_FALSE(description.getAttributeText(images[1], IMAGE_DATA).empty());
	EXPECT_TRUE(description.getObjectList(images[1], LEVEL_SEQUENCE, PIXEL_DATA_REPRESENTATION).empty());

	EXPECT_EQ("MACROIMAGE", description.getAttributeText(images[2], IMAGE_TYPE));
	EXPECT_TRUE(description.hasAttribute(images[2], IMAGE_DATA));
}

// A slide without a pyramid is still valid metadata.
TEST_F(PHTDescriptionTests, createFakeXmlSupportsAnEmptyPyramid) {
	PHTDescription description(MockPHTIFFSlide::createFakeXml(256, 256, 0));
	const std::vector<const tinyxml2::XMLElement*> images =
		description.getObjectList(description.getRoot(), SCANNED_IMAGES, SCANNED_IMAGE);
	ASSERT_EQ(1u, images.size());
	EXPECT_TRUE(description.getObjectList(images[0], LEVEL_SEQUENCE, PIXEL_DATA_REPRESENTATION).empty());
	EXPECT_EQ(256, description.getAttributeInt(images[0], IMAGE_COLUMNS));
}

// Levels never collapse to a zero size, however deep the pyramid is.
TEST_F(PHTDescriptionTests, createFakeXmlClampsLevelSizeToOnePixel) {
	PHTDescription description(MockPHTIFFSlide::createFakeXml(4, 2, 5));
	const std::vector<const tinyxml2::XMLElement*> images =
		description.getObjectList(description.getRoot(), SCANNED_IMAGES, SCANNED_IMAGE);
	ASSERT_EQ(1u, images.size());
	const std::vector<const tinyxml2::XMLElement*> levels =
		description.getObjectList(images[0], LEVEL_SEQUENCE, PIXEL_DATA_REPRESENTATION);
	ASSERT_EQ(5u, levels.size());
	EXPECT_EQ(1, description.getAttributeInt(levels[4], LEVEL_COLUMNS));
	EXPECT_EQ(1, description.getAttributeInt(levels[4], LEVEL_ROWS));
}

// The description of the first tiff directory is what identifies a philips file:
// the extension *.tif says nothing, gdal and ome-tiff use it too.
TEST_F(PHTDescriptionTests, isPhilipsDescriptionAcceptsPhilipsMetadata) {
	EXPECT_TRUE(PHTDescription::isPhilipsDescription(phSampleXML));
	EXPECT_TRUE(PHTDescription::isPhilipsDescription(MockPHTIFFSlide::createFakeXml()));
	EXPECT_TRUE(PHTDescription::isPhilipsDescription(MockPHTIFFSlide::fakeXML));
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
	EXPECT_TRUE(PHTDescription::isPhilipsDescription("\xEF\xBB\xBF" + MockPHTIFFSlide::fakeXML));
}
