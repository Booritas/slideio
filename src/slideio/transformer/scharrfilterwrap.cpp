// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.

#include "slideio/transformer/scharrfilterwrap.hpp"
#include "slideio/transformer/scharrfilter.hpp"

using namespace slideio;

ScharrFilterWrap::ScharrFilterWrap() : m_filter(std::make_shared<ScharrFilter>())
{
}

DataType ScharrFilterWrap::getDepth() const
{
    return m_filter->getDepth();
}

void ScharrFilterWrap::setDepth(const DataType& depth)
{
    m_filter->setDepth(depth);
}

int ScharrFilterWrap::getDx() const
{
    return m_filter->getDx();
}

void ScharrFilterWrap::setDx(int dx)
{
    m_filter->setDx(dx);
}

int ScharrFilterWrap::getDy() const
{
    return m_filter->getDy();
}

void ScharrFilterWrap::setDy(int dy)
{
    m_filter->setDy(dy);
}

double ScharrFilterWrap::getScale() const
{
    return m_filter->getScale();
}

void ScharrFilterWrap::setScale(double scale)
{
    m_filter->setScale(scale);
}

double ScharrFilterWrap::getDelta() const
{
    return m_filter->getDelta();
}

void ScharrFilterWrap::setDelta(double delta)
{
    m_filter->setDelta(delta);
}

TransformationType ScharrFilterWrap::getType() const
{
    return m_filter->getType();
}

