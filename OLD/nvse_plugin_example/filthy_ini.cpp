#include "filthy_ini.h"
/**
* Code for trimming leading and trailing whitespace taken from https://stackoverflow.com/questions/216823/how-to-trim-an-stdstring
*/
// trim from start (in place)
static inline void ltrim(std::string& s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
        return !std::isspace(ch);
        }));
}

// trim from end (in place)
static inline void rtrim(std::string& s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
        }).base(), s.end());
}

// trim from both ends (in place)
static inline void trim(std::string& s) {
    rtrim(s);
    ltrim(s);
}

std::map<std::string, std::string> filthy_ini::GetIp(NVSEInterface* g_nvseInterface, std::string file_Name)
{
    // Get full path to ini file
    std::string fullPath = g_nvseInterface->GetRuntimeDirectory() + std::string("/Data/NVSE/Plugins/") + file_Name;
    std::string record;
    std::string key;
    std::string value;
    std::map<std::string, std::string> details;

    std::ifstream iniFile(fullPath);
    if (iniFile.is_open())
    {
        // read
        while (!iniFile.eof())
        {
            std::getline(iniFile, record); // Read record
            // trim whitespace
            trim(record);
            // To avoid nesting hard, I'm using continue to skip lines
            if (record[0] == ';') { continue; } // If line is a comment
            //if( std::regex_match(record, std::regex("([)(.*)(])")) ) {continue;} // If line is a section header
            if (record[0] == '[') { continue; } // Line is probably a section header
            // Getting connection details
            std::cout << "[Connection Details]";
            //std::cout << record << std::endl;
            key = record.substr(0, record.find("="));
            trim(key);
            value = record.substr(record.find("=") + 1, record.length() -1);
            trim(value);
            std::cout << "'" << key << ":" << value << "'" << std::endl;
            details[key] = value;
        }
    }
    else
    {
        std::cerr << "Unable to open the ini file for quest sync server info!";
    }

    return details;
}
