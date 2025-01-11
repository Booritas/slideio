// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "tempfile.hpp"
#include <filesystem>
#include <random>
#include <string>

std::filesystem::path slideio::TempFile::unique_path(const std::string& model)
{
   static constexpr char charset[] = 
      "0123456789"
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
      "abcdefghijklmnopqrstuvwxyz";

   thread_local static std::mt19937_64 rg{std::random_device{}()};
   thread_local static std::uniform_int_distribution<std::size_t> dist(
      0, sizeof(charset) - 2
   );
   std::string result;
   result.reserve(model.size());
   for (char c : model) {
      if (c == '%') {
         result.push_back(charset[dist(rg)]);
      } else {
         result.push_back(c);
      }
   }
   return std::filesystem::path(result);
}