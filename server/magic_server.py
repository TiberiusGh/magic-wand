import tinytuya
import serial
import time
from datetime import datetime, time as dt_time
import os
import sys

# --- Configuration ---
os.environ['TZ'] = 'Europe/Stockholm'
time.tzset()

SERIAL_PORT = '/dev/ttyUSB0' 
BAUD_RATE = 115200
TOGGLE_COOLDOWN = 0.5

# Check schedule every X seconds inside the loop
SCHEDULE_CHECK_INTERVAL = 30 

ON_WINDOWS = [
    (6, 0, 9, 30),
    (17, 0, 22, 30)
]

def log(msg):
    """Helper to print with precise timestamps for debugging."""
    timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    print(f"[{timestamp}] {msg}", flush=True)

log("Loading Magic Server (Optimized)...")

# --- Connect to Plug ---
try:
    # Load secrets from Environment Variables
    t_id = os.getenv('TUYA_DEVICE_ID')
    t_ip = os.getenv('TUYA_IP')
    t_key = os.getenv('TUYA_LOCAL_KEY')
    t_ver = float(os.getenv('TUYA_VERSION', '3.4')) # Default to 3.4 if missing
    
    # Check if we have what we need
    if not all([t_id, t_ip, t_key]):
        raise ValueError("Missing Tuya environment variables. Check .env file.")

    plug = tinytuya.OutletDevice(t_id, t_ip, t_key)
    plug.set_version(t_ver)
    plug.set_socketPersistent(True)
    
    log(f"Target IP: {t_ip}") # We can't log the name anymore unless we add a TUYA_NAME var
except Exception as e:
    log(f"Config Error: {e}")
    sys.exit(1)

# Global State Tracker
is_plug_on = False

def sync_state():
    """Forces a read from the device to update local memory."""
    global is_plug_on
    try:
        status = plug.status()
        if 'dps' in status:
            is_plug_on = status['dps'].get('1', False)
    except:
        log("Warning: Could not sync state")

# --- Main Setup ---
try:
    ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=0.05)
    ser.dtr = False 
    ser.rts = False
    log(f"✓ Connected to Radio Dongle")
except Exception as e:
    log(f"❌ Radio Error: {e}")
    sys.exit(1)

def get_schedule_target():
    """Returns True if we are currently in an ON window, False otherwise."""
    now = datetime.now().time()
    for start_h, start_m, end_h, end_m in ON_WINDOWS:
        if dt_time(start_h, start_m) <= now <= dt_time(end_h, end_m):
            return True
    return False

# 1. Startup Logic: Enforce Schedule Immediately
log("Performing Startup Sync...")
sync_state() # Update 'is_plug_on' with reality
target_state = get_schedule_target() # Get what reality SHOULD be

if target_state and not is_plug_on:
    log("  --> ON (Startup Enforcement)")
    plug.turn_on()
    is_plug_on = True
elif not target_state and is_plug_on:
    log("  --> OFF (Startup Enforcement)")
    plug.turn_off()
    is_plug_on = False
else:
    log(f"  --> State is correct ({'ON' if is_plug_on else 'OFF'})")

log(f"System Ready. Current State: {'ON' if is_plug_on else 'OFF'}")

# Trackers for the loop
last_toggle_time = 0
last_schedule_check = 0
last_schedule_state = target_state 

# --- Main Loop ---
log("Waiting for Magic or Schedule triggers...")

while True:
    try:
        current_time = time.time()

        # --- A. CHECK SCHEDULE ---
        if (current_time - last_schedule_check) > SCHEDULE_CHECK_INTERVAL:
            last_schedule_check = current_time
            should_be_on = get_schedule_target()
            
            # Only act if the schedule requirement has CHANGED (e.g., clocked over to 06:00)
            if should_be_on != last_schedule_state:
                log(f"⏰ Schedule Transition: {'OFF->ON' if should_be_on else 'ON->OFF'}")
                if should_be_on:
                    plug.turn_on()
                    is_plug_on = True
                    log("  --> ON (Schedule Trigger)")
                else:
                    plug.turn_off()
                    is_plug_on = False
                    log("  --> OFF (Schedule Trigger)")
                
                last_schedule_state = should_be_on

        # --- B. CHECK WAND (SERIAL) ---
        if ser.in_waiting > 0:
            try:
                line = ser.readline().decode('utf-8').strip()
                
                if "TOGGLE" in line:
                    if (current_time - last_toggle_time) > TOGGLE_COOLDOWN:
                        log(f"⚡ WAND WAVE DETECTED!")
                        last_toggle_time = current_time
                        
                        try:
                            if is_plug_on:
                                plug.turn_off()
                                is_plug_on = False
                                log("  --> OFF (Fast)")
                            else:
                                plug.turn_on()
                                is_plug_on = True
                                log("  --> ON (Fast)")
                        except Exception as e:
                            log(f"  Command Failed: {e}. Re-syncing...")
                            plug.set_socketPersistent(True)
                            sync_state()

            except Exception:
                pass
                
    except KeyboardInterrupt:
        log("Stopping server...")
        break
    except Exception as e:
        log(f"Main Loop Error: {e}")
        time.sleep(1)

