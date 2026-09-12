// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/transformer/colormanagement.hpp"
#include "slideio/core/cvscene.hpp"
#include "slideio/core/exceptions.hpp"
#include "slideio/imagetools/icctransform.hpp"

using namespace slideio;

ColorManagement::ColorManagement()
{
    m_type = TransformationType::ColorManagement;
}

ColorManagement::ColorManagement(ColorTarget target) : ColorManagement()
{
    m_target = target;
}

// The origin scene parameter is deliberately unused; see the comment below.
std::shared_ptr<TransformationEx> ColorManagement::bindToSource(
    const CVScene&, const std::vector<DataType>& channelDataTypes,
    const ColorProfile& inputProfile) const
{
    auto bound = std::make_shared<ColorManagement>(*this);

    // channelDataTypes and inputProfile, not the origin scene: in a chain this is the
    // state the earlier transformations hand us, which for anything but the
    // first element differs from the origin scene's. Validating against the
    // origin instead let, say, [ColorTransformation(GRAY), ColorManagement()]
    // bind happily against three origin channels and then throw from inside
    // IccTransform::apply on the first tile -- exactly the several-thousand-
    // tiles-later failure bind-time validation exists to prevent.
    if (channelDataTypes.size() != 3) {
        RAISE_RUNTIME_ERROR << "ColorManagement: expected a 3 channel RGB image, received "
                            << channelDataTypes.size()
                            << " channels. ICC conversion of non-colorimetric channels is"
                               " not meaningful.";
    }
    const DataType dataType = channelDataTypes[0];
    if (dataType != DataType::DT_Byte && dataType != DataType::DT_UInt16) {
        RAISE_RUNTIME_ERROR << "ColorManagement: expected DT_Byte or DT_UInt16 channels, found "
                            << dataType;
    }

    const bool fromOverride = !m_sourceOverride.isEmpty();
    ColorProfile sourceProfile = fromOverride ? m_sourceOverride : inputProfile;
    if (!sourceProfile.isEmpty()) {
        const ColorProfileInfo info = IccTransform::describe(sourceProfile);
        if (!info.present) {
            // Corrupt bytes are treated as absence; the policy below decides.
            sourceProfile = ColorProfile();
        }
        else if (info.dataSpace != IccColorSpace::RGB) {
            RAISE_RUNTIME_ERROR << "ColorManagement: the source profile describes "
                                << info.dataSpace << " data, not RGB";
        }
        else if (fromOverride) {
            // It reached this scene through setSourceProfileOverride rather than out
            // of the file, so its provenance is Supplied whatever the caller stamped
            // on it. Only the valid path is restamped: a corrupt override falls
            // through to the policy above and is reported as Assumed, which is true.
            sourceProfile.setSource(ColorProfileSource::Supplied);
        }
    }

    if (sourceProfile.isEmpty()) {
        switch (m_policy) {
        case MissingProfilePolicy::Fail:
            RAISE_RUNTIME_ERROR << "ColorManagement: the scene embeds no ICC profile and the"
                                   " missing profile policy is Fail";
            break;
        case MissingProfilePolicy::PassThrough:
            if (m_target != ColorTarget::sRGB) {
                RAISE_RUNTIME_ERROR << "ColorManagement: PassThrough is only coherent with the"
                                       " sRGB target; with " << m_target
                                    << " the output data type would depend on whether a file"
                                       " happens to carry a profile";
            }
            bound->m_passThrough = true;
            bound->m_boundSource = ColorProfile();
            return bound;
        case MissingProfilePolicy::AssumeSRGB:
        default:
            sourceProfile = IccTransform::createSRGBProfile();
            break;
        }
    }

    bound->m_boundSource = sourceProfile;
    bound->m_transform = std::make_shared<const IccTransform>(
        sourceProfile, m_target, m_intent, m_blackPointCompensation, dataType);
    return bound;
}

ColorProfile ColorManagement::amendColorProfile(const ColorProfile& input) const
{
    if (m_passThrough || m_boundSource.isEmpty()) {
        return input;
    }
    return m_boundSource;
}

void ColorManagement::applyTransformation(const cv::Mat& block, cv::OutputArray transformedBlock) const
{
    if (m_passThrough || !m_transform) {
        block.copyTo(transformedBlock);
        return;
    }
    m_transform->apply(block, transformedBlock);
}

std::vector<DataType> ColorManagement::computeChannelDataTypes(
    const std::vector<DataType>& channels) const
{
    if (m_target == ColorTarget::sRGB) {
        return channels;
    }
    return std::vector<DataType>(channels.size(), DataType::DT_Float32);
}
