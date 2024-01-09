#include <gtest/gtest.h>
#include <string>

#include "slideio/drivers/vsi/vsifile.hpp"
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
    const std::string filePath = TestTools::getFullTestImagePath("vsi", "Zenodo/Abdominal/G1M16_ABD_HE_B6.vsi");
    VSIFile file(filePath);
    EXPECT_TRUE(file.expectExternalFiles());
    EXPECT_TRUE(file.hasExternalFiles());
}

TEST(VSIFile, expectExternalFilesFalse) {
    std::string filePath = TestTools::getFullTestImagePath("vsi",
        "Zenodo/Q6VM49JF/Figure-1-ultrasound-raw-data"
        "/SPECTRUM_#201_2016-06-14_Jiangtao Liu/1286FL9057GDF8RGDX257R2GLHZ.vsi");
    VSIFile file(filePath);
    EXPECT_FALSE(file.expectExternalFiles());
}
