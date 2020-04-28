// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include <boost/filesystem.hpp>
#include <string>

namespace slideio
{
    class TempFile
    {
    public:
	    explicit TempFile(boost::filesystem::path path) : m_path(path)
	    {
		    if(boost::filesystem::exists(path))
			    boost::filesystem::remove(path);
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
          m_path = boost::filesystem::temp_directory_path();
          m_path /= boost::filesystem::unique_path(pattern);
       }
       const boost::filesystem::path& getPath() const{
		    return m_path;
	    }
	    ~TempFile()
	    {
		    if(boost::filesystem::exists(m_path))
			    boost::filesystem::remove(m_path);
	    }
    private:
	    boost::filesystem::path m_path;
    };
}