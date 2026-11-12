// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <gtest/gtest.h>
#include <clocale>
#include <cstdlib>
#include <string>
#include "slideio/drivers/lif/lifxml.hpp"
#include "slideio/drivers/lif/liflayout.hpp"
#include "slideio/drivers/lif/lifcontainer.hpp"
#include "slideio/core/tools/filereader.hpp"

using namespace slideio;
using namespace slideio::lif;

namespace
{
    std::vector<ImageDescription> parseFixture(const char* name)
    {
        FileReader file(std::string(LIF_TEST_DATA_DIR) + "/" + name);
        const Container c = Container::read(file);
        return parseImages(c.description());
    }

    const ImageDescription& named(const std::vector<ImageDescription>& v, const std::string& path)
    {
        for (const auto& d : v) {
            if (d.path == path) {
                return d;
            }
        }
        throw std::runtime_error("no image " + path);
    }

    // An Element holding one image with the given memory id/size.
    std::string image(const std::string& name, const std::string& mem, const char* size = "10",
                      const std::string& children = "")
    {
        return "<Element Name=\"" + name + "\"><Data><Image><ImageDescription><Channels>"
            "<ChannelDescription DataType=\"0\" Resolution=\"8\" BytesInc=\"0\"/></Channels>"
            "<Dimensions><DimensionDescription DimID=\"1\" NumberOfElements=\"4\" BytesInc=\"1\"/>"
            "</Dimensions></ImageDescription></Image></Data>"
            "<Memory MemoryBlockID=\"" + mem + "\" Size=\"" + size + "\"/>" + children + "</Element>";
    }

    // Sets LC_NUMERIC to the first comma-decimal locale the system has, and puts the
    // previous one back on destruction, whichever way the test leaves.
    class CommaDecimalLocale
    {
    public:
        CommaDecimalLocale()
        {
            const char* current = std::setlocale(LC_NUMERIC, nullptr);
            m_saved = current ? current : "C";
            for (const char* name : {"de_DE.UTF-8", "German_Germany.1252", "de_DE", "de_DE.utf8", "German"}) {
                if (std::setlocale(LC_NUMERIC, name)) {
                    m_name = name;
                    break;
                }
            }
        }
        ~CommaDecimalLocale() { std::setlocale(LC_NUMERIC, m_saved.c_str()); }
        CommaDecimalLocale(const CommaDecimalLocale&) = delete;
        CommaDecimalLocale& operator=(const CommaDecimalLocale&) = delete;
        /// The locale set, empty when none could be.
        const std::string& name() const { return m_name; }
    private:
        std::string m_saved;
        std::string m_name;
    };

    std::string wrap(const std::string& inner)
    {
        return "<LMSDataContainerHeader Version=\"2\"><Element Name=\"Exp\"><Data/><Children>"
            + inner + "</Children></Element></LMSDataContainerHeader>";
    }
}

TEST(LifXml, PathOmitsExperiment)
{
    const auto v = parseFixture("nesting.lif");
    ASSERT_EQ(4u, v.size());
    EXPECT_EQ("Nest/CZMT", v[0].path);
    EXPECT_EQ("Nest/ZCMT", v[1].path);
    EXPECT_EQ("Nest/MCZT", v[2].path);
    EXPECT_EQ("Nest/CZTM", v[3].path);
}

TEST(LifXml, SkipsContainersWithoutMemory)
{
    const std::string xml = wrap(
        "<Element Name=\"Folder\"><Data/><Memory Size=\"0\" MemoryBlockID=\"\"/><Children>"
        "<Element Name=\"NoMemory\"><Data/></Element></Children></Element>" + image("Img", "M1"));
    const auto v = parseImages(xml);
    ASSERT_EQ(1u, v.size());
    EXPECT_EQ("Img", v[0].path);
}

TEST(LifXml, DropsProcessingHistory)
{
    const std::string hist =
        "<ProcessingHistory><Data><Image><ImageDescription/></Image></Data>"
        "<Memory MemoryBlockID=\"H1\" Size=\"10\"/></ProcessingHistory>";
    const std::string xml = wrap(
        "<Element Name=\"Img\"><Data><Image><ImageDescription/></Image></Data>"
        "<Memory MemoryBlockID=\"M1\" Size=\"10\"/>" + hist + "</Element>");
    const auto v = parseImages(xml);
    ASSERT_EQ(1u, v.size());
    EXPECT_EQ("M1", v[0].memoryId);
}

TEST(LifXml, DropsSequentialMaster)
{
    const std::string master =
        "<LDM_Block_Sequential_Master><Element Name=\"Seq\"><Data><Image><ImageDescription/></Image></Data>"
        "<Memory MemoryBlockID=\"S1\" Size=\"10\"/></Element></LDM_Block_Sequential_Master>";
    const std::string xml = wrap(image("Img", "M1") + master);
    const auto v = parseImages(xml);
    ASSERT_EQ(1u, v.size());
    EXPECT_EQ("Img", v[0].path);
}

TEST(LifXml, FirstDescriptionPerMemoryId)
{
    const auto v = parseImages(wrap(image("First", "M1") + image("Second", "M1") + image("Other", "M2")));
    ASSERT_EQ(2u, v.size());
    EXPECT_EQ("First", v[0].path);
    EXPECT_EQ("Other", v[1].path);
}

TEST(LifXml, ReadsDimensionsAndChannels)
{
    const auto v = parseFixture("types.lif");
    const ImageDescription& d = named(v, "U16_12bit");
    ASSERT_EQ(1u, d.channels.size());
    EXPECT_EQ(12, d.channels[0].resolution);
    EXPECT_EQ(0, d.channels[0].dataType);
    EXPECT_EQ(0, d.channels[0].channelTag);
    EXPECT_EQ("Gray", [&] {
        for (const auto& a : d.channels[0].attributes) {
            if (a.first == "LUTName") {
                return a.second;
            }
        }
        return std::string();
    }());
    ASSERT_EQ(2u, d.dimensions.size());
    EXPECT_EQ(1, d.dimensions[0].dimId);
    EXPECT_EQ(16, d.dimensions[0].count);
    EXPECT_EQ(2, d.dimensions[0].bytesInc);
    EXPECT_EQ("MemBlock_1", d.memoryId);
    EXPECT_EQ(-1, named(v, "NoDataType16").channels[0].dataType);
    EXPECT_EQ(-1, named(parseFixture("rgb.lif"), "Untagged").channels[0].channelTag);
}

TEST(LifXml, ReadsMagnification)
{
    const auto v = parseFixture("v2_basic.lif");
    ASSERT_EQ(1u, v.size());
    EXPECT_EQ(63.0, v[0].magnification);
    EXPECT_EQ(16 * 12, v[0].declaredSize);
}

namespace
{
    std::vector<ImageDescription> withAttachment(const std::string& attachment)
    {
        return parseImages(wrap("<Element Name=\"T\"><Data><Image><ImageDescription/>" + attachment
            + "</Image></Data><Memory MemoryBlockID=\"M\" Size=\"4\"/></Element>"));
    }
}

// The shape of IT4_27_02_15.lif (figshare), trimmed: the attachment is a
// HardwareSettingList and the definitions live only inside LDM_Block_Sequential.
TEST(LifXml, ReadsMagnificationFromSequentialList)
{
    const auto v = withAttachment(
        "<Attachment Name=\"HardwareSettingList\" Application=\"LAS AF\">"
        "<HardwareSetting Name=\"default\"><ScannerSetting/>"
        "<LDM_Block_Sequential>"
        "<LDM_Block_Sequential_Master><ATLConfocalSettingDefinition Magnification=\"100\" ObjectiveName=\"Dummy6\"/>"
        "</LDM_Block_Sequential_Master>"
        "<LDM_Block_Sequential_List><ATLConfocalSettingDefinition Magnification=\"100\" ObjectiveName=\"Dummy6\"/>"
        "<ATLConfocalSettingDefinition Magnification=\"100\" ObjectiveName=\"Dummy6\"/></LDM_Block_Sequential_List>"
        "<LDM_Block_Sequential_Execution_Mask><InstrumentSettingMask/></LDM_Block_Sequential_Execution_Mask>"
        "</LDM_Block_Sequential><FilterSetting/></HardwareSetting></Attachment>");
    ASSERT_EQ(1u, v.size());
    EXPECT_EQ(100.0, v[0].magnification);
}

// A direct definition wins over a sequential one, as in Project007.
TEST(LifXml, DirectMagnificationWinsOverSequential)
{
    const auto v = withAttachment(
        "<Attachment Name=\"HardwareSetting\">"
        "<LDM_Block_Sequential><LDM_Block_Sequential_Master>"
        "<ATLConfocalSettingDefinition Magnification=\"10\"/></LDM_Block_Sequential_Master></LDM_Block_Sequential>"
        "<ATLConfocalSettingDefinition Magnification=\"63\"/></Attachment>");
    ASSERT_EQ(1u, v.size());
    EXPECT_EQ(63.0, v[0].magnification);
}

TEST(LifXml, NoMagnificationIsZero)
{
    const auto v = withAttachment("<Attachment Name=\"TileScanInfo\"/>"
        "<Attachment Name=\"HardwareSettingList\"><HardwareSetting/></Attachment>");
    ASSERT_EQ(1u, v.size());
    EXPECT_EQ(0.0, v[0].magnification);
}

TEST(LifXml, ReadsDelayAndKeepsTilesInRawXml)
{
    const std::string att =
        "<Data><Image><ImageDescription/>"
        "<Attachment Name=\"TileScanInfo\"><Tile FieldX=\"0\" FieldY=\"0\" PosX=\"0.5\" PosY=\"0.25\"/>"
        "<Tile FieldX=\"1\" FieldY=\"0\" PosX=\"0.6\" PosY=\"0.25\"/></Attachment>"
        "<Attachment Name=\"HardwareSetting\"><ScannerSettingRecord Identifier=\"nDelayTime_s\" Variant=\"2.5\"/>"
        "</Attachment></Image></Data>";
    const auto v = parseImages(wrap("<Element Name=\"T\">" + att + "<Memory MemoryBlockID=\"M\" Size=\"4\"/></Element>"));
    ASSERT_EQ(1u, v.size());
    EXPECT_DOUBLE_EQ(2.5, v[0].delayTimeSeconds);
    // Field j's record is the j-th Tile of the raw XML (spec 4.5, 6.4).
    const std::string& raw = v[0].rawXml;
    const size_t first = raw.find("<Tile FieldX=\"0\" FieldY=\"0\" PosX=\"0.5\" PosY=\"0.25\"/>");
    const size_t second = raw.find("<Tile FieldX=\"1\" FieldY=\"0\" PosX=\"0.6\" PosY=\"0.25\"/>");
    ASSERT_NE(std::string::npos, first) << raw;
    ASSERT_NE(std::string::npos, second) << raw;
    EXPECT_LT(first, second);
}

TEST(LifXml, RawXmlExcludesChildElements)
{
    const std::string xml = wrap(image("Parent", "M1", "10", "<Children>" + image("ChildName", "M2") + "</Children>"));
    const auto v = parseImages(xml);
    ASSERT_EQ(2u, v.size());
    EXPECT_NE(std::string::npos, v[0].rawXml.find("Parent"));
    EXPECT_EQ(std::string::npos, v[0].rawXml.find("ChildName"));
    EXPECT_NE(std::string::npos, v[0].rawXml.find("ImageDescription"));
    EXPECT_NE(std::string::npos, v[1].rawXml.find("ChildName"));
}

TEST(LifXml, EveryTopLevelElement)
{
    // tinyxml2 accepts several top-level elements; images under each are found, and
    // a leading declaration does not stand in the way.
    const std::string body = wrap(image("A", "M1")) + "<Element Name=\"Exp2\"><Data/><Children>"
        + image("B", "M2") + "</Children></Element>";
    for (const std::string& xml : {body, "<?xml version=\"1.0\"?>" + body}) {
        const auto v = parseImages(xml);
        ASSERT_EQ(2u, v.size()) << xml;
        EXPECT_EQ("A", v[0].path);
        EXPECT_EQ("B", v[1].path);
    }
}

TEST(LifXml, WrapsOnlyTextThatDoesNotParse)
{
    // Several top-level elements parse as they are: no wrapper.
    {
        const XmlDescription d("<A/><B/>");
        EXPECT_FALSE(d.wrapped());
        EXPECT_EQ(2u, d.topLevel().size());
    }
    // Text after the last element fails a plain parse and passes inside the wrapper,
    // and a leading declaration is taken out of the way first.
    for (const std::string xml : {std::string("<A/>text<B/>tail"),
                                  std::string("<?xml version=\"1.0\"?> <A/>text<B/>tail")}) {
        const XmlDescription d(xml);
        EXPECT_TRUE(d.wrapped()) << xml;
        EXPECT_EQ(2u, d.topLevel().size()) << xml;
    }
}

TEST(LifXml, NoElementThrows)
{
    // Wrapped, bare text parses but holds no element; the error quotes the plain parse.
    try {
        XmlDescription d("text");
        FAIL() << "expected an exception";
    }
    catch (const std::exception& ex) {
        const std::string what = ex.what();
        EXPECT_NE(std::string::npos, what.find("LIF: malformed XML description: ")) << what;
        EXPECT_NE(std::string::npos, what.find("XML_ERROR_PARSING_TEXT")) << what;
    }
}

TEST(LifXml, MalformedThrows)
{
    try {
        parseImages("<a><b></a>");
        FAIL() << "expected an exception";
    }
    catch (const std::exception& ex) {
        EXPECT_NE(std::string::npos, std::string(ex.what()).find("LIF: malformed")) << ex.what();
    }
}

TEST(LifXml, NumbersIgnoreProcessLocale)
{
    const CommaDecimalLocale locale;
    if (locale.name().empty()) {
        GTEST_SKIP() << "no comma-decimal locale can be set on this system";
    }
    // The locale really does read a comma as the decimal point.
    ASSERT_EQ(1.5, std::strtod("1,5", nullptr)) << locale.name();
    const auto v = parseFixture("v2_basic.lif");
    ASSERT_EQ(1u, v.size());
    ASSERT_EQ(2u, v[0].dimensions.size());
    EXPECT_NEAR(1.5e-5, v[0].dimensions[0].length, 1e-15) << locale.name();
    EXPECT_NEAR(1.1e-5, v[0].dimensions[1].length, 1e-15) << locale.name();
    EXPECT_DOUBLE_EQ(63., v[0].magnification);
    const Layout l = Layout::build(v[0], 16 * 12);
    EXPECT_NEAR(1e-6, l.calibration().x, 1e-12) << locale.name();
    EXPECT_NEAR(1e-6, l.calibration().y, 1e-12) << locale.name();
}
