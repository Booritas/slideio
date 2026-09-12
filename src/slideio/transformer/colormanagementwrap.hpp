// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "slideio/transformer/transformer_def.hpp"
#include "slideio/transformer/transformationwrapper.hpp"
#include "slideio/core/colorprofile.hpp"
#include <memory>

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4275 4251)
#endif


namespace slideio
{
    class ColorManagement;
    // MissingProfilePolicy is fully defined by the colorprofile.hpp include
    // above; no forward declaration needed here.
    enum class TransformationType;
    class SLIDEIO_TRANSFORMER_EXPORTS ColorManagementWrap : public TransformationWrapper
    {
    public:
        ColorManagementWrap(const ColorManagementWrap& other)
            : TransformationWrapper(other),
              m_filter(other.m_filter) {
        }

        ColorManagementWrap(ColorManagementWrap&& other) noexcept
            : TransformationWrapper(std::move(other)),
              m_filter(std::move(other.m_filter)) {
        }

        ColorManagementWrap& operator=(const ColorManagementWrap& other) {
            if (this == &other)
                return *this;
            TransformationWrapper::operator =(other);
            m_filter = other.m_filter;
            return *this;
        }

        ColorManagementWrap& operator=(ColorManagementWrap&& other) noexcept {
            if (this == &other)
                return *this;
            TransformationWrapper::operator =(std::move(other));
            m_filter = std::move(other.m_filter);
            return *this;
        }

        ColorManagementWrap();
        ColorManagementWrap(const ColorManagement& filter);
        ColorTarget getTarget() const;
        void setTarget(ColorTarget target);
        RenderingIntent getIntent() const;
        void setIntent(RenderingIntent intent);
        bool getBlackPointCompensation() const;
        void setBlackPointCompensation(bool value);
        MissingProfilePolicy getMissingProfilePolicy() const;
        void setMissingProfilePolicy(MissingProfilePolicy policy);
        const ColorProfile& getSourceProfileOverride() const;
        void setSourceProfileOverride(const ColorProfile& profile);
        TransformationType getType() const override;
        std::shared_ptr<ColorManagement> getFilter() const;
    private:
        std::shared_ptr<ColorManagement> m_filter;
    };
}

#if defined(_MSC_VER)
#pragma warning( pop )
#endif
