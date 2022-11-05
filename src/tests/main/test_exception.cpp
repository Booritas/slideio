// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <gtest/gtest.h>
#include "slideio/core/base.hpp"
#include "tests/testlib/testtools.hpp"

using namespace  slideio;

TEST(Exception, riseError)
{
    const int intVal = 1;
    const std::string stringVal = "sssss";
    int thrown = 0;

    try {
        try {
            RAISE_RUNTIME_ERROR << "Error " << intVal << " " << stringVal;
            ASSERT_FALSE(true);
        }
        catch (std::exception& ex) {
            thrown++;
            std::string message = ex.what();
            EXPECT_TRUE(message.find("Error 1 sssss") >= 0);
            throw;
        }
    }
    catch(std::exception& ex) {
        thrown++;
        std::string message = ex.what();
        EXPECT_TRUE(message.find("Error 1 sssss") >= 0);
    }
    EXPECT_EQ(thrown, 2);
}