// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include <vector>
#include <opencv2/core/mat.hpp>

#include "slideio/transformer/transformer_def.hpp"
#include "slideio/transformer/transformation.hpp"


namespace slideio
{
    enum class DataType;
    enum class TransformationType;
    class SLIDEIO_TRANSFORMER_EXPORTS TransformationEx : public Transformation
    {
    public:
        TransformationEx();
        virtual ~TransformationEx() = default;
        TransformationType getType() const override {
            return m_type;
        }
        virtual std::vector<DataType> computeChannelDataTypes(const std::vector<DataType>& channels) const;
        virtual int getInflationValue() const;
        virtual void applyTransformation(const cv::Mat& block, cv::OutputArray transformedBlock) const = 0;
    protected:
        TransformationType m_type;
    };
}
