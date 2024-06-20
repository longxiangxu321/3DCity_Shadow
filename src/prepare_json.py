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
        new_cos = {}
        cm_copy = deepcopy(cm)
        global_idx = 0
        for co_id, co in cm_copy.cityobjects.items():
            for geom in co.geometry:
                target_surfaces = {}
                local_idx = 0
                for type_surfaces in geom.surfaces.values():
                    surface_indexes = type_surfaces['surface_idx']

                    if surface_indexes is None:
                        continue
                    attributes = {k: v for k, v in type_surfaces.items() if k != 'surface_idx'}

                    for surface_index in surface_indexes:
                        new_surface = attributes.copy()
                        new_surface['surface_idx'] = [surface_index]
                        new_surface['attributes'] = new_surface.get('attributes', {}).copy()
                        
                        new_surface['attributes']['global_idx'] = global_idx
                        target_surfaces[local_idx] = new_surface
                        global_idx += 1
                        local_idx += 1

                geom.surfaces = target_surfaces

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

    current_working_directory = os.getcwd()

    result = subprocess.run(command, shell=True, capture_output=True, text=True)

    
    city = cityjson.load(tem_file_str, transform=False)
    new_city = assign_cityobject_attribute(city)
    new_city.j['transform']=city.transform
    

    save_file = name_without_extension + '_processed.city.json'

    
    save_file = os.path.join(directory_path, 'processed_target_tiles', save_file)
    cityjson.save(new_city, save_file)
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