// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include <filesystem>
#include <string>

namespace slideio
{
    class TempFile
    {
    public:
	    explicit TempFile(std::filesystem::path path) : m_path(path)
	    {
		    if(std::filesystem::exists(path))
			    std::filesystem::remove(path);
	    }
        TempFile() : TempFile(static_cast<const char*>(nullptr))
	    {
          
	    }
       explicit TempFile(const char* ext)
       {
          std::string pattern("%%%%-%%%%-%%%%-%%%%.");
          if(ext == nullptr || *ext==0){
             pattern += "temp";
          }
          else{
             pattern += ext;
          }
          m_path = std::filesystem::temp_directory_path();
          m_path /= unique_path(pattern);
       }
       const std::filesystem::path& getPath() const{
		    return m_path;
	    }
	    ~TempFile()
	    {
		    if(std::filesystem::exists(m_path))
			    std::filesystem::remove(m_path);
	    }
      static std::filesystem::path unique_path(const std::string& model = "%%%%-%%%%-%%%%-%%%%");
    private:
	    std::filesystem::path m_path;
    };
}