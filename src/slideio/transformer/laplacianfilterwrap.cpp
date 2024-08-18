// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.

#include "slideio/transformer/laplacianfilter.hpp"
#include "slideio/transformer/laplacianfilterwrap.hpp"
using namespace slideio;

LaplacianFilterWrap::LaplacianFilterWrap() : m_filter(std::make_shared<LaplacianFilter>())
{
}

DataType LaplacianFilterWrap::getDepth() const
{
    return m_filter->getDepth();
}

void LaplacianFilterWrap::setDepth(const DataType& depth)
{
    m_filter->setDepth(depth);
}

int LaplacianFilterWrap::getKernelSize() const
{
    return m_filter->getKernelSize();
}

void LaplacianFilterWrap::setKernelSize(int kernelSize)
{
    m_filter->setKernelSize(kernelSize);
}

double LaplacianFilterWrap::getScale() const
{
    return m_filter->getScale();
}

void LaplacianFilterWrap::setScale(double scale)
{
    m_filter->setScale(scale);
}

double LaplacianFilterWrap::getDelta() const
{
    return m_filter->getDelta();
}

void LaplacianFilterWrap::setDelta(double delta)
{
    m_filter->setDelta(delta);
}

TransformationType LaplacianFilterWrap::getType() const
{
    return m_filter->getType();
}

