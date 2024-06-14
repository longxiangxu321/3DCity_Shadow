// IO.h

#ifndef IO_H
#define IO_H

#include <string> // 这里包含所需的头文件
#include <filesystem>
#include <fstream> 
#include <sstream>
#include <vector>
#include <tuple>
#include <unordered_set>
#include "include/Triangle.h"
#include "include/json.hpp"
#include <string>
using json = nlohmann::json;
/** read obj files*/
class Triangle;

namespace IO {
    std::vector<std::vector<Triangle>> read_obj(const std::string &input_file);

    /** get coordinates from json files*/
    std::vector<vec3> get_coordinates(const json& j, bool translate);

    /** read json files*/
    std::tuple<std::vector<std::vector<Triangle>>, std::vector<std::vector<Triangle>>> read_json(const std::string &input_file, bool target);

    /** read solar position files*/
    std::vector<vec3> readCSVandTransform(const std::filesystem::path& filename);

}



#endif // IO_H


