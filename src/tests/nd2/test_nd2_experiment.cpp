// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <gtest/gtest.h>
#include <cstdint>
#include <limits>
#include <set>
#include <vector>
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

// Design 4.5 (R25): with T outermost, T = ceil((maxPresent + 1) / innerProduct), which
// trims trailing empty time points and extends T over surplus frames; otherwise nothing.
TEST(Nd2Experiment, FitTime)
{
    Experiment missing = experimentOf("frames_missing.nd2");
    ASSERT_EQ(describe(missing), "T5,Z2");
    missing.fitTime({ 0, 1, 2, 4, 5 });
    EXPECT_EQ(missing.count(Axis::T), 3);
    EXPECT_EQ(missing.loopProduct(), 6);

    // Z only: surplus frames 3 and 4 change nothing and stay outside the loops.
    Experiment surplus = experimentOf("frames_surplus.nd2");
    ASSERT_EQ(describe(surplus), "Z3");
    surplus.fitTime({ 0, 1, 2, 3, 4 });
    EXPECT_EQ(describe(surplus), "Z3");
    EXPECT_EQ(surplus.loopProduct(), 3);

    // T not outermost: neither trimmed nor extended.
    Experiment zt = experimentOf("order_zt.nd2");
    zt.fitTime({ 0 });
    EXPECT_EQ(describe(zt), "Z2,T3");
    zt.fitTime({ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 });
    EXPECT_EQ(describe(zt), "Z2,T3");

    // Frames beyond the loop product extend T to cover the highest one.
    Experiment beyond = experimentOf("frames_missing.nd2");
    beyond.fitTime({ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 });
    EXPECT_EQ(beyond.count(Axis::T), 6);
    EXPECT_EQ(beyond.loopProduct(), 12);
    EXPECT_EQ(beyond.frameIndex(5, 0, 1), 11);

    // An index no set of present frames could reach (>= product + count) is a damaged
    // name: it is ignored, and the rest trims as usual.
    Experiment damaged = experimentOf("frames_missing.nd2");
    damaged.fitTime({ 0, 1, 2, 15, int64_t(1) << 62 });   // bound 10 + 5
    EXPECT_EQ(damaged.count(Axis::T), 2);

    // No frame at all: nothing to fit to.
    Experiment none = experimentOf("frames_missing.nd2");
    none.fitTime({});
    EXPECT_EQ(none.count(Axis::T), 5);
}

namespace
{
    // T2 x Z3, declared product 6, frames 0..n-1 present.
    Experiment tz()
    {
        Loop t;
        t.axis = Axis::T;
        t.count = 2;
        Loop z;
        z.axis = Axis::Z;
        z.count = 3;
        return Experiment::fromLoops({ t, z });
    }

    std::set<int64_t> framesUpTo(int64_t n)
    {
        std::set<int64_t> frames;
        for (int64_t f = 0; f < n; ++f) {
            frames.insert(f);
        }
        return frames;
    }

    // Frame f at f*100 ms, then `tail` rewritten.
    std::vector<double> times(int64_t n, const std::vector<std::pair<int64_t, double>>& tail = {})
    {
        std::vector<double> v;
        for (int64_t f = 0; f < n; ++f) {
            v.push_back(100.0 * static_cast<double>(f));
        }
        for (const auto& [f, ms] : tail) {
            v[static_cast<size_t>(f)] = ms;
        }
        return v;
    }
}

// R29: a complete surplus time point whose times follow the series extends T, as in
// old_cells_02.nd2 (T41 x P4 x Z7, 1176 frames = 42 time points).
TEST(Nd2Experiment, FitTimeExtendsByCompleteSurplus)
{
    Experiment e = tz();
    e.fitTime(framesUpTo(12), times(12));
    EXPECT_EQ(describe(e), "T4,Z3");
    EXPECT_EQ(e.frameIndex(3, 0, 2), 11);
}

// R29: a partial surplus time point (190821: 10 of 46 positions) does not.
TEST(Nd2Experiment, FitTimeIgnoresPartialSurplus)
{
    Experiment e = tz();
    e.fitTime(framesUpTo(8), times(8));   // frames 6, 7 of 6..8
    EXPECT_EQ(describe(e), "T2,Z3");
    // A complete point followed by a partial one: only the complete one is added.
    Experiment f = tz();
    f.fitTime(framesUpTo(11), times(11));
    EXPECT_EQ(describe(f), "T3,Z3");
}

// R29: a complete surplus time point whose times run back into the series
// (130606_JCC719_003.nd2) does not extend T, and nothing after it does either.
TEST(Nd2Experiment, FitTimeIgnoresSurplusRunningBack)
{
    Experiment e = tz();
    // Frames 6..8 at 250..270 ms, before frame 5 (500 ms); frames 9..11 would follow.
    e.fitTime(framesUpTo(12), times(12, { {6, 250.}, {7, 260.}, {8, 270.} }));
    EXPECT_EQ(describe(e), "T2,Z3");
    // Times equal to the last declared one do not continue the series either.
    Experiment f = tz();
    f.fitTime(framesUpTo(9), times(9, { {6, 500.} }));
    EXPECT_EQ(describe(f), "T2,Z3");
}

// R29: with no times known, completeness alone decides; one unknown time does not
// block a point whose known times follow.
TEST(Nd2Experiment, FitTimeWithoutTimes)
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    Experiment e = tz();
    e.fitTime(framesUpTo(12));
    EXPECT_EQ(describe(e), "T4,Z3");
    Experiment f = tz();
    f.fitTime(framesUpTo(12), std::vector<double>(12, nan));
    EXPECT_EQ(describe(f), "T4,Z3");
    Experiment g = tz();
    g.fitTime(framesUpTo(9), times(9, { {6, nan} }));
    EXPECT_EQ(describe(g), "T3,Z3");
    Experiment partial = tz();
    partial.fitTime(framesUpTo(8));
    EXPECT_EQ(describe(partial), "T2,Z3");
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

namespace
{
    // The shape of aryeh/old_cells_02.nd2 (Ver2.1 CLX XML), trimmed to what the loops need:
    // every uLoopPars wraps its record in one `no_name` (a one-element list once decoded),
    // and every pItemValid is an empty CLxListVariant. FD 7.2 / validation inventory:
    // T41 (the valid period of an NE loop), P4, Z7.
    const char* const kOldCellsShape =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?><variant version=\"1.0\">"
        "<no_name runtype=\"RLxExperiment\"><eType runtype=\"lx_uint32\" value=\"8\"/>"
        "<uLoopPars runtype=\"CLxListVariant\"><no_name runtype=\"RLxExperiment.RLxExpNETimeLoop\">"
        "<uiCount runtype=\"lx_uint32\" value=\"73\"/>"
        "<pPeriod runtype=\"CLxListVariant\">"
        "<_00 runtype=\"RLxExperiment.RLxExpTimeLoop\"><uiCount runtype=\"lx_uint32\" value=\"41\"/>"
        "<dPeriod runtype=\"double\" value=\"300000.0\"/></_00>"
        "<_01 runtype=\"RLxExperiment.RLxExpTimeLoop\"><uiCount runtype=\"lx_uint32\" value=\"11\"/>"
        "<dPeriod runtype=\"double\" value=\"360000.0\"/></_01></pPeriod>"
        "<pPeriodValid runtype=\"CLxListVariant\"><_00 runtype=\"bool\" value=\"true\"/>"
        "<_01 runtype=\"bool\" value=\"false\"/></pPeriodValid>"
        "</no_name></uLoopPars>"
        "<pItemValid runtype=\"CLxListVariant\"></pItemValid>"
        "<ppNextLevelEx runtype=\"CLxListVariant\"><no_name runtype=\"RLxExperiment\">"
        "<eType runtype=\"lx_uint32\" value=\"2\"/>"
        "<uLoopPars runtype=\"CLxListVariant\"><no_name runtype=\"RLxExperiment.RLxExpXYPosLoop\">"
        "<uiCount runtype=\"lx_uint32\" value=\"4\"/>"
        "<dPosX runtype=\"CLxListVariant\"><item_00000 runtype=\"double\" value=\"1\"/>"
        "<item_00001 runtype=\"double\" value=\"2\"/><item_00002 runtype=\"double\" value=\"3\"/>"
        "<item_00003 runtype=\"double\" value=\"4\"/></dPosX>"
        "</no_name></uLoopPars>"
        "<pItemValid runtype=\"CLxListVariant\"></pItemValid>"
        "<ppNextLevelEx runtype=\"CLxListVariant\"><no_name runtype=\"RLxExperiment\">"
        "<eType runtype=\"lx_uint32\" value=\"4\"/>"
        "<uLoopPars runtype=\"CLxListVariant\"><no_name runtype=\"RLxExperiment.RLxExpZStackLoop\">"
        "<uiCount runtype=\"lx_uint32\" value=\"7\"/><dZStep runtype=\"double\" value=\"3.0\"/>"
        "</no_name></uLoopPars>"
        "<pItemValid runtype=\"CLxListVariant\"></pItemValid>"
        "<ppNextLevelEx runtype=\"CLxListVariant\"></ppNextLevelEx>"
        "</no_name></ppNextLevelEx>"
        "</no_name></ppNextLevelEx>"
        "</no_name></variant>";

    Variant decodeText(const std::string& text)
    {
        return decodeMetadata(std::vector<uint8_t>(text.begin(), text.end()));
    }
}

TEST(Nd2Experiment, XmlListWrappedLoopPars)
{
    // A one-element-list uLoopPars is the loop's parameters, not an empty record.
    const Variant tree = decodeText(kOldCellsShape);
    const Experiment e = Experiment::parse(&tree);
    EXPECT_EQ("T41,P4,Z7", describe(e));
    const Loop* z = e.loop(Axis::Z);
    ASSERT_NE(nullptr, z);
    ASSERT_NE(nullptr, z->pars.get("dZStep"));
    EXPECT_DOUBLE_EQ(3.0, z->pars.get("dZStep")->asDouble());
}

TEST(Nd2Experiment, EmptyItemValidCountsEveryPoint)
{
    // old_cells_02: an XY loop of 4 points with an empty pItemValid list has 4 points.
    const Variant tree = decodeText(kOldCellsShape);
    const Experiment e = Experiment::parse(&tree);
    const Loop* xy = e.loop(Axis::P);
    ASSERT_NE(nullptr, xy);
    EXPECT_EQ(4, xy->count);
    EXPECT_EQ((std::vector<int64_t>{ 0, 1, 2, 3 }), xy->validItems);

    // A non-empty list still filters (FD 7.3).
    std::string flagged = kOldCellsShape;
    const std::string empty = "<pItemValid runtype=\"CLxListVariant\"></pItemValid><ppNextLevelEx runtype=\"CLxListVariant\"><no_name runtype=\"RLxExperiment\"><eType runtype=\"lx_uint32\" value=\"4\"/>";
    const size_t at = flagged.find(empty);
    ASSERT_NE(std::string::npos, at);
    flagged.replace(at, std::string("<pItemValid runtype=\"CLxListVariant\"></pItemValid>").size(),
        "<pItemValid runtype=\"CLxListVariant\"><_00 runtype=\"bool\" value=\"false\"/>"
        "<_01 runtype=\"bool\" value=\"true\"/><_02 runtype=\"bool\" value=\"false\"/>"
        "<_03 runtype=\"bool\" value=\"true\"/></pItemValid>");
    const Variant flaggedTree = decodeText(flagged);
    const Experiment f = Experiment::parse(&flaggedTree);
    const Loop* filtered = f.loop(Axis::P);
    ASSERT_NE(nullptr, filtered);
    EXPECT_EQ(2, filtered->count);
    EXPECT_EQ((std::vector<int64_t>{ 1, 3 }), filtered->validItems);
}
