// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include <memory>
#include "slideio/transformer/transformer_def.hpp"
#include "slideio/transformer/transformationex.hpp"
#include "slideio/transformer/transformationtype.hpp"
#include "slideio/core/colorprofile.hpp"

namespace slideio
{
    class IccTransform;

    // MissingProfilePolicy now lives in slideio/core/colorprofile.hpp, alongside
    // the rest of the public colour vocabulary -- see that header for why.

    /**@brief converts scene blocks into a device-independent colour space.
     *
     * Binds only to colorimetric RGB scenes: three channels of DT_Byte or
     * DT_UInt16, and an embedded profile whose data space is RGB. Anything else
     * throws at bind time rather than producing numbers that cannot mean
     * anything -- fluorescence channel intensities are not colorimetric.*/
    class SLIDEIO_TRANSFORMER_EXPORTS ColorManagement : public TransformationEx
    {
    public:
        ColorManagement();
        explicit ColorManagement(ColorTarget target);
        ColorManagement(const ColorManagement& other) = default;
        ColorManagement& operator=(const ColorManagement& other) = default;

        ColorTarget getTarget() const { return m_target; }
        void setTarget(ColorTarget target) { m_target = target; }
        RenderingIntent getIntent() const { return m_intent; }
        void setIntent(RenderingIntent intent) { m_intent = intent; }
        bool getBlackPointCompensation() const { return m_blackPointCompensation; }
        void setBlackPointCompensation(bool value) { m_blackPointCompensation = value; }
        MissingProfilePolicy getMissingProfilePolicy() const { return m_policy; }
        void setMissingProfilePolicy(MissingProfilePolicy policy) { m_policy = policy; }
        const ColorProfile& getSourceProfileOverride() const { return m_sourceOverride; }
        void setSourceProfileOverride(const ColorProfile& profile) { m_sourceOverride = profile; }

        std::shared_ptr<TransformationEx> bindToSource(const CVScene& source) const override;
        ColorProfile amendColorProfile(const ColorProfile& input) const override;
        void applyTransformation(const cv::Mat& block, cv::OutputArray transformedBlock) const override;
        std::vector<DataType> computeChannelDataTypes(const std::vector<DataType>& channels) const override;
    private:
        ColorTarget m_target = ColorTarget::sRGB;
        RenderingIntent m_intent = RenderingIntent::RelativeColorimetric;
        bool m_blackPointCompensation = true;
        MissingProfilePolicy m_policy = MissingProfilePolicy::AssumeSRGB;
        ColorProfile m_sourceOverride;
        // Set only on a bound copy. Shared rather than unique so the class stays
        // copyable, and immutable after binding so apply() is const and re-entrant.
        std::shared_ptr<const IccTransform> m_transform;
        ColorProfile m_boundSource;
        bool m_passThrough = false;
    };
}
