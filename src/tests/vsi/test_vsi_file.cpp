#include <gtest/gtest.h>
#include <string>

#include "slideio/drivers/vsi/vsifile.hpp"
#include "slideio/drivers/vsi/volume.hpp"
#include "slideio/drivers/vsi/vsitools.hpp"
#include "tests/testlib/testtools.hpp"


using namespace slideio;
using namespace slideio::vsi;

TEST(TagInfo, findChild) {
    vsi::TagInfo root;
    const int tags[] = { 1,2,3,4,5,6,7,8,9,2,2,9,2 };
    for (int tag : tags) {
        vsi::TagInfo tagInfo;
        tagInfo.tag = tag;
        root.children.push_back(tagInfo);
    }
    const vsi::TagInfo* tag5 = root.findChild(5);
    ASSERT_TRUE(tag5 != nullptr);
    EXPECT_EQ(tag5->tag, 5);

    const vsi::TagInfo* tag1 = root.findChild(1);
    ASSERT_TRUE(tag1 != nullptr);
    EXPECT_EQ(tag1->tag, 1);

    const vsi::TagInfo* tag9 = root.findChild(9);
    ASSERT_TRUE(tag9 != nullptr);
    EXPECT_EQ(tag9->tag, 9);

    const vsi::TagInfo* tag10 = root.findChild(10);
    ASSERT_TRUE(tag10 == nullptr);

    vsi::TagInfo* tag5_ = const_cast<vsi::TagInfo*>(tag5);
    for (int tag : tags) {
        vsi::TagInfo tagInfo;
        tagInfo.tag = tag;
        tag5_->children.push_back(tagInfo);
    }

    const vsi::TagInfo* tag5_6 = tag5_->findChild(6);
    ASSERT_TRUE(tag5_6 != nullptr);

    vsi::TagInfo* tag5_6_ = const_cast<vsi::TagInfo*>(tag5_6);
    for (int tag : tags) {
        vsi::TagInfo tagInfo;
        tagInfo.tag = tag;
        tag5_6_->children.push_back(tagInfo);
    }

    std::vector<int> path1 = { 5,6,3 };
    auto tag3 = root.findChild(path1);
    ASSERT_TRUE(tag3 != nullptr);
    EXPECT_EQ(tag3->tag, 3);

    std::vector<int> path2 = { 5,6 };
    auto tag6 = root.findChild(path2);
    ASSERT_TRUE(tag6 != nullptr);
    EXPECT_EQ(tag6->tag, 6);

    std::vector<int> path3 = { 5,6,10 };
    auto tag0 = root.findChild(path3);
    ASSERT_TRUE(tag0 == nullptr);

    {
        EXPECT_TRUE(root.findNextChild(10, root.begin()) == root.end());
        int count = 0;
        for (vsi::TagInfo::const_iterator it = root.begin(); it != root.end(); ++it) {
            it = root.findNextChild(2, it);
            EXPECT_EQ(it->tag, 2);
            ++count;
        }
        EXPECT_EQ(count, 4);
    }
}

TEST(VSIFile, expectExternalFilesTrue) {
    const std::string filePath = TestTools::getTestImagePath("vsi", "Zenodo/Abdominal/G1M16_ABD_HE_B6.vsi");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
    VSIFile file(filePath);
    EXPECT_TRUE(file.expectExternalFiles());
    EXPECT_TRUE(file.hasExternalFiles());
}

TEST(VSIFile, expectExternalFilesFalse) {
    std::string filePath = TestTools::getTestImagePath("vsi",
        "Zenodo/Q6VM49JF/Figure-1-ultrasound-raw-data"
        "/SPECTRUM_#201_2016-06-14_Jiangtao Liu/1286FL9057GDF8RGDX257R2GLHZ.vsi");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
    VSIFile file(filePath);
    EXPECT_FALSE(file.expectExternalFiles());
}


TEST(VSIFile, findChildRecursively) {

    {
        TagInfo root;
        const TagInfo* result = root.findChildRecursively(1);
        EXPECT_EQ(result, nullptr);
    }
    {
        TagInfo root;
        TagInfo child;
        child.tag = 1;
        root.addChild(child);

        const TagInfo* result = root.findChildRecursively(1);
        ASSERT_NE(result, nullptr);
        EXPECT_EQ(result->tag, 1);
    }

    {
        TagInfo root;
        TagInfo child1;
        child1.tag = 1;
        TagInfo child2;
        child2.tag = 2;
        child1.addChild(child2);
        root.addChild(child1);

        const TagInfo* result = root.findChildRecursively(2);
        ASSERT_NE(result, nullptr);
        EXPECT_EQ(result->tag, 2);
    }

    {
        TagInfo root;
        TagInfo child;
        child.tag = 1;
        root.addChild(child);

        const TagInfo* result = root.findChildRecursively(2);
        EXPECT_EQ(result, nullptr);
    }

    {
        TagInfo root;
        TagInfo child1;
        child1.tag = 1;
        TagInfo child2;
        child2.tag = 2;
        TagInfo child3;
        child3.tag = 3;
        root.addChild(child1);
        root.addChild(child2);
        root.addChild(child3);

        const TagInfo* result = root.findChildRecursively(2);
        ASSERT_NE(result, nullptr);
        EXPECT_EQ(result->tag, 2);
    }
}

TEST(Volume, DimensionOrdersAreUnsetUntilTheFileSetsThem) {
    // X and Y are fixed by the format. Every other dimension has to read the
    // unset sentinel until DIMENSION_DESCRIPTION supplies an order, otherwise a
    // caller cannot tell "not recorded" from the order 0 that belongs to X.
    vsi::Volume volume;
    EXPECT_EQ(volume.getDimensionOrder(Dimensions::X), 0);
    EXPECT_EQ(volume.getDimensionOrder(Dimensions::Y), 1);
    for (const Dimensions dim : {Dimensions::Z, Dimensions::C, Dimensions::T,
                                 Dimensions::L, Dimensions::P}) {
        EXPECT_EQ(volume.getDimensionOrder(dim), -1) << "dimension " << static_cast<int>(dim);
    }
}

TEST(Volume, PlaneTimestampsRoundTrip) {
    vsi::Volume volume;
    volume.setPlaneTimestamps({0.0, 2000.0, 4000.0});
    ASSERT_EQ(volume.getPlaneTimestampCount(), 3);
    EXPECT_DOUBLE_EQ(volume.getPlaneTimestampByIndex(0), 0.0);
    EXPECT_DOUBLE_EQ(volume.getPlaneTimestampByIndex(1), 2000.0);
    EXPECT_DOUBLE_EQ(volume.getPlaneTimestampByIndex(2), 4000.0);
    EXPECT_DOUBLE_EQ(volume.getPlaneTimestampByIndex(3), 0.0);
}

TEST(VSITools, UnitToSeconds) {
    EXPECT_FALSE(VSITools::unitToSeconds("").has_value());
    EXPECT_FALSE(VSITools::unitToSeconds("um").has_value());
    EXPECT_FALSE(VSITools::unitToSeconds("10^-3m^1").has_value());

    ASSERT_TRUE(VSITools::unitToSeconds("10^-3s^1").has_value());
    EXPECT_DOUBLE_EQ(*VSITools::unitToSeconds("10^-3s^1"), 1e-3);
    EXPECT_DOUBLE_EQ(*VSITools::unitToSeconds("10^-3 s^1"), 1e-3);
    EXPECT_DOUBLE_EQ(*VSITools::unitToSeconds("s^1"), 1.0);
    EXPECT_DOUBLE_EQ(*VSITools::unitToSeconds("s"), 1.0);
    EXPECT_DOUBLE_EQ(*VSITools::unitToSeconds("10^-6s^1"), 1e-6);
    EXPECT_DOUBLE_EQ(*VSITools::unitToSeconds("10^0s"), 1.0);
}

TEST(VSITools, PlaneTimestampListIndexFallbackTZC) {
    // Unset orders (< 2) → (t*nZ+z)*nC+c
    EXPECT_EQ(VSITools::planeTimestampListIndex(0, 0, 0, 25, 2, 1, -1, -1, -1), 0);
    EXPECT_EQ(VSITools::planeTimestampListIndex(0, 1, 0, 25, 2, 1, -1, -1, -1), 1);
    EXPECT_EQ(VSITools::planeTimestampListIndex(1, 0, 0, 25, 2, 1, -1, -1, -1), 2);
    EXPECT_EQ(VSITools::planeTimestampListIndex(1, 1, 0, 25, 2, 1, -1, -1, -1), 3);
}

TEST(VSITools, PlaneTimestampListIndexChannelMajor) {
    // IX73-style DIMENSION_DESCRIPTION: T=2, Z=3, C=4 → T fastest, C slowest.
    constexpr int orderT = 2, orderZ = 3, orderC = 4;
    constexpr int nT = 25, nC = 2, nZ = 1;
    EXPECT_EQ(VSITools::planeTimestampListIndex(0, 0, 0, nT, nC, nZ, orderT, orderC, orderZ), 0);
    EXPECT_EQ(VSITools::planeTimestampListIndex(1, 0, 0, nT, nC, nZ, orderT, orderC, orderZ), 1);
    EXPECT_EQ(VSITools::planeTimestampListIndex(24, 0, 0, nT, nC, nZ, orderT, orderC, orderZ), 24);
    EXPECT_EQ(VSITools::planeTimestampListIndex(0, 1, 0, nT, nC, nZ, orderT, orderC, orderZ), 25);
    EXPECT_EQ(VSITools::planeTimestampListIndex(1, 1, 0, nT, nC, nZ, orderT, orderC, orderZ), 26);
    EXPECT_EQ(VSITools::planeTimestampListIndex(24, 1, 0, nT, nC, nZ, orderT, orderC, orderZ), 49);
}

namespace {
    vsi::TagInfo makeChild(int tag, const std::string& value) {
        vsi::TagInfo child;
        child.tag = tag;
        child.value = value;
        return child;
    }
}

TEST(VSITools, APlaneTimestampNodeIsTheOneStatingATimeUnit) {
    // TIME_VALUE and VECTOR_LAYER_VOLUME are both tag 2017, so the tag alone
    // cannot tell them apart. A timestamp states the unit of its value; a vector
    // layer is a document subtree and states none.
    vsi::TagInfo timestamp;
    timestamp.tag = Tag::TIME_VALUE;
    timestamp.children.push_back(makeChild(Tag::UNITS, "10^-3s^1"));
    timestamp.children.push_back(makeChild(Tag::VALUE, "29559.439000"));
    EXPECT_TRUE(VSITools::isPlaneTimestampNode(timestamp));

    vsi::TagInfo vectorLayer;
    vectorLayer.tag = Tag::VECTOR_LAYER_VOLUME;   // the same 2017
    vectorLayer.children.push_back(makeChild(6, "1162180352"));
    vectorLayer.children.push_back(makeChild(5, "xv.idgen.unique"));
    EXPECT_FALSE(VSITools::isPlaneTimestampNode(vectorLayer));
}

TEST(VSITools, APlaneTimestampNodeNeedsTheUnitToBeATimeUnit) {
    vsi::TagInfo node;
    node.tag = Tag::TIME_VALUE;
    node.children.push_back(makeChild(Tag::UNITS, "m^1"));
    node.children.push_back(makeChild(Tag::VALUE, "12.5"));
    EXPECT_FALSE(VSITools::isPlaneTimestampNode(node));

    vsi::TagInfo other;
    other.tag = Tag::MULTIDIM_STACK_PROPERTIES;
    other.children.push_back(makeChild(Tag::UNITS, "10^-3s^1"));
    EXPECT_FALSE(VSITools::isPlaneTimestampNode(other));
}

TEST(VSITools, PlaneTimestampListIndexIgnoresTheOrderOfASingletonDimension) {
    // One time frame, so the file need not state an order for T. A dimension of
    // extent 1 contributes nothing to the index whatever its position, so the
    // described layout of C and Z still applies: Z fastest, C slowest, index z + 11c.
    constexpr int orderT = -1, orderZ = 2, orderC = 3;
    constexpr int nT = 1, nC = 2, nZ = 11;
    EXPECT_EQ(VSITools::planeTimestampListIndex(0, 0, 0, nT, nC, nZ, orderT, orderC, orderZ), 0);
    EXPECT_EQ(VSITools::planeTimestampListIndex(0, 0, 1, nT, nC, nZ, orderT, orderC, orderZ), 1);
    EXPECT_EQ(VSITools::planeTimestampListIndex(0, 0, 10, nT, nC, nZ, orderT, orderC, orderZ), 10);
    EXPECT_EQ(VSITools::planeTimestampListIndex(0, 1, 0, nT, nC, nZ, orderT, orderC, orderZ), 11);
    EXPECT_EQ(VSITools::planeTimestampListIndex(0, 1, 1, nT, nC, nZ, orderT, orderC, orderZ), 12);
}

TEST(VSITools, PlaneTimestampListIndexFallsBackWhenAPresentDimensionHasNoOrder) {
    // Z has extent, so its missing order leaves the layout undescribed and the
    // TZC fallback applies: (t*nZ + z)*nC + c.
    constexpr int orderT = 2, orderZ = -1, orderC = 3;
    constexpr int nT = 2, nC = 2, nZ = 11;
    EXPECT_EQ(VSITools::planeTimestampListIndex(0, 0, 1, nT, nC, nZ, orderT, orderC, orderZ), 2);
    EXPECT_EQ(VSITools::planeTimestampListIndex(1, 1, 0, nT, nC, nZ, orderT, orderC, orderZ), 23);
}

TEST(Volume, TResolutionRequiresParseableUnit) {
    vsi::Volume volume;
    volume.setTResolution(2000.0);
    EXPECT_DOUBLE_EQ(volume.getTResolution(), 0.0);

    volume.setTResolutionUnit("10^-3s^1");
    EXPECT_DOUBLE_EQ(volume.getTResolution(), 2.0);

    volume.setTResolutionUnit("bogus");
    EXPECT_DOUBLE_EQ(volume.getTResolution(), 0.0);
}

TEST(Volume, PlaneTimestampsScaledByUnit) {
    vsi::Volume volume;
    volume.setPlaneTimestamps({10003.0, 12003.0});
    volume.setPlaneTimestampUnit("10^-3s^1");
    EXPECT_NEAR(volume.getPlaneTimestampByIndex(0), 10.003, 1e-9);
    EXPECT_NEAR(volume.getPlaneTimestampByIndex(1), 12.003, 1e-9);
}