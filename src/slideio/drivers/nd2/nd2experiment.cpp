// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/nd2/nd2experiment.hpp"
#include "slideio/core/exceptions.hpp"

#include <limits>
#include <string>

using namespace slideio;
using namespace slideio::nd2;

namespace
{
    constexpr int kEtTime = 1;
    constexpr int kEtXY = 2;
    constexpr int kEtZ = 4;
    constexpr int kEtSpectral = 6;
    constexpr int kEtCustom = 7;
    constexpr int kEtNeTime = 8;
    /// The tree is a chain in practice; a deeper one is a damaged file.
    constexpr int kMaxDepth = 64;

    /// A loop record before it is checked against the supported shapes.
    struct RawLoop
    {
        int type = 0;
        int level = 0;
        int64_t count = 0;
        Variant pars;
        std::vector<int64_t> validItems;
    };

    /// The elements of a list, or the members of a map whose names are not indices.
    std::vector<const Variant*> elementsOf(const Variant* v)
    {
        std::vector<const Variant*> out;
        if (!v) {
            return out;
        }
        if (!v->items().empty()) {
            for (const Variant& item : v->items()) {
                out.push_back(&item);
            }
        }
        else {
            for (const auto& member : v->members()) {
                out.push_back(&member.second);
            }
        }
        return out;
    }

    /// A validity flag list: a byte array, a list of numbers, or a lone flag.
    /// `present` is false when the field is absent.
    std::vector<bool> flagsOf(const Variant* v, bool& present)
    {
        std::vector<bool> flags;
        present = v != nullptr;
        if (!v) {
            return flags;
        }
        switch (v->kind()) {
        case Variant::Kind::Bytes:
            for (uint8_t b : v->bytes()) {
                flags.push_back(b != 0);
            }
            break;
        case Variant::Kind::Map:
        case Variant::Kind::List:
            for (const Variant* e : elementsOf(v)) {
                flags.push_back(e->asInt(0) != 0 || e->asBool(false));
            }
            break;
        case Variant::Kind::Null:
            break;
        default:
            flags.push_back(v->asInt(0) != 0 || v->asBool(false));
            break;
        }
        return flags;
    }

    int64_t uiCount(const Variant& pars)
    {
        const Variant* v = pars.get("uiCount");
        return v ? v->asInt(0) : 0;
    }

    /// `uLoopPars` in 2.x can be wrapped in a single `i0000000000`.
    const Variant& unwrapPars(const Variant& pars)
    {
        if (pars.kind() == Variant::Kind::Map && pars.get("i0000000000") && !pars.items().empty()) {
            return pars.items().front();
        }
        return pars;
    }

    /// XY: the points whose `pItemValid` flag is set (FD 7.3). The point list rules
    /// over the flag list, which can be longer. Without flags every point counts.
    void countPoints(RawLoop& loop, const Variant& node)
    {
        const Variant* points = loop.pars.get("Points");
        int64_t n = points ? static_cast<int64_t>(elementsOf(points).size()) : 0;
        if (n == 0) {
            n = uiCount(loop.pars);
        }
        bool hasFlags = false;
        const std::vector<bool> flags = flagsOf(node.get("pItemValid"), hasFlags);
        for (int64_t i = 0; i < n; ++i) {
            const bool valid = hasFlags ? (static_cast<size_t>(i) < flags.size() && flags[static_cast<size_t>(i)]) : true;
            if (valid) {
                loop.validItems.push_back(i);
            }
        }
        loop.count = static_cast<int64_t>(loop.validItems.size());
    }

    /// NE time: the sum of the counts of the periods flagged valid.
    int64_t countPeriods(const Variant& pars)
    {
        bool hasFlags = false;
        const std::vector<bool> flags = flagsOf(pars.get("pPeriodValid"), hasFlags);
        int64_t total = 0;
        const std::vector<const Variant*> periods = elementsOf(pars.get("pPeriod"));
        for (size_t i = 0; i < periods.size(); ++i) {
            const bool valid = hasFlags ? (i < flags.size() && flags[i]) : true;
            if (valid) {
                total += uiCount(unwrapPars(*periods[i]));
            }
        }
        return total;
    }

    bool computeLoop(const Variant& node, int type, RawLoop& loop)
    {
        const Variant* pars = node.get("uLoopPars");
        if (!pars) {
            return false;
        }
        loop.type = type;
        loop.pars = unwrapPars(*pars);
        switch (type) {
        case kEtXY:
            countPoints(loop, node);
            break;
        case kEtNeTime:
            loop.count = countPeriods(loop.pars);
            break;
        case kEtSpectral: {
            const Variant* planes = loop.pars.get("pPlanes");
            loop.count = planes ? uiCount(*planes) : 0;
            if (loop.count == 0) {
                loop.count = uiCount(loop.pars);
            }
            break;
        }
        default:
            loop.count = uiCount(loop.pars);
            break;
        }
        return true;
    }

    /// FD 7.2 rules 1-4, without the `pSubLoops` special case.
    void flatten(const Variant& node, int level, int depth, std::vector<RawLoop>& out)
    {
        if (depth > kMaxDepth) {
            throw Unsupported("ND2: unsupported experiment: loop tree deeper than " + std::to_string(kMaxDepth));
        }
        const Variant* typeField = node.get("eType");
        const int type = typeField ? static_cast<int>(typeField->asInt(0)) : 0;
        RawLoop loop;
        const bool hasPars = computeLoop(node, type, loop);
        int childLevel = level + 1;
        if (type == kEtSpectral) {
            // Rule 2: not a frame dimension; its children stand at its own level.
            childLevel = level;
        }
        else if (hasPars && loop.count > 0) {
            loop.level = level;
            if (out.empty() || out.back().level < level) {
                out.push_back(std::move(loop));
            }
            else if (out.back().level == level && out.back().type == type && out.back().count < loop.count) {
                out.back() = std::move(loop);
            }
        }
        for (const Variant* child : elementsOf(node.get("ppNextLevelEx"))) {
            flatten(*child, childLevel, depth + 1, out);
        }
    }

    std::string label(const RawLoop& l)
    {
        const char* axis = "E";
        switch (l.type) {
        case kEtTime: case kEtNeTime: axis = "T"; break;
        case kEtXY: axis = "P"; break;
        case kEtZ: axis = "Z"; break;
        case kEtCustom: axis = "C"; break;
        default: break;
        }
        std::string s = axis;
        if (l.type != kEtTime && l.type != kEtNeTime && l.type != kEtXY && l.type != kEtZ && l.type != kEtCustom) {
            s += std::to_string(l.type) + ":";
        }
        return s + std::to_string(l.count);
    }

    std::string list(const std::vector<RawLoop>& loops)
    {
        std::string s;
        for (const RawLoop& l : loops) {
            if (!s.empty()) {
                s += ", ";
            }
            s += label(l);
        }
        return s;
    }

    [[noreturn]] void refuse(const std::string& why, const std::vector<RawLoop>& loops)
    {
        throw Unsupported("ND2: unsupported experiment (" + why + "): " + list(loops));
    }

    bool axisOf(int type, Axis& axis)
    {
        switch (type) {
        case kEtTime: case kEtNeTime: axis = Axis::T; return true;
        case kEtXY: axis = Axis::P; return true;
        case kEtZ: axis = Axis::Z; return true;
        default: return false;
        }
    }
}

Experiment Experiment::parse(const Variant* experiment)
{
    Experiment result;
    if (!experiment) {
        return result;
    }
    std::vector<RawLoop> raw;
    flatten(*experiment, 0, 0, raw);

    int perAxis[3] = { 0, 0, 0 };
    for (const RawLoop& l : raw) {
        Axis axis;
        if (!axisOf(l.type, axis)) {
            if (l.count > 1) {
                refuse(l.type == kEtCustom ? "custom loop with more than one item" :
                    "loop type " + std::to_string(l.type) + " is not supported", raw);
            }
            continue;   // one item: it adds no dimension
        }
        if (++perAxis[static_cast<int>(axis)] > 1) {
            refuse(axis == Axis::T ? "more than one time loop" :
                axis == Axis::P ? "more than one XY loop" : "more than one Z loop", raw);
        }
    }

    int64_t product = 1;
    for (RawLoop& l : raw) {
        Axis axis;
        if (!axisOf(l.type, axis)) {
            continue;
        }
        if (l.count > std::numeric_limits<int64_t>::max() / product) {
            refuse("too many frames", raw);
        }
        product *= l.count;
        Loop loop;
        loop.axis = axis;
        loop.count = l.count;
        loop.pars = std::move(l.pars);
        loop.validItems = std::move(l.validItems);
        result.m_loops.push_back(std::move(loop));
    }
    return result;
}

const Loop* Experiment::loop(Axis a) const
{
    for (const Loop& l : m_loops) {
        if (l.axis == a) {
            return &l;
        }
    }
    return nullptr;
}

int64_t Experiment::count(Axis a) const
{
    const Loop* l = loop(a);
    return l ? l->count : 1;
}

int64_t Experiment::loopProduct() const
{
    int64_t product = 1;
    for (const Loop& l : m_loops) {
        product *= l.count;
    }
    return product;
}

int64_t Experiment::frameIndex(int64_t t, int64_t p, int64_t z) const
{
    const int64_t index[3] = { t, p, z };
    const char* const names[3] = { "T", "P", "Z" };
    for (int a = 0; a < 3; ++a) {
        const int64_t n = count(static_cast<Axis>(a));
        if (index[a] < 0 || index[a] >= n) {
            RAISE_RUNTIME_ERROR << "ND2: " << names[a] << " index " << index[a]
                << " is outside [0, " << n << ")";
        }
    }
    int64_t frame = 0;
    for (const Loop& l : m_loops) {
        frame = frame * l.count + index[static_cast<int>(l.axis)];
    }
    return frame;
}

void Experiment::trimTime(const std::set<int64_t>& presentFrames)
{
    if (m_loops.empty() || m_loops.front().axis != Axis::T) {
        return;
    }
    const int64_t product = loopProduct();
    int64_t maxPresent = -1;
    for (auto it = presentFrames.rbegin(); it != presentFrames.rend(); ++it) {
        if (*it >= 0 && *it < product) {
            maxPresent = *it;
            break;
        }
    }
    if (maxPresent < 0) {
        return;
    }
    Loop& time = m_loops.front();
    const int64_t inner = product / time.count;
    const int64_t points = maxPresent / inner + 1;   // ceil((maxPresent + 1) / inner)
    if (points < time.count) {
        time.count = points;
    }
}
