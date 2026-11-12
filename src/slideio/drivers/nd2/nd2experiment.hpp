// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

#include "slideio/drivers/nd2/nd2_api_def.hpp"
#include "slideio/drivers/nd2/nd2attributes.hpp"
#include "slideio/drivers/nd2/nd2variant.hpp"
#include <cstdint>
#include <set>
#include <vector>

namespace slideio
{
    namespace nd2
    {
        enum class Axis { T, P, Z };

        /// One frame dimension of the flattened experiment. Self-contained: it owns a
        /// copy of the loop parameters, so an Experiment may outlive the metadata tree.
        struct SLIDEIO_ND2_EXPORTS Loop
        {
            Axis axis = Axis::T;
            /// Items along this axis. For P, the number of valid points.
            int64_t count = 1;
            /// `uLoopPars` (the `i0000000000` wrapper removed): `dZStep`, `dPeriod`,
            /// `Points`, `pPeriod` and so on.
            Variant pars;
            /// P only: index in the XY loop of each valid point, in order. The point
            /// with ordinal p (the p of Experiment::frameIndex) is validItems[p].
            std::vector<int64_t> validItems;
        };

        /// The experiment loop tree flattened to frame dimensions (FD 7, design 4.4).
        class SLIDEIO_ND2_EXPORTS Experiment
        {
        public:
            /// nullptr or a tree without loops gives no loops. Throws Unsupported,
            /// listing the loops, for a custom loop with more than one item or for
            /// more than one loop of an axis.
            static Experiment parse(const Variant* experiment);
            /// Builds an experiment from loops already flattened, outermost first, applying the
            /// checks parse() applies: at most one loop per axis and a loop product that fits
            /// in int64. Throws Unsupported, listing the loops, otherwise.
            static Experiment fromLoops(std::vector<Loop> loops);
            /// Outermost first.
            const std::vector<Loop>& loops() const { return m_loops; }
            /// Items along the axis; 1 when there is no such loop.
            int64_t count(Axis a) const;
            /// Product of the loop counts; 1 without loops.
            int64_t loopProduct() const;
            /// Row-major frame number, innermost loop fastest. `p` is the ordinal among
            /// the valid XY points. Each index must be in [0, count(axis)); an axis
            /// without a loop only accepts 0. Throws RuntimeError otherwise.
            int64_t frameIndex(int64_t t, int64_t p, int64_t z) const;
            /// Design 4.5 (R25, R29, R31), only when the time loop is the outermost:
            /// - trim: T ends at the last declared time point holding a frame;
            /// - extend: when the last declared time point holds a frame, surplus time points are
            ///   added one by one while each is complete (all innerProduct frames present)
            ///   and its known times (`frameTimesMs`, by frame, NaN = unknown) come after
            ///   the previous time point's; with times unknown, completeness alone
            ///   decides. The first time point failing either test stops it, and frames
            ///   at or past loopProduct() + presentFrames.size() never count.
            /// Does nothing when T is not the outermost loop or no frame lies inside it.
            void fitTime(const std::set<int64_t>& presentFrames, const std::vector<double>& frameTimesMs = {});
            /// nullptr when there is no loop on this axis.
            const Loop* loop(Axis a) const;

        private:
            std::vector<Loop> m_loops;
        };
    }
}
