from pvlib import solarposition
import pandas as pd
import numpy as np
import pytz
import json
from datetime import datetime
import os

def main():
    with open('config.json', 'r') as file:
        CONFIG = json.load(file)
    
    cfg = CONFIG["study_area"]
    tz = cfg["timezone"]
    lat, lon = cfg["lat"], cfg["long"]
    times = pd.date_range(cfg["start_time"], cfg["end_time"], freq=cfg["frequency"], tz=tz)
    solpos = solarposition.get_solarposition(times, lat, lon)
    # remove nighttime
    solpos = solpos.loc[solpos['apparent_elevation'] > 0, :]

    current_time = datetime.now()

    # Format the datetime to the specified format
    formatted_time = current_time.strftime("%Y%m%d_%H%M%S")

    # Concatenate with the "output" string
    result_string = "output_" + formatted_time

    result_dir = os.path.join(cfg["data_root"], result_string)
    intermediate_dir = os.path.join(result_dir, "intermediate")

    CONFIG["shadow_calc"]["output_folder_name"] = result_string

    os.makedirs(intermediate_dir, exist_ok=True)
    solar_path = os.path.join(intermediate_dir, "sun_pos.csv")
    solpos.to_csv(solar_path)

    cfg_path = os.path.join(result_dir, "config.json")
    with open(cfg_path, 'w') as file:
        json.dump(CONFIG, file, indent=4)

    with open('config.json', 'w') as file:
        json.dump(CONFIG, file, indent=4)


    # Print the result
    print("saving solar pos to: ",result_string)


if __name__ == '__main__':
        main()