# Shadow and Solar Irradiance Calculation with Semantic 3D City Models
This repository contains the implementation of a Solar Irradiance simulation tool for 3D City Models. The model considers the shadowing effect in urban areas to estimate solar irradiance. 



Please cite the following paper if you find it helpful.

```
@inproceedings{xu2023shadowing,
  title={Shadowing Calculation on Urban Areas from Semantic 3D City Models},
  author={Xu, Longxiang and Le{\'o}n-S{\'a}nchez, Camilo and Agugiaro, Giorgio and Stoter, Jantien},
  booktitle={International 3D GeoInfo Conference},
  pages={31--47},
  year={2023},
  organization={Springer}
}
```







# Environment setup

Make sure cmake and conda are installed.

If not.

## Cmake

- for linux

  ```
  sudo apt update
  sudo apt-get install build-essential libssl-dev
  ```

- for windows

  Use **windows** **installer** from [Download CMake](https://cmake.org/download/) under **Binary distributions**



## Conda

Install conda or miniconda from [Installing conda](https://conda.io/projects/conda/en/latest/user-guide/install/index.html#regular-installation)



## Check if correctly installed

```
cmake --version
conda --version
```



## Create conda environment

```
conda create -y --name shadow python=3.9
conda activate shadow
pip install -q pvlib numpy pytz pandas cjio==0.9.0 joblib triangle tqdm
```



## Build project

In the project folder

- linux

  ```
  mkdir build
  cd build
  cmake -DCMAKE_BUILD_TYPE=Release ..
  make
  cd ..
  ```

- windows

  ```
  mkdir build
  cd build
  cmake -DCMAKE_BUILD_TYPE=Release ..
  cmake --build . --config Release
  cd ..
  ```
  
  



# Run simulation

## One line mode

Make sure conda environment is activated, and project is built.

- linux

  ```
  ./run_simulation.sh
  ```

  

- windows

  ```
  run_simulation.bat
  ```




## Step-by-step guide

### Data preparation

The supported file format for 3D city model is [cityjson](https://www.cityjson.org/). It can also work with obj file format, but the functionality is incomlete.

First download one or multiple tiles from [3DBAG](https://3dbag.nl/en/download) dataset, which contains almost all the buildings in the Netherlands.

Next, create a folder under data directory. The folder structure should be like this:

```bash
data
	- Delft   # Create your folder for the study area
		- citymodel
			- neighbouring_tiles # Tiles working as buffering tiles
				- Delft_1_2.city.json
				- ....
			- target_tiles # Tiles which you want to sample points and run simulation
				- Delft_1_1.city.json
				- ...
```

The shadow calculation program will sample points **only** on **target_tiles**. And the determination of shadowing effect will consider **all the geometries** in **target_tiles** and **neighbouring_tiles**



### Configure the simulation

```json
{
    "study_area": {
        "timezone": "Europe/Amsterdam",
        "lat": 53.21562567169522,
        "long": 6.567723598534742,
        "start_time": "2023-01-01 00:00:00",
        "end_time": "2023-01-02 00:00:00",
        "data_root": "./data/Delft",
        "frequency": "h",
        "lod": 2.2
    },
    "shadow_calc": {
        "save_result_pc": false,
        "target_surfaces": [
            "WallSurface",
            "RoofSurface"
        ],
        "output_folder_name": "output_20240620_130351"
    },
    "num_threads": 20,
    "irradiance_model": "isotropic",
    "epw_file": null
}
```

For your own project: following configuration needs to be set accordingly:

- timezone
- lat
- long
- start_time
- end_time
- frequency
  - Default is 1 hour. Other available frequency: [Time series / date functionality — pandas 2.2.0 documentation (pydata.org)](https://pandas.pydata.org/docs/user_guide/timeseries.html#timeseries-offset-aliases)
- lod
  - The level of detail you want for the geometries. Available are 1.2, 1.3, and 2.2
  - Refer to [General Concepts - 3DBAG](https://docs.3dbag.nl/en/schema/concepts/)
- save_result_pc
  - Set to ***true*** if you wish to save the point grid result for **every calculation moment**. **The points that are not shadowed will be saved for each moment**. Not recommended if calculation moments are large.
- target_surfaces
  - The surface type for which you wish to sample points and calculate if they are shadowed. 
  - Recommended for using "WallSurface" and "RoofSurface". Others available. Please refer OGC CityGML standards.
- output_folder_name
  - This field will be populated automatically
- irradiance_model
  - Available are "isotropic", "perez" etc.
  - Refer to [pvlib.irradiance.get_total_irradiance — pvlib python 0.10.5 documentation (pvlib-python.readthedocs.io)](https://pvlib-python.readthedocs.io/en/stable/reference/generated/pvlib.irradiance.get_total_irradiance.html#model)
- num_threads
  - According to your device
- epw_file
  - If you have your own weather data, you can specify here. Otherwise the model will fetch the tmy3 file automatically



### (One line mode)

Now you have prepared everything, you can still use the one line mode to run the simulation



### Process the cityjson

Following is the triangulation and identifier assignment to the triangulated surfaces.

```
python src/prepare_json.py
```

The location of the processed files will be printed.



### Obtain solar positions

```
python3 src/solar_pos.py
```

The location of the solar position file will be printed.



### Shadow calculation

- linux

  ```
  build/shadow_calculation
  ```

  

- windows

  ```
  build\Release\shadow_calculation.exe
  ```



Location of the results will be printed.



### Solar irradiance calculation

```
python src\calculate_irradiance.py
```



Result will be a npz file.



# Interpretation of result

## Shadow result

saved to a binary file, together with a csv file(recording the identifier with the same order). If there are 5000 sample points, then the csv file should have 5000 rows.



To read it into a numpy array:

```python
with open(bin_file, "rb") as file:
    result_bin = np.fromfile(file, dtype=np.int32)
    num_rows = result_bin[0]
    num_cols = result_bin[1]  
    # Remove the first two elements (number of rows and columns)
    result_bin = result_bin[2:]

    result_bin = result_bin.reshape((num_rows, num_cols))
```

Number of rows represent the number of sample points, while number of column represent number of timestamps.



## Irradiance result

```
result = np.load('irradiance_result.npz', allow_pickle=True)
gmlid_list = result['gmlids']
irradiance_data = result['hourly_irradiance']
```

Here the values are aggregated for each triangle surface. If there are 3000 surfaces, then there should have 3000 rows.
