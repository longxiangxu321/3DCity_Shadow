import numpy as np
import pandas as pd
import os
import json
import pvlib
from joblib import Parallel, delayed
import logging
import time
import threading
import contextlib
from tqdm import tqdm
import joblib
# Set up basic configuration for logging
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
with open('config.json', 'r') as file:
    CFG = json.load(file)

def read_result(CFG):
    data_root = os.path.join(CFG['study_area']['data_root'], 
                             CFG['shadow_calc']['output_folder_name'])
    bin_file = os.path.join(data_root, 'shadow_result','results.bin')
    grid_file = os.path.join(data_root, 'intermediate', 'grid.xyz')
    gmlid_file = os.path.join(data_root, 'shadow_result','gmlids.csv')
    sunpos_file = os.path.join(data_root, 'intermediate','sun_pos.csv')

    with open(bin_file, "rb") as file:
        result_bin = np.fromfile(file, dtype=np.int32)
        num_rows = result_bin[0]
        num_cols = result_bin[1]  
        # Remove the first two elements (number of rows and columns)
        result_bin = result_bin[2:]

        result_bin = result_bin.reshape((num_rows, num_cols))

    point_grid = []
    with open(grid_file, 'r') as file:
        for line in file:
            x, y, z, nx, ny, nz = line.split()
            point_grid.append([float(x), float(y), float(z), float(nx), float(ny), float(nz)])

    point_grid = np.array(point_grid)

    gmlids = pd.read_csv(gmlid_file, header=None)
    gmlids = gmlids[[0]]
    gmlids.columns = ['gmlid']
    sunpos = pd.read_csv(sunpos_file)
    sunpos.columns.values[0]='timestamp'
    gmlid_array = gmlids.to_numpy()

    assert result_bin.shape[0] == point_grid.shape[0]==gmlids.shape[0]
    assert result_bin.shape[1] == sunpos.shape[0]
    print("Num of faces: ", gmlids['gmlid'].nunique())
    print("Num of sample points: ", point_grid.shape[0])
    print("Num of timestamps: ", result_bin.shape[1])
    return result_bin, point_grid, gmlid_array, sunpos

def normal2angle(normal_vector):

    normal_vector = np.array(normal_vector)
    magnitude = np.linalg.norm(normal_vector)
    nx, ny, nz = normal_vector / magnitude
    tilt = np.degrees(np.arccos(nz))
    azimuth = np.degrees(np.arctan2(nx, ny))
    if azimuth < 0:
        azimuth += 360

    return tilt, azimuth

def aggregate_shadow(gmlids, merged_result):
    """
    Aggregate the shadow results by gmlid
    gmlids: array of gmlids
    merged_result: array of shadow results,     
        merged_result[:, -6:-3] are the xyz coordinates
        merged_result[:, -3:] are the normal vectors
    """
    indices = []

    start_idx = 0
    unique_gmlids = []
    unique_gmlids.append(gmlids[0])
    for i in range(1, len(gmlids)):
        if gmlids[i] != gmlids[i - 1]:
            unique_gmlids.append(gmlids[i])
            end_idx = i
            indices.append((start_idx, end_idx))
            start_idx = i
    # Add the last group
    indices.append((start_idx, len(gmlids)))

    # Initialize an array to store the aggregated results
    aggregated_data = np.empty((len(indices), merged_result.shape[1]))

    # Calculate averages for each group
    for i, (start, end) in enumerate(indices):
        aggregated_data[i, :] = np.mean(merged_result[start:end, :], axis=0)

    # breakpoint()
    # result_array = np.hstack((aggregated_data[:, :-1], aggregated_data[:, -3:]))
    return aggregated_data, unique_gmlids


def obtain_tmy(latitude, longitude, sunpos):
    tmy = pvlib.iotools.get_pvgis_tmy(latitude, longitude)
    irradiance_data = tmy[0]

    irradiance_data.index = pd.to_datetime(irradiance_data.index)
    tmy = pvlib.iotools.get_pvgis_tmy(latitude, longitude)
    irradiance_data = tmy[0]

    irradiance_data.index = pd.to_datetime(irradiance_data.index)

    sunpos['timestamp'] = pd.to_datetime(sunpos['timestamp'], utc=True)
    all_ghi = []
    all_dni = []
    all_dhi = []
    all_solar_zenith = []
    all_solar_azimuth = []
    all_dni_extra = []

    for index, row in sunpos.iterrows():
            row_time = row['timestamp'].tz_convert(irradiance_data.index.tz)
            month, day, hour = row_time.month, row_time.day, row_time.hour
            match = irradiance_data[(irradiance_data.index.month == month) & 
                        (irradiance_data.index.day == day) & 
                        (irradiance_data.index.hour == hour)]
            solar_zenith = row['apparent_zenith']
            solar_azimuth = row['azimuth']
            dni_extra = pvlib.irradiance.get_extra_radiation(row['timestamp'])
            if (not match.empty):
                ghi, dni, dhi = match.iloc[0][['ghi', 'dni', 'dhi']]
                all_ghi.append(ghi)
                all_dni.append(dni)
                all_dhi.append(dhi)
                all_solar_zenith.append(solar_zenith)
                all_solar_azimuth.append(solar_azimuth)
                all_dni_extra.append(dni_extra)
            else:
                print("error finding match")
    
    return all_ghi, all_dni, all_dhi, all_solar_zenith, all_solar_azimuth, all_dni_extra


def calculate_irradiance(result_array, all_ghi, all_dni, all_dhi, all_solar_zenith, all_solar_azimuth):
    """
    calculate the irradiance for each point in the result_array
    result_array: array of shadow results,
    # result_array.shape[0] are the number of surfaces
    # result_array[:, -6:-3] are xyz coordinates
    # result_array[:, -3:] are the normal vectors
    """
    result_arr = np.zeros(result_array.shape, dtype=float)
    directs = np.zeros(result_array.shape, dtype=float)
    diffuses = np.zeros(result_array.shape, dtype=float)

    for i in range(result_array.shape[0]):
        # point_result = []
        surface_normal = result_array[i][-3:]
        surface_tilt, surface_azimuth = normal2angle(surface_normal)
        for j in range(len(all_ghi)):
                poa = pvlib.irradiance.get_total_irradiance(surface_tilt, surface_azimuth, 
                                                            all_solar_zenith[j],
                                                            all_solar_azimuth[j], 
                                                            all_dni[j], all_ghi[j], all_dhi[j])
                direct = poa['poa_global']
                diffuse = poa['poa_diffuse']
                directs[i][j] = direct
                diffuses[i][j] = diffuse

    return result_arr, directs, diffuses

def calculate_single_surface(i, result_array, all_ghi, all_dni, all_dhi, all_solar_zenith, all_solar_azimuth, all_dni_extra, all_airmass, model):
    """
    Calculates irradiance for a single surface, to be used in parallel processing.
    """
    surface_normal = result_array[i][-3:]
    surface_tilt, surface_azimuth = normal2angle(surface_normal)
    directs_local = np.zeros(len(all_ghi))
    diffuses_local = np.zeros(len(all_ghi))
    
    for j in range(len(all_ghi)):
        
        poa = pvlib.irradiance.get_total_irradiance(surface_tilt, surface_azimuth, 
                                                    all_solar_zenith[j],
                                                    all_solar_azimuth[j], 
                                                    all_dni[j]+1e-10, all_ghi[j]+1e-10, all_dhi[j]+1e-10, dni_extra=all_dni_extra[j], airmass=all_airmass[j], model=model)
        direct = poa['poa_global']
        diffuse = poa['poa_diffuse']
            
        directs_local[j] = direct
        diffuses_local[j] = diffuse
    
    return directs_local, diffuses_local

def calculate_irradiance_parallel(result_array, all_ghi, all_dni, all_dhi, all_solar_zenith, all_solar_azimuth, all_dni_extra, all_airmass, model):
    """
    Parallelized calculation of the irradiance for each point in the result_array.
    """

    num_surfaces = result_array.shape[0]
    directs = np.zeros((num_surfaces, len(all_ghi)))
    diffuses = np.zeros((num_surfaces, len(all_ghi)))
    
    with tqdm_joblib(tqdm(desc="My calculation", total=num_surfaces)) as progress_bar:
        results = Parallel(n_jobs=CFG['num_threads'])(delayed(calculate_single_surface)(i, result_array, all_ghi, all_dni, all_dhi, 
                                                                                        all_solar_zenith, all_solar_azimuth, all_dni_extra, all_airmass, model) for i in range(num_surfaces))
    
    for i, (directs_local, diffuses_local) in enumerate(results):
        directs[i] = directs_local
        diffuses[i] = diffuses_local

    return directs, diffuses

@contextlib.contextmanager
def tqdm_joblib(tqdm_object):
    """Context manager to patch joblib to report into tqdm progress bar given as argument"""
    # reference: https://stackoverflow.com/questions/24983493/tracking-progress-of-joblib-parallel-execution
    class TqdmBatchCompletionCallback(joblib.parallel.BatchCompletionCallBack):
        def __call__(self, *args, **kwargs):
            tqdm_object.update(n=self.batch_size)
            return super().__call__(*args, **kwargs)

    old_batch_callback = joblib.parallel.BatchCompletionCallBack
    joblib.parallel.BatchCompletionCallBack = TqdmBatchCompletionCallback
    try:
        yield tqdm_object
    finally:
        joblib.parallel.BatchCompletionCallBack = old_batch_callback
        tqdm_object.close()

def get_airmass(all_solar_zenith):
    airmass = []
    for zenith in all_solar_zenith:
        airmass.append(pvlib.atmosphere.get_relative_airmass(zenith))
    return airmass


if __name__ == '__main__':
    result_bin, point_grid, gmlids, sunpos = read_result(CFG)
    print("Reading completed")
    latitude = CFG['study_area']['lat']
    longitude = CFG['study_area']['long']

    merged_result = np.hstack((result_bin, point_grid))
    # merged_result[:, -6:-3] are the normal vectors
    # merged_result[:, -3:-1] are the gmlids

    print("aggregating shadow results by gmlid...")
    result_array, unique_gmlids = aggregate_shadow(gmlids, merged_result)
    # result_array[:, -6:-3] are xyz coordinates
    # result_array[:, -3:] are the normal vectors

    print("Obtaining and processing tmy data...")
    all_ghi, all_dni, all_dhi, all_solar_zenith, all_solar_azimuth, all_dni_extra = obtain_tmy(latitude, longitude, sunpos)
    all_airmass = get_airmass(all_solar_zenith)


    print("Calculating irradiance in parallel with {} threads...".format(CFG['num_threads']))
    model = CFG['irradiance_model']
    print("using " + model+ " model")
    directs, diffuses = calculate_irradiance_parallel(result_array, 
                                                                    all_ghi, all_dni, all_dhi, 
                                                                    all_solar_zenith, all_solar_azimuth, all_dni_extra, all_airmass, model)

    # directs, diffuses have the same shape
    shadow_arr = result_array[:, :-6]
    total_irradiance = directs * shadow_arr  + diffuses

    unique_gmlids_arr = np.array(unique_gmlids)

    save_path = os.path.join(CFG['study_area']['data_root'], CFG['shadow_calc']['output_folder_name'],
                                model+'_hourly_result.npz')


    np.savez(save_path, gmlids=unique_gmlids_arr, hourly_irradiance=total_irradiance)