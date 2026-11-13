// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/nd2/nd2experiment.hpp"
#include "slideio/drivers/nd2/nd2internal.hpp"
#include "slideio/core/exceptions.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>

using namespace slideio;
using namespace slideio::nd2;
using slideio::nd2::detail::elementsOf;
using slideio::nd2::detail::RawLoop;

namespace
{
    using namespace slideio::nd2::detail;

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

    void flattenImpl(const Variant& node, const LoopRecordReader& read, int level, int depth,
        std::vector<RawLoop>& out)
    {
        if (depth > kMaxLoopDepth) {
            throw Unsupported("ND2: unsupported experiment: loop tree deeper than " + std::to_string(kMaxLoopDepth));
        }
        LoopRecord record = read(node);
        int childLevel = level + 1;
        if (record.type == kEtSpectral) {
            // Rule 2: not a frame dimension; its children stand at its own level.
            childLevel = level;
        }
        else if (record.hasPars && record.loop.count > 0) {
            RawLoop& loop = record.loop;
            loop.type = record.type;
            loop.level = level;
            if (out.empty() || out.back().level < level) {
                out.push_back(std::move(loop));
            }
            else if (out.back().level == level && out.back().type == record.type) {
                if (out.back().count < loop.count) {
                    out.back() = std::move(loop);
                }
            }
            else {
                // A branched tree (PR review): a loop of another type at the same level, or
                // one at a shallower level after a deeper one. [ND2] drops it silently, and
                // then the frame order no longer matches the loops. No corpus file has one.
                out.push_back(std::move(loop));
                refuse("the loop tree branches", out);
            }
        }
        for (const Variant* child : record.children) {
            flattenImpl(*child, read, childLevel, depth + 1, out);
        }
    }

    /// The chunked schema (`ImageMetadata[LV]!`, FD 7.1): `eType`, `uLoopPars`,
    /// `pItemValid`, `ppNextLevelEx`; XY points in `uLoopPars.Points`.
    LoopRecord readChunkedRecord(const Variant& node, int64_t maxFrames)
    {
        LoopRecord r;
        const Variant* typeField = node.get("eType");
        r.type = typeField ? static_cast<int>(typeField->asInt(0)) : 0;
        if (const Variant* pars = node.get("uLoopPars")) {
            r.hasPars = true;
            r.loop.pars = unwrapPars(*pars);
            countLoop(r.type, r.loop.pars.get("Points"), node.get("pItemValid"), r.loop, maxFrames);
        }
        r.children = elementsOf(node.get("ppNextLevelEx"));
        return r;
    }
}

// ------------------------------------------------------------- shared loop rules

std::string slideio::nd2::detail::unprefixed(const std::string& name)
{
    size_t i = 0;
    while (i < name.size() && ((name[i] >= 'a' && name[i] <= 'z') || name[i] == '_')) {
        ++i;
    }
    return i == name.size() ? name : name.substr(i);
}

const Variant* slideio::nd2::detail::fieldOf(const Variant* record, const std::string& name)
{
    if (!record) {
        return nullptr;
    }
    if (const Variant* v = record->get(name)) {
        return v;
    }
    const std::string bare = unprefixed(name);
    return bare == name ? nullptr : record->get(bare);
}

const Variant& slideio::nd2::detail::unwrapPars(const Variant& pars)
{
    if (pars.kind() == Variant::Kind::Map && pars.get("i0000000000") && !pars.items().empty()) {
        return pars.items().front();
    }
    // CLX XML (2.x chunked and legacy) wraps the record in one `no_name`, which decodes as
    // a one-element list.
    if (pars.kind() == Variant::Kind::List && pars.items().size() == 1
        && pars.items().front().kind() == Variant::Kind::Map) {
        return pars.items().front();
    }
    return pars;
}

std::vector<bool> slideio::nd2::detail::flagsOf(const Variant* v, bool& present)
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

bool slideio::nd2::detail::flagSet(const std::vector<bool>& flags, bool present, size_t i)
{
    return !present || (i < flags.size() && flags[i]);
}

int64_t slideio::nd2::detail::countPeriods(const Variant& pars)
{
    bool hasFlags = false;
    const std::vector<bool> flags = flagsOf(fieldOf(&pars, "pPeriodValid"), hasFlags);
    int64_t total = 0;
    const std::vector<const Variant*> periods = elementsOf(fieldOf(&pars, "pPeriod"));
    for (size_t i = 0; i < periods.size(); ++i) {
        if (flagSet(flags, hasFlags, i)) {
            const Variant* n = fieldOf(&unwrapPars(*periods[i]), "uiCount");
            const int64_t count = n ? n->asInt(0) : 0;
            if (count <= 0) {
                continue;   // a negative count is damage, not a period
            }
            if (count > std::numeric_limits<int64_t>::max() - total) {
                throw Unsupported("ND2: unsupported experiment (the NE time periods' uiCount values sum past "
                    + std::to_string(std::numeric_limits<int64_t>::max()) + ")");
            }
            total += count;
        }
    }
    return total;
}

std::vector<int64_t> slideio::nd2::detail::validPoints(int64_t n, const Variant* itemValid, int64_t maxPoints)
{
    // Refused before anything is allocated: the loops of the file could not hold them.
    if (n > maxPoints) {
        throw Unsupported("ND2: unsupported experiment (the XY loop declares " + std::to_string(n)
            + " points, more than the " + std::to_string(maxPoints) + " frames the file can justify)");
    }
    bool hasFlags = false;
    const std::vector<bool> flags = flagsOf(itemValid, hasFlags);
    // An empty flag list (`<pItemValid runtype="CLxListVariant"/>` in 2.x files such as
    // old_cells_02.nd2) says nothing: every point counts, as without one.
    hasFlags = hasFlags && !flags.empty();
    std::vector<int64_t> valid;
    for (int64_t i = 0; i < n; ++i) {
        if (flagSet(flags, hasFlags, static_cast<size_t>(i))) {
            valid.push_back(i);
        }
    }
    return valid;
}

void slideio::nd2::detail::countLoop(int type, const Variant* points, const Variant* itemValid, RawLoop& loop,
    int64_t maxFrames)
{
    auto uiCount = [](const Variant* pars) -> int64_t {
        const Variant* v = fieldOf(pars, "uiCount");
        return v ? v->asInt(0) : 0;
    };
    switch (type) {
    case kEtXY: {
        // The point list rules over the flag list, which can be longer (FD 7.3).
        int64_t n = points ? static_cast<int64_t>(elementsOf(points).size()) : 0;
        if (n == 0) {
            n = uiCount(&loop.pars);
        }
        loop.validItems = validPoints(n, itemValid, maxFrames);
        loop.count = static_cast<int64_t>(loop.validItems.size());
        break;
    }
    case kEtNeTime:
        loop.count = countPeriods(loop.pars);
        break;
    case kEtSpectral: {
        const Variant* planes = fieldOf(&loop.pars, "pPlanes");
        loop.count = planes ? uiCount(planes) : 0;
        if (loop.count == 0) {
            loop.count = uiCount(&loop.pars);
        }
        break;
    }
    default:
        loop.count = uiCount(&loop.pars);
        break;
    }
}

void slideio::nd2::detail::flattenLoops(const Variant& node, const LoopRecordReader& read, int level,
    std::vector<RawLoop>& out)
{
    flattenImpl(node, read, level, 0, out);
}

std::vector<Loop> slideio::nd2::detail::axisLoops(std::vector<RawLoop> raw)
{
    std::vector<Loop> loops;
    for (RawLoop& l : raw) {
        Axis axis;
        if (!axisOf(l.type, axis)) {
            if (l.count > 1) {
                refuse(l.type == kEtCustom ? "custom loop with more than one item" :
                    "loop type " + std::to_string(l.type) + " is not supported", raw);
            }
            continue;   // one item: it adds no dimension
        }
        Loop loop;
        loop.axis = axis;
        loop.count = l.count;
        loop.pars = std::move(l.pars);
        loop.validItems = std::move(l.validItems);
        loops.push_back(std::move(loop));
    }
    return loops;
}

// ------------------------------------------------------------------ Experiment

int64_t slideio::nd2::maxDeclaredFrames(int64_t presentFrames)
{
    constexpr int64_t kPerPresentFrame = 1024;
    const int64_t present = std::max<int64_t>(presentFrames, 0);
    if (present >= kMaxDeclaredFrames / kPerPresentFrame) {
        return kMaxDeclaredFrames;
    }
    return (present + 1) * kPerPresentFrame;
}

Experiment Experiment::parse(const Variant* experiment, int64_t maxFrames)
{
    if (!experiment) {
        return Experiment();
    }
    std::vector<RawLoop> raw;
    flattenLoops(*experiment,
        [maxFrames](const Variant& node) { return readChunkedRecord(node, maxFrames); }, 0, raw);
    return fromLoops(axisLoops(std::move(raw)), maxFrames);
}

Experiment Experiment::fromLoops(std::vector<Loop> loops, int64_t maxFrames)
{
    auto describe = [&loops]() {
        std::string s;
        for (const Loop& l : loops) {
            if (!s.empty()) {
                s += ", ";
            }
            s += l.axis == Axis::T ? "T" : l.axis == Axis::P ? "P" : "Z";
            s += std::to_string(l.count);
        }
        return s;
    };
    int perAxis[3] = { 0, 0, 0 };
    int64_t product = 1;
    for (const Loop& l : loops) {
        if (++perAxis[static_cast<int>(l.axis)] > 1) {
            throw Unsupported(std::string("ND2: unsupported experiment (") +
                (l.axis == Axis::T ? "more than one time loop" :
                 l.axis == Axis::P ? "more than one XY loop" : "more than one Z loop") + "): " + describe());
        }
        if (l.count < 1 || l.count > std::numeric_limits<int64_t>::max() / product) {
            throw Unsupported("ND2: unsupported experiment (" +
                std::string(l.count < 1 ? "a loop without items" : "too many frames") + "): " + describe());
        }
        product *= l.count;
    }
    // Design 4.5: loops the file cannot back are refused before anything is sized by them.
    if (product > maxFrames) {
        throw Unsupported("ND2: unsupported experiment (the loops declare " + std::to_string(product)
            + " frames, more than the " + std::to_string(maxFrames) + " the file can justify): " + describe());
    }
    Experiment result;
    result.m_loops = std::move(loops);
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

namespace
{
    // Earliest and latest known (not NaN) time of frames [first, first + n); false when
    // none is known.
    bool timeSpan(const FrameTimes& times, int64_t first, int64_t n, double& lo, double& hi)
    {
        bool any = false;
        for (int64_t f = first; f < first + n; ++f) {
            const double v = times[f];
            if (std::isnan(v)) {
                continue;
            }
            lo = any ? std::min(lo, v) : v;
            hi = any ? std::max(hi, v) : v;
            any = true;
        }
        return any;
    }
}

void Experiment::fitTime(const std::set<int64_t>& presentFrames, const FrameTimes& frameTimesMs)
{
    if (m_loops.empty() || m_loops.front().axis != Axis::T || presentFrames.empty()) {
        return;
    }
    Loop& time = m_loops.front();
    const int64_t product = loopProduct();
    const int64_t inner = product / time.count;
    const int64_t declared = time.count;

    // Trim: the last declared time point with a frame ends T.
    int64_t maxInside = -1;
    for (auto it = presentFrames.rbegin(); it != presentFrames.rend(); ++it) {
        if (*it >= 0 && *it < product) {
            maxInside = *it;
            break;
        }
    }
    if (maxInside < 0) {
        return;
    }
    if (maxInside / inner + 1 < declared) {
        time.count = maxInside / inner + 1;
        return;
    }

    // Extend (R29): add surplus time points while each is complete -- every inner frame
    // present -- and its known times follow the previous time point's. Frames at or past
    // product + presentFrames.size() are never acquired ones (R31). Unknown (NaN) times
    // leave completeness to decide alone.
    const int64_t bound = product + static_cast<int64_t>(presentFrames.size());
    double prevLo = 0, prevHi = 0;
    bool prevKnown = timeSpan(frameTimesMs, (declared - 1) * inner, inner, prevLo, prevHi);
    int64_t points = declared;
    for (;;) {
        const int64_t first = points * inner;
        if (first + inner > bound) {
            break;
        }
        bool complete = true;
        for (int64_t f = first; f < first + inner && complete; ++f) {
            complete = presentFrames.count(f) != 0;
        }
        if (!complete) {
            break;
        }
        double lo = 0, hi = 0;
        const bool known = timeSpan(frameTimesMs, first, inner, lo, hi);
        if (known && prevKnown && lo <= prevHi) {
            break;
        }
        if (known) {
            prevHi = hi;
            prevKnown = true;
        }
        ++points;
    }
    time.count = points;
}
