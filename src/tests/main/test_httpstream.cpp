// src/tests/main/test_httpstream.cpp
// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <gtest/gtest.h>
#include "slideio/imagetools/httpstream.hpp"
#include "http_fixture/http_fixture.hpp"

#include <filesystem>
#include <fstream>
#include <string>

using slideio::tests::HttpFixture;

namespace {
std::filesystem::path makeRoot() {
    auto p = std::filesystem::temp_directory_path() / "slideio_http_fixture";
    std::filesystem::create_directories(p);
    return p;
}
}

TEST(HttpStreamTest, SizeFromHead) {
    auto root = makeRoot();
    std::filesystem::path file = root / "size.bin";
    {
        std::ofstream out(file, std::ios::binary);
        for (int i = 0; i < 12345; ++i) out.put(static_cast<char>(i & 0xff));
    }
    HttpFixture fx(root);
    slideio::HttpStream s(fx.url("size.bin"));
    EXPECT_EQ(s.size(), 12345u);
}
