// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "slideio/core/slideio_core_def.hpp"
#include <list>
#include <map>
#include <string>
#include <vector>

namespace slideio
{
	class SLIDEIO_CORE_EXPORTS Dimensions
	{
	public:
		Dimensions() = default;
		Dimensions(const std::vector<std::string>& labels, const std::vector<int>& dimensionSizes, const std::vector<int>& increments);
		void init(const std::vector<std::string>& labels, const std::vector<int>& dimensionSizes, const std::vector<int>& increments);
		bool incrementCoordinates(std::vector<int>& coordinates) const;
        std::vector<int> createCoordinates(const std::list<std::pair<std::string, int>>& coordList) const;
		const std::vector<std::string>& getOrder() const { return m_order; }
		const std::vector<int>& getSizes() const { return m_sizes; }
		int getDimensionSize(const std::string& label) const;
		int getDimensionIndex(const std::string& label) const;
		int getNumDimensions() const { return static_cast<int>(m_sizes.size()); }

    private:
        std::vector<std::string> m_order;
		std::vector<int> m_sizes;
		std::map<std::string, int> m_dimensionMap;
		std::vector<int> m_increments;
	};

}
