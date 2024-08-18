// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/transformer/sobelfilterwrap.hpp"
#include "slideio/transformer/sobelfilter.hpp"

using namespace slideio;

SobelFilterWrap::SobelFilterWrap() : m_filter(std::make_shared<SobelFilter>())
{
}

DataType SobelFilterWrap::getDepth() const
{
    return m_filter->getDepth();
}

void SobelFilterWrap::setDepth(const DataType& depth)
{
    m_filter->setDepth(depth);
}

int SobelFilterWrap::getDx() const
{
    return m_filter->getDx();
}

void SobelFilterWrap::setDx(int dx)
{
    m_filter->setDx(dx);
}

int SobelFilterWrap::getDy() const
{
    return m_filter->getDy();
}

void SobelFilterWrap::setDy(int dy)
{
    m_filter->setDy(dy);
}

int SobelFilterWrap::getKernelSize() const
{
    return m_filter->getKernelSize();
}

void SobelFilterWrap::setKernelSize(int ksize)
{
    m_filter->setKernelSize(ksize);
}

double SobelFilterWrap::getScale() const
{
    return m_filter->getScale();
}

void SobelFilterWrap::setScale(double scale)
{
    m_filter->setScale(scale);
}

double SobelFilterWrap::getDelta() const
{
    return m_filter->getDelta();
}

void SobelFilterWrap::setDelta(double delta)
{
    m_filter->setDelta(delta);
}

TransformationType SobelFilterWrap::getType() const
{
    return m_filter->getType();
}

