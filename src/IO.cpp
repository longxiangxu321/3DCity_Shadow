// IO.cpp

#include "IO.h"  
// #include "include/Triangle.h"

std::vector<std::vector<Triangle>> IO::read_obj(const std::string &input_file) {
    std::ifstream input_stream;
    input_stream.open(input_file);
    std::vector<std::vector<Triangle>> objects;
    std::vector<point3> points;
    std::vector<Triangle> current_object;

    bool first_object_read = false;
    int index = 1;

    if (input_stream.is_open()) {
        std::string line;
        while (std::getline(input_stream, line)) {
            if (line[0] == 'v') {
                double x,y,z;
                std::stringstream ss(line);
                std::string temp;
                ss >> temp >> x >> y >> z;
                points.emplace_back(x, y, z);
            }
            if (line[0] == 'o') {//every o means a new object
                if (!first_object_read) {
                    first_object_read = true;
                } else {
                    objects.push_back(current_object);
                    current_object.clear();
                    index++;
                }
            }
            if (line[0] == 'f') {//shell has different Faces
                unsigned long v0, v1, v2;
                std::stringstream ss(line);
                std::string temp;
                ss >> temp >> v0 >> v1 >> v2;
                point3 vx = points[v0-1];
                point3 vy = points[v1-1];
                point3 vz = points[v2-1];
                current_object.emplace_back(Triangle(vx, vy, vz,index));
            }
            else {
                continue;
            }
        }
        if (!current_object.empty()) {
            objects.push_back(current_object);
        }
    }

    return objects;
};

std::vector<vec3> IO::get_coordinates(const json& j, bool translate) {
    std::vector<vec3> lspts;
    std::vector<std::vector<int>> lvertices = j["vertices"];
    if (translate) {
        for (auto& vi : lvertices) {
            double x = (vi[0] * j["transform"]["scale"][0].get<double>()) + j["transform"]["translate"][0].get<double>();
            double y = (vi[1] * j["transform"]["scale"][1].get<double>()) + j["transform"]["translate"][1].get<double>();
            double z = (vi[2] * j["transform"]["scale"][2].get<double>()) + j["transform"]["translate"][2].get<double>();
            lspts.push_back(vec3(x, y, z));
        }
    } else {
        //-- do not translate, useful to keep the values low for downstream processing of data
        for (auto& vi : lvertices) {
            double x = (vi[0] * j["transform"]["scale"][0].get<double>());
            double y = (vi[1] * j["transform"]["scale"][1].get<double>());
            double z = (vi[2] * j["transform"]["scale"][2].get<double>());
            lspts.push_back(vec3(x, y, z));
        }
    }
    return lspts;
}

std::tuple<std::vector<std::vector<Triangle>>, std::vector<std::vector<Triangle>>> IO::read_json(const std::string &input_file, bool target) {
    std::unordered_set<std::string> targetTypes = {"WallSurface", "RoofSurface"};

    std::fstream input(input_file);
    json j;
    input >> j;
    input.close();
    std::vector<vec3> lspts = get_coordinates(j, true);
    std::vector<std::vector<Triangle>> all_objects;
    std::vector<std::vector<Triangle>> target_objects;
    int index = 1;

    for (auto &co: j["CityObjects"].items()) {    //.items() return the key-value pairs belong to the CityObjects
        if (co.value()["type"] == "BuildingPart" || co.value()["type"] == "Building") {
            std::vector<Triangle> current_all_object;
            std::vector<Triangle> current_target_object;
            for (auto &g: co.value()["geometry"]) {
                
                    std::unordered_map<std::string, std::shared_ptr<std::string>> gmlidMap;
                    for (int i = 0; i< g["boundaries"][0].size(); i++) {
                        if (target) {
                            // std::cout<<g["boundaries"][0][i][0]<<std::endl;
                            std::vector<std::vector<int>> triangle = g["boundaries"][0][i];
                            point3 vx = lspts[triangle[0][0]];
                            point3 vy = lspts[triangle[0][1]];
                            point3 vz = lspts[triangle[0][2]];
                            Triangle Tri = Triangle(vx, vy, vz, index);
                            index++;

                            std::string surf_type = g["semantics"]["surfaces"][i]["type"];
                            if (targetTypes.find(surf_type) != targetTypes.end()) {
                                int gmlid_int = g["semantics"]["surfaces"][i]["global_idx"];
                                std::string gmlid = std::to_string(gmlid_int);

                                auto it = gmlidMap.find(gmlid);
                                if (it == gmlidMap.end()) {
                                    // If not found, create a shared pointer for the gmlid
                                    std::shared_ptr<std::string> gmlidPtr = std::make_shared<std::string>(gmlid);
                                    it = gmlidMap.emplace(gmlid, gmlidPtr).first;  // Add it to the map
                                }

                                Tri.surf_gmlid = it->second;
                                current_target_object.emplace_back(Tri);
                            }
                            current_all_object.emplace_back(Tri);
                        }
                        else {
                            std::vector<std::vector<int>> triangle = g["boundaries"][0][i];
                            point3 vx = lspts[triangle[0][0]];
                            point3 vy = lspts[triangle[0][1]];
                            point3 vz = lspts[triangle[0][2]];
                            Triangle Tri = Triangle(vx, vy, vz, index);
                            index++;
                            current_all_object.emplace_back(Tri);
                        }

                    }
                
            }
            all_objects.emplace_back(current_all_object);
            target_objects.emplace_back(current_target_object);
        }
    }
    std::tuple<std::vector<std::vector<Triangle>>, std::vector<std::vector<Triangle>>> result = std::tuple(all_objects, target_objects);
    return result;
}

std::vector<vec3> IO::readCSVandTransform(const std::filesystem::path& filename) {
    std::vector<vec3> unitVectors;
    std::ifstream file(filename);

    if (!file.is_open()) {
        std::ostringstream oss;
        oss << "Could not open file: " << filename;
        throw std::runtime_error(oss.str());
    }

    std::string line;
    std::getline(file, line); // skip header

    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string cell;

        std::getline(ss, cell, ','); // skip date/time

        std::getline(ss, cell, ',');
        double apparent_elevation = std::stod(cell);
        std::getline(ss, cell, ',');
        double azimuth = std::stod(cell);

        // Skip remaining columns
        while (std::getline(ss, cell, ',')) {}

        double theta = degrees_to_radians(apparent_elevation);
        double phi =  degrees_to_radians(azimuth - 90);

        // Conversion to unit vector (in spherical coordinates)
        double x = cos(theta) * cos(phi);
        double y = - cos(theta) * sin(phi);
        double z = sin(theta);
        unitVectors.push_back(vec3(x, y, z));
    }

    file.close();
    return unitVectors;
}