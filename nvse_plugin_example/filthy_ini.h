#pragma once
// This litterally only does one thing: get the IP and Port from the file
// It's not usefull for anything else
#include "nvse/PluginAPI.h"
#include <iostream>
#include <string>
#include <fstream>
#include <regex>
#include <map>
class filthy_ini
{
public:
	static std::map<std::string, std::string> GetIp(NVSEInterface* g_nvseInterface, std::string file_Name);
};