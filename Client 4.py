import requests
import json

# Example: Send water level data
def send_water_level(tank_name, water_level):
    url = 'http://localhost:5000/update-water-level'
    data = {
        'tank_name': tank_name,
        'water_level': water_level
    }
    response = requests.post(url, json=data)
    print(response.json())

# Example: Control the valve
def control_valve(tank_name, valve_status):
    url = 'http://localhost:5000/control-valve'
    data = {
        'tank_name': tank_name,
        'valve_status': valve_status  # 'open' or 'close'
    }
    response = requests.post(url, json=data)
    print(response.json())

# Example: Get water level data
def get_water_level(tank_name):
    url = f'http://localhost:5000/get-water-level/{tank_name}'
    response = requests.get(url)
    print(response.json())

# Example: Get valve status
def get_valve_status(tank_name):
    url = f'http://localhost:5000/get-valve-status/{tank_name}'
    response = requests.get(url)
    print(response.json())

# Simulate sending data from ESP32
send_water_level('Tank1', 80)
control_valve('Tank1', 'open')
get_water_level('Tank1')
get_valve_status('Tank1')
