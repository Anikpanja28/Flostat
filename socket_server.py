import socket
import threading
import sys
import sqlite3
import time

# Global variable to store the current leader
current_leader = None
leader_lock = threading.Lock()  # For thread-safe leader updates

# List of connected servers (for leader election and status sync)
connected_servers = []

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

def query_leader_from_servers():
    """Query other servers to see if a leader exists."""
    global current_leader
    for server_ip, server_port in connected_servers:
        try:
            print(f"Querying leader from {server_ip}:{server_port}...")
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            s.settimeout(2)  # Set timeout for querying
            s.connect((server_ip, server_port))
            s.sendall("WHOIS_LEADER".encode())
            response = s.recv(1024).decode().strip()
            s.close()
            if response.startswith("LEADER"):
                leader_info = response.split(":")[1] + ":" + response.split(":")[2]
                with leader_lock:
                    current_leader = leader_info  # Set the leader globally
                return current_leader  # Leader found
        except Exception as e:
            print(f"Failed to query leader from {server_ip}:{server_port}: {e}")
    return None

def broadcast_leader_status(my_ip, my_port):
    """Broadcast that this server is the leader."""
    global connected_servers
    with leader_lock:
        for server_ip, server_port in connected_servers:
            try:
                s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                s.settimeout(2)  # Set timeout for broadcasting
                s.connect((server_ip, server_port))
                s.sendall(f"LEADER:{my_ip}:{my_port}\n".encode())
                s.close()
            except Exception as e:
                print(f"Failed to broadcast leader status to {server_ip}:{server_port} - {e}")

def elect_leader(my_ip, my_port):
    """Leader election process, running in its own thread."""
    global current_leader
    while True:
        # Query other servers to check if a leader already exists
        leader = query_leader_from_servers()
        if leader:
            with leader_lock:
                current_leader = leader
            print(f"Recognized existing leader: {leader}")
        else:
            # No leader found, elect self
            with leader_lock:
                current_leader = f"{my_ip}:{my_port}"
            print(f"Elected self as leader: {current_leader}")
            broadcast_leader_status(my_ip, my_port)
        time.sleep(10)  # Check every 10 seconds

def handle_client(client_socket):
    """Handle requests from clients or other servers."""
    global current_leader
    try:
        while True:
            msg = client_socket.recv(1024).decode('utf-8').strip()
            if not msg:
                break

            print(f"Received message: {msg}")

            if msg == "WHOIS_LEADER":
                # Respond with the current leader
                with leader_lock:
                    if current_leader:
                        client_socket.send(f"LEADER:{current_leader}\n".encode())
                    else:
                        client_socket.send("LEADER:None\n".encode())
            elif msg.startswith("LEADER"):
                # Update the current leader if a new leader message is received
                leader_info = msg.split(":")[1] + ":" + msg.split(":")[2]
                with leader_lock:
                    current_leader = leader_info
                print(f"Updated leader to {current_leader}")
            elif msg.startswith("SENSOR_DATA:"):
                try:
                    sensor_value = int(msg.split(":")[1])
                    store_data_in_db(sensor_value)
                    client_socket.send("Data stored.\n".encode())
                except (IndexError, ValueError) as e:
                    client_socket.send("Invalid data format.\n".encode())
            else:
                client_socket.send("Unknown message.\n".encode())
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
    my_port = int(sys.argv[1]) if len(sys.argv) > 1 else 5001

    # Add other servers to connect with for leader election
    connected_servers = [("192.168.15.40", 5002), ("192.168.15.40", 5003), ("192.168.15.40", 5004), ("192.168.15.40", 5005)]

    # Initialize local database
    init_db()

    # Start leader election in a separate thread
    threading.Thread(target=elect_leader, args=(my_ip, my_port), daemon=True).start()

    # Start the main server to handle clients (including ESP32)
    start_server(my_ip, my_port)
