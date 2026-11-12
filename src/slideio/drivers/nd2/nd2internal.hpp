// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

// Internal to the ND2 driver: not listed among the public headers, not installed.
#include "slideio/drivers/nd2/nd2experiment.hpp"
#include "slideio/drivers/nd2/nd2variant.hpp"
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace slideio
{
    namespace nd2
    {
        namespace detail
        {
            /// The elements of a list, or the members of a map whose names are not indices.
            inline std::vector<const Variant*> elementsOf(const Variant* v)
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

            /// The loop tree is a chain in practice; a deeper one is a damaged file. Also
            /// bounds the walks over wrapper levels.
            constexpr int kMaxLoopDepth = 64;

            /// Experiment loop types (FD 7.1).
            constexpr int kEtTime = 1;
            constexpr int kEtXY = 2;
            constexpr int kEtZ = 4;
            constexpr int kEtSpectral = 6;
            constexpr int kEtCustom = 7;
            constexpr int kEtNeTime = 8;

            /// `name` without its lowercase type prefix (`^[a-z_]+`): `uiCount` -> `Count`,
            /// `m_sName` -> `Name`. A name that is all prefix is returned unchanged.
            std::string unprefixed(const std::string& name);

            /// Member `name` (a chunked, prefixed name such as `uiCount`) of a map, or, when
            /// there is none, the member spelled without the prefix (`Count`), which is how
            /// the nd2 package and the legacy fixtures name it. nullptr when neither exists.
            const Variant* fieldOf(const Variant* record, const std::string& name);

            /// `uLoopPars` and period records can arrive wrapped in an `i0000000000` map (2.x).
            /// Returns the record inside.
            const Variant& unwrapPars(const Variant& pars);

            /// A validity flag list: a byte array, a list or map of flags, or a lone flag.
            /// `present` is false only when `v` is nullptr; a present but empty list
            /// validates nothing.
            std::vector<bool> flagsOf(const Variant* v, bool& present);

            /// Item `i` of a flag list is valid: always when there is no list.
            bool flagSet(const std::vector<bool>& flags, bool present, size_t i);

            /// NE time loop count (FD 7.1): the sum of `uiCount` over the `pPeriod` entries
            /// whose `pPeriodValid` flag is set (all of them without flags).
            int64_t countPeriods(const Variant& pars);

            /// XY loop (FD 7.3): the indices among `n` points whose flag in `itemValid` is
            /// set; every index when `itemValid` is nullptr.
            std::vector<int64_t> validPoints(int64_t n, const Variant* itemValid);

            /// A loop record before it is checked against the supported shapes.
            struct RawLoop
            {
                int type = 0;
                int level = 0;
                int64_t count = 0;
                Variant pars;
                std::vector<int64_t> validItems;
            };

            /// One loop record as its schema presents it: `type`, the unwrapped parameters
            /// with the loop's count (and valid XY items), and the child records.
            struct LoopRecord
            {
                int type = 0;
                bool hasPars = false;
                RawLoop loop;
                std::vector<const Variant*> children;
            };
            /// Reads a record of one schema (chunked `ImageMetadata`, legacy `AIM1`).
            using LoopRecordReader = std::function<LoopRecord(const Variant&)>;

            /// The count of a loop of `type` with parameters `pars` (FD 7.1): NE time sums
            /// the valid periods, spectral reads `pPlanes.uiCount` then `uiCount`, XY counts
            /// the valid points of `points` (else of `uiCount`) under `itemValid`, the
            /// others read `uiCount`. Sets `loop.count` and, for XY, `loop.validItems`.
            void countLoop(int type, const Variant* points, const Variant* itemValid, RawLoop& loop);

            /// FD 7.2 rules 1-4 over a loop tree whose records `read` decodes: loops without
            /// parameters or items are ignored; a spectral loop is skipped and its children
            /// stand at its level; a loop is appended below a shallower one, or replaces a
            /// same-level, same-type loop with a smaller count. Throws Unsupported for a tree
            /// deeper than kMaxLoopDepth.
            void flattenLoops(const Variant& node, const LoopRecordReader& read, int level,
                std::vector<RawLoop>& out);

            /// Raw loops to frame dimensions: T for time and NE time, P for XY, Z for Z; any
            /// other type is dropped when it has one item and refused (Unsupported, listing
            /// the loops) otherwise.
            std::vector<Loop> axisLoops(std::vector<RawLoop> raw);

            /// Seconds between time points: `dPeriod` of a time loop, or of the first valid
            /// period of an NE time loop; 0 when not positive.
            double periodOf(const Loop& time);

            /// The ND2 channel count of a picture metadata record: `sPicturePlanes.uiCount`,
            /// else the number of plane records; 0 when there are neither.
            int64_t pictureChannelCount(const Variant* pictureMetadata);
        }
    }
}
