#!/usr/bin/env python3
"""
PC-side Temperature Monitor and MQTT Publisher
------------------------------------------------
This script auto-detects the connected Arduino, reads temperature values from Serial,
displays them in real-time, and publishes them to the MQTT broker.
It includes a built-in simulation mode if no physical Arduino is connected.
"""

import sys
import time
import json
import random
import serial
import serial.tools.list_ports
import paho.mqtt.client as mqtt

# ==========================================
# CONFIGURATION
# ==========================================
MQTT_BROKER = "broker.benax.rw"
MQTT_PORT = 1883
MQTT_TOPIC = "uwase/sonia/temperature"
BAUD_RATE = 9600
RECONNECT_DELAY = 5

def find_arduino_port():
    """Locate the serial port where the Arduino is connected."""
    ports = serial.tools.list_ports.comports()
    for port in ports:
        description = port.description.lower()
        hwid = port.hwid.lower()
        # Look for Arduino common identifiers
        if "arduino" in description or "ch340" in description or "usb" in description:
            print(f"[*] Auto-detected Arduino on: {port.device} ({port.description})")
            return port.device
    
    # Return first available port if any, else None
    if ports:
        print(f"[!] No explicit Arduino driver found. Using first available port: {ports[0].device}")
        return ports[0].device
    return None

def on_connect(client, userdata, flags, rc):
    """Callback for when the MQTT client connects to the broker."""
    if rc == 0:
        print(f"[+] Connected successfully to MQTT Broker: {MQTT_BROKER} (VPS Broker)")
    else:
        print(f"[-] Connection failed to MQTT Broker. Return code: {rc}")

def run_simulation(mqtt_client):
    """Generate mock temperature data if no hardware is present."""
    print("[*] Starting in SIMULATION Mode. Generating mock temperature data...")
    temp = 24.5
    try:
        while True:
            # Random walk for temperature simulation
            temp += random.uniform(-0.5, 0.5)
            temp = round(max(15.0, min(40.0, temp)), 1)
            
            payload = {
                "temperature": temp,
                "timestamp": int(time.time()),
                "mode": "simulated"
            }
            
            payload_str = json.dumps(payload)
            print(f"[Real-time View] Temp: {temp:.1f} °C (Simulated)")
            
            # Publish to broker
            mqtt_client.publish(MQTT_TOPIC, payload_str)
            time.sleep(2)
    except KeyboardInterrupt:
        print("\n[*] Exiting simulation...")

def run_real_hardware(port, mqtt_client):
    """Read data from the serial port and publish to MQTT."""
    print(f"[*] Attempting to connect to serial port {port} @ {BAUD_RATE} baud...")
    try:
        ser = serial.Serial(port, BAUD_RATE, timeout=2)
        # Flush input buffer to discard old/incomplete data
        ser.reset_input_buffer()
        print("[+] Serial port connected successfully.")
        
        while True:
            if ser.in_waiting > 0:
                line = ser.readline().decode('utf-8', errors='ignore').strip()
                if line:
                    try:
                        # Attempt to parse the temperature string
                        temp = float(line)
                        payload = {
                            "temperature": temp,
                            "timestamp": int(time.time()),
                            "mode": "hardware"
                        }
                        payload_str = json.dumps(payload)
                        print(f"[Real-time View] Temp: {temp:.1f} °C")
                        
                        # Publish to broker
                        mqtt_client.publish(MQTT_TOPIC, payload_str)
                    except ValueError:
                        # Print raw line if not a float (e.g. debugging messages)
                        print(f"[Arduino Output] {line}")
            time.sleep(0.1)
            
    except serial.SerialException as e:
        print(f"[-] Serial error: {e}")
    except KeyboardInterrupt:
        print("\n[*] Exiting monitor...")
    finally:
        if 'ser' in locals() and ser.is_open:
            ser.close()
            print("[*] Serial port closed.")

def main():
    print("==================================================")
    print("      Temperature Monitoring & MQTT Publisher     ")
    print("      Candidate: UWASE Sonia (SPE Trade Code)    ")
    print("==================================================")
    
    # Initialize MQTT client
    client = mqtt.Client()
    client.on_connect = on_connect
    
    try:
        client.connect(MQTT_BROKER, MQTT_PORT, keepalive=60)
        client.loop_start()
    except Exception as e:
        print(f"[-] Could not connect to MQTT Broker: {e}")
        print("[!] Client will run in offline terminal-only fallback mode.")
    
    # Detect port
    port = find_arduino_port()
    
    if port:
        run_real_hardware(port, client)
    else:
        print("[!] No active serial port or Arduino detected.")
        run_simulation(client)
        
    client.loop_stop()
    client.disconnect()
    print("[*] Disconnected from MQTT Broker. Goodbye!")

if __name__ == "__main__":
    main()
