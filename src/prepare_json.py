from cjio import cityjson
from copy import deepcopy
import os
import json
import subprocess
import shutil
from pathlib import Path

def check_extension(filename, extension):
    return Path(filename).suffix.lower() == extension.lower()


def assign_cityobject_attribute(cm):
        """assign the semantic surface with new attribute global_idx.
        Returns a copy of the citymodel.
        """

        global_idx = 0

        # index = 0
        cm_copy = deepcopy(cm)

        for i, cityobject in enumerate(cm_copy['CityObjects'].values()):
            if cityobject['type'] == 'BuildingPart':
                semantics = cityobject['geometry'][0]['semantics']
                new_semantics = {}

                n_surfaces = []
                n_values = []
                for i in range(len(semantics['values'][0])):
                    sem_value = semantics['values'][0][i]
                    previous_surf_semantic = semantics['surfaces'][sem_value]
                    new_surf_semantic = deepcopy(previous_surf_semantic)
                    new_surf_semantic['global_idx'] = global_idx
                    n_surfaces.append(new_surf_semantic)
                    n_values.append(i)
                    global_idx += 1

                new_semantics['surfaces'] = n_surfaces
                new_semantics['values'] = [n_values]
                cityobject['geometry'][0]['semantics'] = new_semantics
        return cm_copy

def prepare_and_save_target_json(fn,lod):
    """Prepare the target tiles"""
    filename = fn
    lod = lod

    directory_path = os.path.dirname(os.path.dirname(filename))
    filename_without_path = os.path.basename(filename)
    name_without_extension = os.path.splitext(os.path.splitext(filename_without_path)[0])[0]

    # triangulated temporary cityjson
    temp_file = os.path.join(directory_path, 'temp', name_without_extension + '_temp.city.json')
    tem_file_str = str(temp_file)
    command = f'''cjio {filename} upgrade lod_filter {lod} triangulate save {tem_file_str}'''

    # current_working_directory = os.getcwd()

    result = subprocess.run(command, shell=True, capture_output=True, text=True)
    print("target tiles triangulated and saved to: ",tem_file_str)
    
    # city = cityjson.load(tem_file_str, transform=False)
    city = json.load(open(tem_file_str))
    # breakpoint()
    new_city = assign_cityobject_attribute(city)
    # new_city.j['transform']=city.transform
    

    save_file = name_without_extension + '_processed.city.json'

    
    save_file = os.path.join(directory_path, 'processed_target_tiles', save_file)
    # cityjson.save(new_city, save_file)
    with open(save_file, 'w') as f:
        json.dump(new_city, f)

    print("target tiles processed and saved to: ",save_file)


def prepare_and_save_surrounding_json(fn, lod):
    """Prepare the target tiles"""
    filename = fn
    lod = lod

    directory_path = os.path.dirname(os.path.dirname(filename))
    filename_without_path = os.path.basename(filename)
    name_without_extension = os.path.splitext(os.path.splitext(filename_without_path)[0])[0]

    # triangulated temporary cityjson
    triangulated_file = os.path.join(directory_path, 'processed_neighbouring_tiles', name_without_extension + '_triangulated.cityjson')
    command = f'''cjio {filename} upgrade lod_filter {lod} triangulate vertices_clean save {triangulated_file}'''
    result = subprocess.run(command, shell=True, capture_output=True, text=True)
    print("surrounding tiles triangulated and saved to: ",triangulated_file)

def main(CFG):
    data_root = CFG["study_area"]["data_root"]
    lod = CFG["study_area"]["lod"]

    target_tiles_path = os.path.join(data_root, 'citymodel', 'target_tiles')
    temp_tiles_path = os.path.join(data_root, 'citymodel', 'temp')
    processed_tiles_path = os.path.join(data_root, 'citymodel', 'processed_target_tiles')
    
    surrounding_tiles_path = os.path.join(data_root, 'citymodel', 'neighbouring_tiles')
    processed_surrounding_tiles_path = os.path.join(data_root, 'citymodel', 'processed_neighbouring_tiles')

    if not os.path.exists(processed_tiles_path):
        os.makedirs(processed_tiles_path)

    if not os.path.exists(temp_tiles_path):
        os.makedirs(temp_tiles_path)

    if not os.path.exists(processed_surrounding_tiles_path):
        os.makedirs(processed_surrounding_tiles_path)

    target_tiles = os.listdir(target_tiles_path)
    print(" ")
    print("----------------------------------")
    print("preparing target tiles ...")
    for tile in target_tiles:
        if check_extension(tile, '.json'):
            tile_path = os.path.join(target_tiles_path, tile)
            prepare_and_save_target_json(tile_path, lod)
    print("----------------------------------")

    print(" ")
    print("----------------------------------")
    print("preparing surrounding tiles ...")
    if os.path.exists(surrounding_tiles_path):
        surrounding_tiles = os.listdir(surrounding_tiles_path)
        for tile in surrounding_tiles:
            if check_extension(tile, '.json'):
                tile_path = os.path.join(surrounding_tiles_path, tile)
                prepare_and_save_surrounding_json(tile_path, lod)
            elif check_extension(tile, '.obj'):
                destination_path = os.path.join(processed_surrounding_tiles_path, tile)
                shutil.move(os.path.join(surrounding_tiles_path, tile), destination_path)
                print("surrounding tiles moved to: ",destination_path)
    print("----------------------------------")
    if os.path.exists(temp_tiles_path):
        shutil.rmtree(temp_tiles_path)

if __name__ == '__main__':
    script_dir = os.path.dirname(os.path.abspath(__file__))
    parent_dir = os.path.dirname(script_dir)
    os.chdir(parent_dir)

    with open('config.json', 'r') as file:
        CONFIG = json.load(file)

    main(CONFIG)