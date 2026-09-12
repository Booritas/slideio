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

std::shared_ptr<TransformationEx> ColorManagement::bindToSource(const CVScene& source) const
{
    auto bound = std::make_shared<ColorManagement>(*this);

    if (source.getNumChannels() != 3) {
        RAISE_RUNTIME_ERROR << "ColorManagement: expected a 3 channel RGB scene, found "
                            << source.getNumChannels()
                            << " channels. ICC conversion of non-colorimetric channels is"
                               " not meaningful.";
    }
    const DataType dataType = source.getChannelDataType(0);
    if (dataType != DataType::DT_Byte && dataType != DataType::DT_UInt16) {
        RAISE_RUNTIME_ERROR << "ColorManagement: expected DT_Byte or DT_UInt16 channels, found "
                            << dataType;
    }

    ColorProfile sourceProfile = m_sourceOverride.isEmpty() ? source.getColorProfile()
                                                            : m_sourceOverride;
    if (!sourceProfile.isEmpty()) {
        const ColorProfileInfo info = IccTransform::describe(sourceProfile);
        if (!info.present) {
            // Corrupt bytes are treated as absence; the policy below decides.
            sourceProfile = ColorProfile();
        }
        else if (info.dataSpace != IccColorSpace::RGB) {
            RAISE_RUNTIME_ERROR << "ColorManagement: the embedded profile describes "
                                << info.dataSpace << " data, not RGB";
        }
    }

    if (sourceProfile.isEmpty()) {
        switch (m_policy) {
        case MissingProfilePolicy::Fail:
            RAISE_RUNTIME_ERROR << "ColorManagement: the scene embeds no ICC profile and the"
                                   " missing profile policy is Fail";
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
