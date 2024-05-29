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
#include "sample_pointGrid.h"
#include <bitset>

#include <omp.h>

const size_t CHUNK_SIZE = 1000;
using json = nlohmann::json;
json CFG;
#define DEG_TO_RAD pi / 180.0

const long long BITSET_SIZE = 1000000LL * 360 * 90;


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


void print_eta(long long completed_iter, long long total_iterations, std::chrono::time_point<std::chrono::high_resolution_clock> total_start) {
    auto current_time = std::chrono::high_resolution_clock::now();
    auto elapsed_seconds = std::chrono::duration_cast<std::chrono::seconds>(current_time - total_start).count();
    
    long double percentage = 100 * static_cast<long double>(completed_iter) / static_cast<long double>(total_iterations);
    float total_estimated_time = elapsed_seconds / (percentage / 100.0);
    int eta_seconds = static_cast<int>(total_estimated_time - elapsed_seconds);
    
    int hours = eta_seconds / 3600;
    int minutes = (eta_seconds % 3600) / 60;
    int seconds = eta_seconds % 60;
    
    // std::cout<< completed_iter << " out of " << total_iterations << " iterations completed. ";
    std::cout << percentage << "% are done. eta: " << hours << " hours " << minutes << " minutes " << seconds << " seconds" << std::endl;
}


void save_bitset_to_file(const std::bitset<BITSET_SIZE>& bitset, const std::filesystem::path filename) {
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open the file for writing: " << filename << std::endl;
        return;
    }

    for (size_t i = 0; i < BITSET_SIZE; i += 8) {
        unsigned char byte = 0;
        for (size_t j = 0; j < 8 && (i + j) < BITSET_SIZE; ++j) {
            if (bitset[i + j]) {
                byte |= (1 << j);
            }
        }
        file.write(reinterpret_cast<const char*>(&byte), sizeof(byte));
    }

    file.close();
    std::cout << "Bitset saved to " << filename << std::endl;
}


int calculate_svf(const bvh_node &bvh, const std::vector<GridPoint> &point_grid, const std::filesystem::path &result_path) {
    const float steps_in_degree = 1.0;
    auto total_start = std::chrono::high_resolution_clock::now();
    std::cout<<"entered"<<std::endl;
    
    int n_row = point_grid.size();
    std::cout << "num of row: " << n_row << std::endl;

    float seperation = degrees_to_radians(steps_in_degree);
    int num_theta = static_cast<int>(2 * pi / seperation);
    int num_phi = static_cast<int>(pi / 2 / seperation);

    int n_col = num_theta * num_phi;
    std::cout << "num of col: " << n_col << std::endl;


    

    std::vector<std::string> gmlidArray(n_row);

    // std::cout << omp_get_thread_num() << std::endl;
    int num_threads = CFG["num_threads"];
    std::cout << "num of threads for svf: " << num_threads <<std::endl;
    omp_set_num_threads(num_threads);


    for (size_t j = 0; j < point_grid.size(); ++j) {
    gmlidArray[j] = *(point_grid[j].surf_gmlid);
    }

    long long total_iterations = static_cast<long long>(n_row) * static_cast<long long>(n_col);
    // std::cout << "Maximum value for long long: " << std::numeric_limits<long long>::max() << std::endl;
    std::cout<<"total iterations: "<<total_iterations<<std::endl;
    long long completed_iter = 0;
    const int update_interval_seconds = 30;
    auto last_update_time = std::chrono::high_resolution_clock::now();




    // std::bitset<BITSET_SIZE> results;
    std::unique_ptr<std::bitset<BITSET_SIZE>> results = std::make_unique<std::bitset<BITSET_SIZE>>();

    std::cout<<"binary array created"<<std::endl;


    std::cout<<"start ray tracing..."<<std::endl;

    for (int i = 0; i<point_grid.size(); i++) {
        vec3 dir = point_grid[i].normal;
        std::vector<vec3> directions = sampling::hemisphere_sampling(dir, steps_in_degree);
        for (size_t j = 0; j < directions.size(); j += 1) {
            #pragma omp atomic
            completed_iter += 1;

            vec3 origin = point_grid[i].position;
            ray r(origin, directions[j]);
            hit_record rec;

            if (!bvh.hit(r, 0, infinity, rec)) {
                // results.set(j+i*n_col);
                results->set(j + i * n_col);
            }

            auto current_time = std::chrono::high_resolution_clock::now();
            auto elapsed_seconds = std::chrono::duration_cast<std::chrono::seconds>(current_time - last_update_time).count();
            if (elapsed_seconds >= update_interval_seconds) {
                last_update_time = current_time;
                print_eta(completed_iter, total_iterations, total_start);
            }
        
        }

    }

    std::cout <<"saving result to files..." << std::endl;
    // save_bitset_to_file(results, result_path);
    save_bitset_to_file(*results, result_path);

    
    auto total_end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(total_end - total_start).count();

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

    std::filesystem::path svf_dir = shadow_result_dir / "svf_results.bin";
    CFG["shadow_calc"]["pointgrid_path"] = intermediate_dir / "grid.xyz";

    CFG["shadow_calc"]["gmlid_path"] = shadow_result_dir / "gmlids.csv";

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
    std::vector<GridPoint> grid = sampling::create_point_grid(objects, CFG);

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

    float steps_in_degree = CFG["svf_calc"]["steps_in_degree"];
    std::cout << "steps in degree: " << steps_in_degree << std::endl;

    int t = calculate_svf(bvh, grid, svf_dir);
    int hours = t / 3600;
    int minutes = (t % 3600) / 60;
    int seconds = t % 60;
    // double result = t / directions.size();
    std::cout << "total time for " << grid.size() << " samples: " << t / 1000 << std::endl;
    // std::cout << "average time  for intersection test (one moment) " << result / 1000 << std::endl;
    return 0;

}