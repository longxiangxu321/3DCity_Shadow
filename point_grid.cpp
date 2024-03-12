//
// Created by 55241 on 2023/5/6.
//

#include <fstream>
#include <string>
#include <vector>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <filesystem>
#include <sstream>
#include <unordered_set>

#include "include/Triangle.h"
#include "include/vec3.h"
#include "include/bvh.h"
#include "include/grid_point.h"
#include "include/json.hpp"

#include <omp.h>

using json = nlohmann::json;
json CFG;
#define DEG_TO_RAD pi / 180.0

void readConfig(const std::string& filename) {
    std::ifstream file(filename);
    if (file.is_open()) {
        try {
            file >> CFG;
        } catch (const json::parse_error& e) {
            std::cerr << "JSON parsing error: " << e.what() << '\n';
            // Handle the error or throw an exception
        }
    } else {
        std::cerr << "Unable to open file: " << filename << '\n';
        // Handle the error or throw an exception
    }
}


void calculate_mass_center(const vec3 &A, const vec3 &B, const vec3 &C, const vec3 &direction, const int splits,
                           std::stringstream &output_stream, std::vector<GridPoint> &grid_n, int current_depth = 0,
                           const int &max_depth = 10, const std::shared_ptr<std::string> &surf_gmlid_ptr = nullptr)
{
//    vec3 sun_d(1,1,0.5);
    vec3 D,E,F;
    D = (A+B)/2;
    E = (B+C)/2;
    F = (C+A)/2;

    if (current_depth > max_depth) {
        return;  // prevent excessive recursion
    }

    if (splits == 0) {
        vec3 vo = (A + B + C) /3;
        if (!vo.x() || !vo.y() || !vo.z()) return;
        output_stream << std::setprecision(10) << vo.x() + direction.x() * 0.1 << " " << vo.y() + direction.y() * 0.1 << " " << vo.z() + direction.z() * 0.1<< " "<< direction.x() << " "<<
        direction.y() << " "<< direction.z() <<"\n";
//        output_stream << std::setprecision(10) << vo.x() + sun_d.x() * 2 << " " << vo.y() + sun_d.y() * 2 << " " << vo.z() + sun_d.z() * 2<<"\n";
        vec3 position = vec3(vo.x() + direction.x() * 0.1, vo.y() + direction.y() * 0.1, vo.z() + direction.z() * 0.1);
        vec3 normal = vec3(direction.x(), direction.y(), direction.z());
        GridPoint gp(position + direction*0.001, normal);
        gp.surf_gmlid = surf_gmlid_ptr;
        grid_n.emplace_back(gp);
//        return;
    }

    if (splits == -1) {

        if ( (vec_distance(A,B)/ vec_distance(B,C))>10 || (vec_distance(A,C)/ vec_distance(B,C))>10) {
            calculate_mass_center(A, D, F, direction, 0, output_stream, grid_n, current_depth + 1, max_depth,
                                  surf_gmlid_ptr);
        }
        if ( (vec_distance(A,B)/ vec_distance(A,C)) >10 || (vec_distance(B,C)/ vec_distance(A,C))>10) {
            calculate_mass_center(D, B, E, direction, 0, output_stream, grid_n, current_depth + 1, max_depth,
                                  surf_gmlid_ptr);
        }
        if ( (vec_distance(A,C)/ vec_distance(A,B)) >10 || (vec_distance(B,C)/ vec_distance(A,B))>10) {
            calculate_mass_center(F, E, C, direction, 0, output_stream, grid_n, current_depth + 1, max_depth,
                                  surf_gmlid_ptr);
        }
        return;
    }

    calculate_mass_center(A, D, F, direction, splits - 1, output_stream, grid_n, current_depth + 1, max_depth,
                          surf_gmlid_ptr);
    calculate_mass_center(D, B, E, direction, splits - 1, output_stream, grid_n, current_depth + 1, max_depth,
                          surf_gmlid_ptr);
    calculate_mass_center(F, E, C, direction, splits - 1, output_stream, grid_n, current_depth + 1, max_depth,
                          surf_gmlid_ptr);
    calculate_mass_center(D, E, F, direction, splits - 1, output_stream, grid_n, current_depth + 1, max_depth,
                          surf_gmlid_ptr);
}


std::vector<GridPoint> create_point_grid(const std::vector<std::vector<Triangle>>& mesh) {
//    std::string output_file = "../data/grid.xyz";
    std::string output_file = CFG["shadow_calc"]["pointgrid_path"];
    std::ofstream out(output_file);



    std::vector<GridPoint> grid_current;

    if (!out.is_open()) {
        std::cout << "Error opening file " << "'" << output_file <<"'.";
    }

    else
    {
        std::stringstream ss;
        std::stringstream specification_stream;
        specification_stream<<"grid_start"<<" "<<"grid_end"<<"\n";
        for (auto it = mesh.begin(); it!=mesh.end(); ++it) {
            for (auto jt = it->begin(); jt!=it->end(); ++jt) {
                specification_stream<<grid_current.size()<<" ";
                int num_s;
                if (jt->area() <=2) num_s = 0;
                else if (jt->area() <4 && jt-> area() >2) num_s = 1;
                else
                {
                    double num_half_square = jt->area();
                    num_s = std::floor(std::log(num_half_square) / std::log(4));
                }

                if (jt->surf_gmlid == nullptr) {
                    // Pointer is nullptr, raise an error
                    std::cerr << "Error: gmlidPtr is nullptr!" << std::endl;
                } else {
                    // Pointer is not nullptr, you can safely use it
                    //                calculate_mass_center(jt->v0, jt->v1, jt->v2, unit_vector(jt->normal), num_s, ss, grid_current, 0, num_s+2);
                    calculate_mass_center(jt->v0, jt->v1, jt->v2, unit_vector(jt->normal), num_s, ss, grid_current, 0, 3,
                                          jt->surf_gmlid);
                    specification_stream<<grid_current.size() - 1<<"\n";
                }
            }
        }

        out << ss.str();
    }
    return grid_current;
}

std::vector<std::vector<Triangle>> read_obj(const std::string &input_file){
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

std::vector<vec3> get_coordinates(const json& j, bool translate) {
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

std::tuple<std::vector<std::vector<Triangle>>, std::vector<std::vector<Triangle>>> read_json(const std::string &input_file, bool target) {
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
                if (g["lod"]=="2") {
                    std::unordered_map<std::string, std::shared_ptr<std::string>> gmlidMap;

                    for (int i = 0; i< g["boundaries"].size(); i++) {
                        if (target) {
                            std::vector<std::vector<int>> triangle = g["boundaries"][i];
                            point3 vx = lspts[triangle[0][0]];
                            point3 vy = lspts[triangle[0][1]];
                            point3 vz = lspts[triangle[0][2]];
                            Triangle Tri = Triangle(vx, vy, vz, index);
                            index++;
                            int semantic_index = g["semantics"]["values"][i];
                            std::string surf_type = g["semantics"]["surfaces"][semantic_index]["type"];
                            if (targetTypes.find(surf_type) != targetTypes.end()) {
                                std::string gmlid = g["semantics"]["surfaces"][semantic_index]["id"];

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
            }
            all_objects.emplace_back(current_all_object);
            target_objects.emplace_back(current_target_object);
        }
    }
    std::tuple<std::vector<std::vector<Triangle>>, std::vector<std::vector<Triangle>>> result = std::tuple(all_objects, target_objects);
    return result;
}

int calculate_shadow(const std::vector<vec3> &directions, const bvh_node &bvh, const std::vector<GridPoint> &point_grid, const std::filesystem::path &result_path) {
    std::ofstream outfile(result_path, std::ios::binary);

//    const size_t buffer_size = 100000; // Size of the buffer in terms of GridPoints
//    std::vector<unsigned char> buffer;
//    buffer.reserve(buffer_size * ((directions.size() + 7) / 8));
    bool save_point = CFG["shadow_calc"]["save_result_pc"];
    auto total_start = std::chrono::high_resolution_clock::now();
    int n_row = point_grid.size();
    int n_col = directions.size();
    std::vector<std::vector<int>> binaryArray(n_row, std::vector<int>(n_col, 0));
    std::vector<std::string> gmlidArray(n_row);

    // std::cout << omp_get_thread_num() << std::endl;
    int num_threads = CFG["num_threads"];
    std::cout << "num of threads for ray trace: " << num_threads <<std::endl;
    omp_set_num_threads(num_threads);


    for (size_t j = 0; j < point_grid.size(); ++j) {
    gmlidArray[j] = *(point_grid[j].surf_gmlid);
    }

    #pragma omp parallel for
    for (int i = 0; i < directions.size(); ++i) {
        auto start = std::chrono::high_resolution_clock::now();

        std::stringstream ss;

        for (int j = 0; j < point_grid.size(); j += 1) {
            // if (i==0) gmlidArray[j]= *(point_grid[j].surf_gmlid);

            vec3 origin = point_grid[j].position;
            ray r(origin, directions[i]);
            hit_record rec;
            int counts = 0;
            if (dot(point_grid[j].normal, directions[i]) > 0) {
                    if (!bvh.hit(r, 0, infinity, rec)) {
                        // if (save_point)
                        //     ss <<std::setprecision(10) << r.origin().x() << " " << r.origin().y() << " " << r.origin().z() <<"\n";
                        binaryArray[j][i]=1;
                    }
            }
        }
        // if (save_point) {
        //     std::string result_pc_path = CFG["shadow_calc"]["result_pc_path"];
        //     std::string out_file = result_pc_path + "pc" + std::to_string(i) + ".xyz";
        //     std::ofstream out(out_file);
        //     out << ss.str();
        // }

    }



    std::ofstream outFile(result_path, std::ios::out | std::ios::binary);

    if (!outFile) {
        std::cerr << "Error: Unable to open the file for writing." << result_path<< std::endl;
        return 1;
    }


    outFile.write(reinterpret_cast<char*>(&n_row), sizeof(n_row));
    outFile.write(reinterpret_cast<char*>(&n_col), sizeof(n_col));


    for (int row = 0; row < n_row; ++row) {
        for (int col = 0; col < n_col; ++col) {
            int value = binaryArray[row][col];
            outFile.write(reinterpret_cast<char*>(&value), sizeof(value));
        }
    }


    outFile.close();


    // Open a CSV file for writing
//    std::ofstream csvFile("../data/gmlids.csv");
    std::string gmlid_path = CFG["shadow_calc"]["gmlid_path"];
    std::ofstream csvFile(gmlid_path);

    if (!csvFile.is_open()) {
        std::cerr << "Error opening CSV file for writing." << std::endl;
        return 1;
    }

    // Iterate through the vector and write each string to the CSV file
    for (int i = 0; i < n_row; i++) {
        // Ensure proper CSV formatting (e.g., escaping characters if needed)
        // For simplicity, let's assume strings don't contain special characters
        std::string csvString = gmlidArray[i];

        // Write the string to the CSV file, followed by a comma
        csvFile << csvString;

        // Add a comma after each string except the last one
        if (i < n_row - 1) {
            csvFile << ",";
        }

        // Add a newline character to move to the next row
        csvFile << "\n";
    }

    // Close the CSV file
    csvFile.close();

    std::cout << "csv file of gmlids saved to " << gmlid_path << std::endl;

    std::cout << "Binary array saved to " << result_path << std::endl;


    auto total_end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(total_end - total_start).count();
    outfile.close();
    return duration;
}


std::vector<vec3> readCSVandTransform(const std::filesystem::path& filename) {
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
//        std::cout<<x<<" "<<y<<" "<<z<<std::endl;
        unitVectors.push_back(vec3(x, y, z));
    }

    file.close();
    return unitVectors;
}

int main() {

    std::filesystem::path current_path = "../../..";
    try {
        std::filesystem::current_path(current_path); // Set the current working directory
        std::cout << "Current working directory changed to: " << std::filesystem::current_path() << std::endl;
    } catch(const std::filesystem::filesystem_error& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }

    if (std::filesystem::exists("config.json")) {
        std::cout << "File exists." << std::endl;
    }
    else {
        throw std::runtime_error("File does not exist.");
    }

    readConfig("config.json");



    std::filesystem::path root_folder = CFG["study_area"]["data_root"];
    std::filesystem::path in_dir = root_folder / "citymodel" / "target_tiles";
    std::filesystem::path en_dir = root_folder / "citymodel" / "neighbouring_tiles";
    std::filesystem::path output_folder = root_folder / CFG["shadow_calc"]["output_folder_name"];

    std::filesystem::path intermediate_dir = output_folder / "intermediate";
    std::filesystem::path shadow_result_dir = output_folder / "shadow_result";
    std::filesystem::create_directories(intermediate_dir);
    std::filesystem::create_directories(shadow_result_dir);

    std::filesystem::path sun_dir = intermediate_dir / "sun_pos.csv";
    std::filesystem::path result_dir = shadow_result_dir / "results.bin";
    CFG["shadow_calc"]["pointgrid_path"] = intermediate_dir / "grid.xyz";

    CFG["shadow_calc"]["result_pc_path"] = shadow_result_dir / "result_pcs";
    std::filesystem::create_directories(CFG["shadow_calc"]["result_pc_path"]);

    CFG["shadow_calc"]["gmlid_path"] = shadow_result_dir / "gmlids.csv";

    std::vector<vec3> directions = readCSVandTransform(sun_dir);
    std::cout<<"moment number "<<directions.size()<<std::endl;

    std::vector<std::vector<Triangle>> objects;

    for(const auto & entry : std::filesystem::directory_iterator(in_dir)) {
        if(entry.path().extension() == ".obj") {
            std::vector<std::vector<Triangle>> obj1 = read_obj(entry.path().string());
            objects.insert(objects.end(), obj1.begin(), obj1.end());
        }
        else if (entry.path().extension() == ".json") {
            std::tuple<std::vector<std::vector<Triangle>>, std::vector<std::vector<Triangle>>> result = read_json(entry.path().string(), true);
            std::vector<std::vector<Triangle>> obj_target = std::get<1>(result);
            objects.insert(objects.end(), obj_target.begin(), obj_target.end());
        }
    }

    std::vector<std::vector<Triangle>> surrounding_objects;
    for(const auto & entry : std::filesystem::directory_iterator(en_dir)) {
        if(entry.path().extension() == ".obj") {
            std::vector<std::vector<Triangle>> obj1 = read_obj(entry.path().string());
            surrounding_objects.insert(surrounding_objects.end(), obj1.begin(), obj1.end());
        }
        else if (entry.path().extension() == ".json") {
            std::tuple<std::vector<std::vector<Triangle>>, std::vector<std::vector<Triangle>>> result = read_json(entry.path().string(), false);
            std::vector<std::vector<Triangle>> obj_all = std::get<0>(result);
            surrounding_objects.insert(surrounding_objects.end(), obj_all.begin(), obj_all.end());
        }
    }

    std::cout<<"target building num "<<objects.size()<<std::endl;
    std::cout<<"total building num "<<objects.size()+surrounding_objects.size()<<std::endl;
    std::vector<GridPoint> grid = create_point_grid(objects);

    int face_num =0;
    hittable_list scene;
    for (auto it = objects.begin(); it!=objects.end(); ++it) {
        for (auto jt = it->begin(); jt != it->end(); ++jt) {
            scene.add(std::make_shared<Triangle>(*jt));
            face_num++;
        }
    }
    std::cout<<"total target triangles "<< face_num<<std::endl;

    for (auto it = surrounding_objects.begin(); it!=surrounding_objects.end(); ++it) {
        for (auto jt = it->begin(); jt != it->end(); ++jt) {
            scene.add(std::make_shared<Triangle>(*jt));
            face_num++;
        }
    }
    std::cout<<"total scene triangles "<< face_num<<std::endl;

    std::cout<<"point  grid created"<<std::endl;
    std::cout<<"grid point number "<<grid.size()<<std::endl;



    auto start = std::chrono::high_resolution_clock::now();

    auto bvh = bvh_node(scene, 0, 1);
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    std::cout << "Time taken for bvh construction: " << duration << " milliseconds." << std::endl;
    int t = calculate_shadow(directions, bvh, grid, result_dir);
    int hours = t / 3600;
    int minutes = (t % 3600) / 60;
    int seconds = t % 60;
    double result = t / directions.size();
    std::cout << "total time for " << directions.size() << " moment: " << t / 1000 << std::endl;
    std::cout << "average time  for intersection test (one moment) " << result / 1000 << std::endl;
    return 0;

}