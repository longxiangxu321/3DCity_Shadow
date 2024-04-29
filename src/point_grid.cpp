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
#include "IO.h"


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


int calculate_shadow(const std::vector<vec3> &directions, const bvh_node &bvh, const std::vector<GridPoint> &point_grid, const std::filesystem::path &result_path) {
    std::ofstream outfile(result_path, std::ios::binary);


    bool save_point = CFG["shadow_calc"]["save_result_pc"];
    
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

    long long completed_iter = 0;
    auto total_start = std::chrono::high_resolution_clock::now();
    const int update_interval_seconds = 30; // Update ETA every 10 seconds
    auto last_update_time = std::chrono::high_resolution_clock::now();
    long long total_iterations = directions.size() * point_grid.size();

    #pragma omp parallel for collapse(2)
    for (int i = 0; i < directions.size(); ++i) {
        for (int j = 0; j < point_grid.size(); j += 1) {
            #pragma omp atomic
            completed_iter += 1;
            vec3 origin = point_grid[j].position;
            ray r(origin, directions[i]);
            hit_record rec;
            // int counts = 0;
            if (dot(point_grid[j].normal, directions[i]) > 0) {
                    if (!bvh.hit(r, 0, infinity, rec)) {
                        binaryArray[j][i]=1;
                    }
            }
            auto current_time = std::chrono::high_resolution_clock::now();
            auto elapsed_seconds = std::chrono::duration_cast<std::chrono::seconds>(current_time - last_update_time).count();
            if (elapsed_seconds >= update_interval_seconds) {
                last_update_time = current_time; // Update last update time
                auto duration = std::chrono::duration_cast<std::chrono::seconds>(current_time - total_start).count();
                long double percentage = 100 * static_cast<long double>(completed_iter) / static_cast<long double>(total_iterations);
                // std::cout<<completed_iter<<"/"<<total_iterations<<std::endl;
                // std::cout<<percentage<<std::endl;

                // auto elapsed_seconds = std::chrono::duration_cast<std::chrono::seconds>(current_time - total_start).count();
                float total_estimated_time = duration / (percentage / 100.0);
                std::cout<<"duration"<<duration<<std::endl;
                std::cout<<"percentage"<<percentage<<std::endl;
                int eta_seconds = static_cast<int>(total_estimated_time - duration);
                // std::cout << eta_seconds << std::endl;
                // Convert `eta_seconds` to hours, minutes, and seconds
                int hours = eta_seconds / 3600;
                int minutes = (eta_seconds % 3600) / 60;
                int seconds = eta_seconds % 60;
                // double eta_ms = (duration / 1000) * ((100 - percentage) / percentage);
                // std::chrono::milliseconds eta(static_cast<long long>(eta_ms));
                // auto hours = eta.count() / 3600;
                // auto minutes = (eta.count() % 3600) / 60;
                // auto seconds = eta.count() % 60;
                std::cout << percentage << "\% are done. " << "eta: " << hours << " hours " << minutes << " minutes " << seconds << " seconds" << std::endl;
            }
        }
    }

    auto total_end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(total_end - total_start).count();

    std::cout <<"saving result to files..." << std::endl;

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



    outfile.close();
    return duration;
}



int main() {

    // std::filesystem::path current_path = "../../..";
    std::filesystem::path current_path = "../";
    try {
        std::filesystem::current_path(current_path); // Set the current working directory
        std::cout << "Current working directory changed to: " << std::filesystem::current_path() << std::endl;
    } catch(const std::filesystem::filesystem_error& e) {
        std::cerr << "Error when changing current directory: " << e.what() << std::endl;
    }

    if (std::filesystem::exists("config.json")) {
        std::cout << "Config file exists." << std::endl;
    }
    else {
        throw std::runtime_error("Config file does not exist.");
    }

    readConfig("config.json");
    std::cout << "Config file read." << std::endl;


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

    std::vector<vec3> directions = IO::readCSVandTransform(sun_dir);
    std::cout<<"moment number "<<directions.size()<<std::endl;

    std::vector<std::vector<Triangle>> objects;

    for(const auto & entry : std::filesystem::directory_iterator(in_dir)) {
        if(entry.path().extension() == ".obj") {
            std::vector<std::vector<Triangle>> obj1 = IO::read_obj(entry.path().string());
            objects.insert(objects.end(), obj1.begin(), obj1.end());
        }
        else if (entry.path().extension() == ".json") {
            std::tuple<std::vector<std::vector<Triangle>>, std::vector<std::vector<Triangle>>> result = IO::read_json(entry.path().string(), true);
            std::vector<std::vector<Triangle>> obj_target = std::get<1>(result);
            objects.insert(objects.end(), obj_target.begin(), obj_target.end());
        }
    }

    std::vector<std::vector<Triangle>> surrounding_objects;
    for(const auto & entry : std::filesystem::directory_iterator(en_dir)) {
        if(entry.path().extension() == ".obj") {
            std::vector<std::vector<Triangle>> obj1 = IO::read_obj(entry.path().string());
            surrounding_objects.insert(surrounding_objects.end(), obj1.begin(), obj1.end());
        }
        else if (entry.path().extension() == ".json") {
            std::tuple<std::vector<std::vector<Triangle>>, std::vector<std::vector<Triangle>>> result = IO::read_json(entry.path().string(), false);
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