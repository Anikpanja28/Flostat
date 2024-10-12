import socket
import threading
import sys
import sqlite3

# Global variable to store the current leader
current_leader = None

# Database file name for local storage
DB_FILE = "server_data.db"

# Initialize the SQLite database and table
def init_db():
    conn = sqlite3.connect(DB_FILE)
    cursor = conn.cursor()
    cursor.execute('''CREATE TABLE IF NOT EXISTS SensorData (
                        id INTEGER PRIMARY KEY AUTOINCREMENT,
                        timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
                        sensor_value INTEGER)''')
    conn.commit()
    cursor.close()
    conn.close()
    print("Database initialized.")

# Store data locally in the SQLite database
def store_data_in_db(sensor_value):
    conn = sqlite3.connect(DB_FILE)
    cursor = conn.cursor()
    cursor.execute("INSERT INTO SensorData (sensor_value) VALUES (?)", (sensor_value,))
    conn.commit()
    cursor.close()
    conn.close()
    print(f"Sensor data {sensor_value} stored in database.")

def handle_client(client_socket):
    """Handle requests from clients or other servers."""
    global current_leader
    try:
        while True:
            msg = client_socket.recv(1024).decode('utf-8').strip()
            if not msg:
                break

            print(f"Received message: {msg}")  # Log exactly what was received

            if msg == "WHOIS_LEADER":
                # Respond with the current leader
                current_leader = "192.168.15.40:5001"
                client_socket.send(f"LEADER:{current_leader}\n".encode())
            elif msg.startswith("SENSOR_DATA:"):
                try:
                    # Extract the sensor value from the message
                    sensor_value = int(msg.split(":")[1])
                    print(f"Received sensor data: {sensor_value}")
                    store_data_in_db(sensor_value)
                    client_socket.send("Data stored.\n".encode())
                except (IndexError, ValueError) as e:
                    # Handle cases where the sensor data is missing or not a valid integer
                    print(f"Error parsing sensor data: {e}")
                    client_socket.send("Invalid data format.\n".encode())
            else:
                print(f"Unknown message: {msg}")

    except Exception as e:
        print(f"Error handling client: {e}")
    finally:
        client_socket.close()

def start_server(my_ip, my_port):
    """Start the server and listen for connections."""
    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_socket.bind((my_ip, my_port))
    server_socket.listen(5)
    print(f"Server running on {my_ip}:{my_port}")

    while True:
        client_sock, addr = server_socket.accept()
        print(f"Accepted connection from {addr}")
        threading.Thread(target=handle_client, args=(client_sock,)).start()

if __name__ == "__main__":
    my_ip = "192.168.15.40"
    my_port = 5002  # Simplified to run only on port 5001

    # Initialize local database
    init_db()

    # Start server on port 5001
    start_server(my_ip, my_port)
