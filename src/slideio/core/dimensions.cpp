// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "dimensions.hpp"
#include "slideio/base/exceptions.hpp"
#include <string>
#include <vector>


using namespace slideio;

Dimensions::Dimensions(const std::vector<std::string>& labels, const std::vector<int>& dimensionSizes, const std::vector<int>& increments) {
	init(labels, dimensionSizes, increments);
}

void Dimensions::init(const std::vector<std::string>& labels, const std::vector<int>& dimensionSizes, const std::vector<int>& increments) {
	m_order = labels;
	m_sizes = dimensionSizes;
	m_increments = increments;
	if (m_order.size() != m_sizes.size()) {
		RAISE_RUNTIME_ERROR << "Dimensions: dimension label vector size: "
			<< m_order.size() << " does not match dimensions size vector: " << m_sizes.size();
	}
	m_dimensionMap.clear();
	for (size_t i = 0; i < m_order.size(); ++i) {
		m_dimensionMap[m_order[i]] = static_cast<int>(i);
	}
}

bool Dimensions::incrementCoordinates(std::vector<int>& coordinates) const {
	if (coordinates.size() != m_sizes.size()) {
		RAISE_RUNTIME_ERROR << "Dimensions::incrementCoordinates: coordinates size: "
	        << coordinates.size() << " does not match dimensions size: " << m_sizes.size();
	}
    for (size_t i = 0; i < m_sizes.size(); ++i) {
        coordinates[i] += m_increments[i];
        if (coordinates[i] < m_sizes[i]) {
            break;
        }
		else if (i == m_sizes.size() - 1) {
			return false; // all dimensions are at their max size
		}
        coordinates[i] = 0;
    }
    return true;
}

std::vector<int> Dimensions::createCoordinates(const std::list<std::pair<std::string, int>>& coordList) const {
	if (coordList.size() != m_sizes.size()) {
		RAISE_RUNTIME_ERROR << "Dimensions::createCoordinates: coordinate list size: "
			<< coordList.size() << " does not match dimensions size: " << m_sizes.size();
	}
    std::vector<int> coordinates(m_sizes.size(), 0);
    for (const auto& dim : coordList) {
        auto it = m_dimensionMap.find(dim.first);
        if (it == m_dimensionMap.end()) {
			RAISE_RUNTIME_ERROR << "Dimensions::createCoordinates: dimension: " << dim.first
				<< " not found in dimension map";
        }
        coordinates[it->second] = dim.second;
    }
    return coordinates;
}

int Dimensions::getDimensionSize(const std::string& label) const {
    auto it = m_dimensionMap.find(label);
    if (it == m_dimensionMap.end()) {
		RAISE_RUNTIME_ERROR << "Dimensions::getDimensionSize: dimension: " << label
			<< " not found in dimension map";
    }
    return m_sizes[it->second];
}

int Dimensions::getDimensionIndex(const std::string& label) const {
	auto it = m_dimensionMap.find(label);
	if (it == m_dimensionMap.end()) {
		RAISE_RUNTIME_ERROR << "Dimensions::getDimensionIndex: dimension: " << label
			<< " not found in dimension map";
	}
	return it->second;
}
