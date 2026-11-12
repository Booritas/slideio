// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <gtest/gtest.h>
#include <set>
#include <string>
#include "slideio/drivers/nd2/nd2experiment.hpp"
#include "nd2_test_trees.hpp"

using namespace slideio;
using namespace slideio::nd2;

namespace
{
    Experiment experimentOf(const char* name)
    {
        const nd2test::Trees t = nd2test::loadTrees(name);
        return Experiment::parse(&t.experiment);
    }

    std::string describe(const Experiment& e)
    {
        std::string s;
        for (const Loop& l : e.loops()) {
            if (!s.empty()) s += ",";
            s += (l.axis == Axis::T ? "T" : l.axis == Axis::P ? "P" : "Z");
            s += std::to_string(l.count);
        }
        return s;
    }

    std::string refusal(const char* name)
    {
        try {
            experimentOf(name);
        }
        catch (const Unsupported& e) {
            return e.what();
        }
        return "";
    }
}

TEST(Nd2Experiment, Tpz)
{
    const Experiment e = experimentOf("v3_tpz.nd2");
    EXPECT_EQ(describe(e), "T3,P2,Z4");
    ASSERT_NE(e.loop(Axis::P), nullptr);
    EXPECT_EQ(e.loop(Axis::P)->validItems, (std::vector<int64_t>{ 0, 2 }));
    EXPECT_EQ(e.count(Axis::T), 3);
    EXPECT_EQ(e.count(Axis::P), 2);
    EXPECT_EQ(e.count(Axis::Z), 4);
    EXPECT_EQ(e.loopProduct(), 24);
    EXPECT_EQ(e.frameIndex(1, 1, 2), (1 * 2 + 1) * 4 + 2);
    EXPECT_EQ(e.frameIndex(2, 1, 3), 23);
    EXPECT_DOUBLE_EQ(e.loop(Axis::Z)->pars.get("dZStep")->asDouble(), 1.5);
    EXPECT_DOUBLE_EQ(e.loop(Axis::T)->pars.get("dPeriod")->asDouble(), 2000.0);
}

TEST(Nd2Experiment, Orders)
{
    const Experiment pz = experimentOf("order_pz.nd2");
    EXPECT_EQ(describe(pz), "P2,Z3");
    EXPECT_EQ(pz.frameIndex(0, 1, 2), 5);
    const Experiment zt = experimentOf("order_zt.nd2");
    EXPECT_EQ(describe(zt), "Z2,T3");
    EXPECT_EQ(zt.frameIndex(2, 0, 1), 1 * 3 + 2);
}

TEST(Nd2Experiment, NoLoops)
{
    const Experiment e = experimentOf("v3_basic.nd2");
    EXPECT_TRUE(e.loops().empty());
    EXPECT_EQ(e.loopProduct(), 1);
    EXPECT_EQ(e.frameIndex(0, 0, 0), 0);
    EXPECT_EQ(e.count(Axis::T), 1);
    EXPECT_EQ(e.loop(Axis::T), nullptr);
}

TEST(Nd2Experiment, NullExperiment)
{
    const Experiment e = Experiment::parse(nullptr);
    EXPECT_TRUE(e.loops().empty());
    EXPECT_EQ(e.loopProduct(), 1);
}

TEST(Nd2Experiment, NeTimeAndSpectral)
{
    EXPECT_EQ(describe(experimentOf("ne_time.nd2")), "T2,Z2");
}

TEST(Nd2Experiment, WrappedLoopPars)
{
    const Experiment e = experimentOf("v21_xml.nd2");
    EXPECT_EQ(describe(e), "T2,Z3");
}

TEST(Nd2Experiment, FrameIndexIsChecked)
{
    const Experiment e = experimentOf("v3_tpz.nd2");
    EXPECT_THROW(e.frameIndex(3, 0, 0), std::exception);
    EXPECT_THROW(e.frameIndex(0, 2, 0), std::exception);
    EXPECT_THROW(e.frameIndex(0, 0, 4), std::exception);
    EXPECT_THROW(e.frameIndex(-1, 0, 0), std::exception);
    const Experiment none = experimentOf("v3_basic.nd2");
    EXPECT_THROW(none.frameIndex(0, 0, 1), std::exception);
}

TEST(Nd2Experiment, TrimTime)
{
    Experiment missing = experimentOf("frames_missing.nd2");
    ASSERT_EQ(describe(missing), "T5,Z2");
    missing.trimTime({ 0, 1, 2, 4, 5 });
    EXPECT_EQ(missing.count(Axis::T), 3);
    EXPECT_EQ(missing.loopProduct(), 6);

    Experiment surplus = experimentOf("frames_surplus.nd2");
    surplus.trimTime({ 0, 1, 2, 3, 4 });
    EXPECT_EQ(surplus.count(Axis::Z), 3);
    EXPECT_EQ(surplus.loopProduct(), 3);

    Experiment zt = experimentOf("order_zt.nd2");
    zt.trimTime({ 0 });
    EXPECT_EQ(describe(zt), "Z2,T3");

    // Frames beyond the loop product do not keep a time point alive.
    Experiment beyond = experimentOf("frames_missing.nd2");
    beyond.trimTime({ 0, 1, 2, 99 });
    EXPECT_EQ(beyond.count(Axis::T), 2);

    // No frame inside the loops at all: nothing to trim to.
    Experiment none = experimentOf("frames_missing.nd2");
    none.trimTime({});
    EXPECT_EQ(none.count(Axis::T), 5);
}

TEST(Nd2Experiment, Refusals)
{
    EXPECT_THROW(experimentOf("custom_loop.nd2"), Unsupported);
    EXPECT_THROW(experimentOf("two_time_loops.nd2"), Unsupported);
    const std::string two = refusal("two_time_loops.nd2");
    EXPECT_NE(two.find("T2, T3"), std::string::npos) << two;
    const std::string custom = refusal("custom_loop.nd2");
    EXPECT_NE(custom.find("3"), std::string::npos) << custom;
}
