#pragma once
#include <string>

class TestTools
{
public:
	static std::string getTestImageDirectory();
	static std::string getTestImagePath(const std::string& subfolder, const std::string& image);
};

