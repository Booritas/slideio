// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma

#include <vector>

namespace slideio
{
    class SimilarityTools
    {
    public:
        template <typename Type>
        static double dotProduct(const std::vector<Type>& a, const std::vector<Type>& b) {
            // Assuming arrays are of equal length
            double result = 0.0;
            for (std::size_t i = 0; i < a.size(); ++i) {
                result += a[i] * b[i];
            }
            return result;
        }

        template <typename Type>
        static double magnitude(const std::vector<Type>& v) {
            double result = 0.0;
            for (double value : v) {
                result += value * value;
            }
            return sqrt(result);
        }

        template <typename Type>
        static double cosineSimilarity(const std::vector<Type>& a, const std::vector<Type>& b) {
            double dotProd = dotProduct(a, b);
            double magA = magnitude(a);
            double magB = magnitude(b);
            if(magA == 0 && magB == 0) {
                return 1.0;
            }
            else if (magA == 0 || magB == 0) {
                // Handle division by zero
                return 0.0;
            }
            return dotProd / (magA * magB);
        }
    };
}