// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include <vector>
#include <memory>
#include <opencv2/core.hpp>

#include "slideio/transformer/transformer_def.hpp"
#include "slideio/transformer/transformation.hpp"
#include "slideio/core/colorprofile.hpp"


namespace slideio
{
    enum class DataType;
    enum class TransformationType;
    class CVScene;
    class SLIDEIO_TRANSFORMER_EXPORTS TransformationEx : public Transformation
    {
    public:
        TransformationEx(const TransformationEx& other)
            : Transformation(other),
              m_type(other.m_type) {
        }

        TransformationEx(TransformationEx&& other) noexcept
            : Transformation(std::move(other)),
              m_type(other.m_type) {
        }

        TransformationEx& operator=(const TransformationEx& other) {
            if (this == &other)
                return *this;
            Transformation::operator =(other);
            m_type = other.m_type;
            return *this;
        }

        TransformationEx& operator=(TransformationEx&& other) noexcept {
            if (this == &other)
                return *this;
            Transformation::operator =(std::move(other));
            m_type = other.m_type;
            return *this;
        }

        TransformationEx();
        virtual ~TransformationEx() = default;
        TransformationType getType() const override {
            return m_type;
        }
        virtual std::vector<DataType> computeChannelDataTypes(const std::vector<DataType>& channels) const;
        virtual int getInflationValue() const;
        virtual void applyTransformation(const cv::Mat& block, cv::OutputArray transformedBlock) const = 0;

        /**@brief returns a copy of this transformation specialised to a source scene.
         *
         * The default returns nullptr, meaning the transformation needs no
         * binding and is used as-is; it does not touch its argument. A
         * transformation whose behaviour depends on the source image -- colour
         * management on the source ICC profile, stain normalisation on source
         * statistics -- overrides it and returns a new, fully prepared object.
         *
         * A bound copy rather than mutation of this, so one configuration object
         * stays reusable across many scenes instead of becoming last-bind-wins.*/
        virtual std::shared_ptr<TransformationEx> bindToSource(const CVScene& source) const {
            return nullptr;
        }

        /**@brief lets a bound transformation amend the colour profile its scene reports.
         *
         * The default returns the input unchanged. Colour management overrides
         * it to record that it substituted an assumed sRGB profile, so a caller
         * can tell a real correction from an assumed one without knowing which
         * transformation performed it.*/
        virtual ColorProfile amendColorProfile(const ColorProfile& input) const {
            return input;
        }
    protected:
        TransformationType m_type;
    };
}
