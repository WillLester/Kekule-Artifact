#pragma once

#include <string>
#include <vector>

class Utility {
public:
	static std::vector<std::string> split_str(std::string str, char sep) {
		std::vector<std::string> result;
        size_t pos1 = 0, pos2 = str.find(sep);
        while (pos2 != std::string::npos) {
            std::string sub = str.substr(pos1, pos2-pos1);
            result.push_back(sub);
            pos1 = pos2 + 1;
            pos2 = str.find(sep, pos1);
        }
        if (pos1 != str.size()) {
            result.push_back(str.substr(pos1));
        }
        return result;	
	}
};
