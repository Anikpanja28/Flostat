from flask import Flask, request, jsonify, render_template
import sqlite3
import requests
from datetime import datetime

app = Flask(__name__)

# List of other server IPs to synchronize with
OTHER_SERVERS = ['http://192.168.1.7:5003', 'http://192.168.1.7:5002']  # Replace with actual server IPs

# Create a local SQLite database (if using MySQL, set up accordingly)
def create_db():
    conn = sqlite3.connect('water_system_5001.db')
    c = conn.cursor()
    # Create table for water levels (this remains unchanged)
    c.execute('''
        CREATE TABLE IF NOT EXISTS water_levels (
            tank_id INTEGER PRIMARY KEY,
            water_level INTEGER
        )
    ''')
    # Create table for valve control logs (new design with AUTOINCREMENT ID)
    c.execute('''
        CREATE TABLE IF NOT EXISTS valve_control (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            tank_id INTEGER,
            valve_state TEXT,
            timestamp TEXT,
            server TEXT
        )
    ''')
    conn.commit()
    conn.close()

# Insert new valve control entry into the local database, including timestamp and server
def insert_valve_control(tank_id, valve_state, server):
    timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")  # Get current timestamp
    conn = sqlite3.connect('water_system_5001.db')
    c = conn.cursor()
    c.execute('''
        INSERT INTO valve_control (tank_id, valve_state, timestamp, server)
        VALUES (?, ?, ?, ?)
    ''', (tank_id, valve_state, timestamp, server))
    conn.commit()
    conn.close()

# Insert or update water level data in the local database
def update_water_level(tank_id, water_level):
    conn = sqlite3.connect('water_system_5001.db')
    c = conn.cursor()
    c.execute('''
        INSERT INTO water_levels (tank_id, water_level) VALUES (?, ?)
        ON CONFLICT(tank_id) DO UPDATE SET water_level = ?
    ''', (tank_id, water_level, water_level))
    conn.commit()
    conn.close()

# Fetch the latest data from all other servers during startup
def synchronize_with_other_servers():
    print("Attempting to synchronize with other servers...")
    
    for server in OTHER_SERVERS:
        try:
            # Fetch the latest water levels from the other server
            response = requests.get(f'{server}/api/get_all_water_levels', timeout=5)
            if response.status_code == 200:
                water_levels = response.json().get('water_levels', [])
                for tank in water_levels:
                    update_water_level(tank['tank_id'], tank['water_level'])
                print(f"Synchronized water levels from {server}")
            else:
                print(f"Failed to fetch water levels from {server}: Status Code {response.status_code}")

            # Fetch the latest valve states from the other server
            response = requests.get(f'{server}/api/get_all_valve_states', timeout=5)
            if response.status_code == 200:
                valve_states = response.json().get('valve_states', [])
                for valve in valve_states:
                    insert_valve_control(valve['tank_id'], valve['valve_state'], server)
                print(f"Synchronized valve states from {server}")
            else:
                print(f"Failed to fetch valve states from {server}: Status Code {response.status_code}")
        
        except requests.exceptions.RequestException as e:
            print(f"Error synchronizing with {server}: {e}")

# Serve the "dashboard.html" page when root URL is accessed
@app.route('/')
def index():
    return render_template('dashboard.html')

# Endpoint to fetch all water levels (used for synchronization)
@app.route('/api/get_all_water_levels', methods=['GET'])
def get_all_water_levels():
    conn = sqlite3.connect('water_system_5001.db')
    c = conn.cursor()
    c.execute('SELECT * FROM water_levels')
    water_levels = [{'tank_id': row[0], 'water_level': row[1]} for row in c.fetchall()]
    conn.close()
    return jsonify({"status": "success", "water_levels": water_levels})

# Endpoint to fetch all valve states (used for synchronization)
@app.route('/api/get_all_valve_states', methods=['GET'])
def get_all_valve_states():
    conn = sqlite3.connect('water_system_5001.db')
    c = conn.cursor()
    c.execute('SELECT * FROM valve_control')
    valve_states = [{'tank_id': row[1], 'valve_state': row[2], 'timestamp': row[3], 'server': row[4]} for row in c.fetchall()]
    conn.close()
    return jsonify({"status": "success", "valve_states": valve_states})

# Endpoint to receive water level data from ESP32
@app.route('/api/water_level', methods=['POST'])
def water_level():
    data = request.json
    tank_id = data['tank_id']
    water_level = data['water_level']
    update_water_level(tank_id, water_level)

    # Sync data with other servers
    sync_with_other_servers(tank_id, water_level, 'water_level')

    return jsonify({"status": "success", "message": "Water level updated"})

# Endpoint to control the valve
@app.route('/api/control_valve', methods=['POST'])
def control_valve():
    data = request.json
    tank_id = data['tank_id']
    command = data['command']  # "open" or "close"
    server = request.remote_addr  # Get the server IP that made the change
    insert_valve_control(tank_id, command, server)

    # Sync the valve command with other servers
    sync_with_other_servers(tank_id, command, 'valve_control', server)

    return jsonify({"status": "success", "message": f"Valve {command} command sent."})

# Sync data with other servers on the local network
def sync_with_other_servers(tank_id, data, data_type, server=None):
    for server_url in OTHER_SERVERS:
        try:
            if data_type == 'water_level':
                requests.post(f'{server_url}/api/sync_water_level', json={'tank_id': tank_id, 'water_level': data}, timeout=5)
            elif data_type == 'valve_control':
                requests.post(f'{server_url}/api/sync_valve_control', json={'tank_id': tank_id, 'command': data, 'server': server}, timeout=5)
        except requests.exceptions.RequestException as e:
            print(f"Error syncing with {server_url}: {e}")

# Endpoint to sync water level data from another server
@app.route('/api/sync_water_level', methods=['POST'])
def sync_water_level():
    data = request.json
    tank_id = data['tank_id']
    water_level = data['water_level']
    update_water_level(tank_id, water_level)
    return jsonify({"status": "success", "message": "Water level synchronized"})

@app.route('/api/sync_valve_control', methods=['POST'])
def sync_valve_control():
    data = request.json
    tank_id = data['tank_id']
    command = data['command']
    server = data.get('server', 'unknown')  # Get the server from the request, or use 'unknown' as a default
    insert_valve_control(tank_id, command, server)
    return jsonify({"status": "success", "message": "Valve control synchronized"})

@app.route('/api/get_valve_state/<int:tank_id>', methods=['GET'])
def get_valve_state(tank_id):
    conn = sqlite3.connect('water_system_5001.db')
    c = conn.cursor()

    # Fetch the most recent valve state for the given tank_id
    c.execute('''
        SELECT valve_state FROM valve_control
        WHERE tank_id = ?
        ORDER BY id DESC LIMIT 1
    ''', (tank_id,))
    row = c.fetchone()
    conn.close()

    if row:
        return jsonify({"status": "success", "valve_state": row[0]})
    else:
        return jsonify({"status": "error", "message": "No valve state found"}), 404

if __name__ == "__main__":
    create_db()
    synchronize_with_other_servers()  # Attempt to synchronize with other servers before starting
    app.run(host="0.0.0.0", port=5001)
