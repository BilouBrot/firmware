#!/usr/bin/env python3
"""
Automated Log Capture and Cleanup Script - Multi-Load Test Mode
Combines serial monitoring and log cleanup with load test categorization:
1. Connects to serial port and captures data
2. Listens until it gets a complete log from LOGSTART to LOGEND
3. Automatically cleans up the captured log
4. Categorizes LOG entries by timestamp into load tests (configurable via constants)
5. Saves organized logs in experiment folder structure

Load test timing is configured via constants at the top of this file.
"""

import serial
import serial.tools.list_ports
import time
import re
import os
from datetime import datetime
from typing import Optional, List, Tuple

# Load Test Configuration Constants (in hours)
LOWLOAD_START = 1.0      # Start time for LowLoad phase
LOWLOAD_DURATION = 7.0   # Duration of LowLoad phase in hours
MEDIUMLOAD_DURATION = 3.0  # Duration of MediumLoad phase in hours
HIGHLOAD_DURATION = 3.0   # Duration of HighLoad phase in hours
BREAK_DURATION = 0.5     # Break duration between phases in hours (30 minutes)

# Calculated time ranges (automatically computed from above constants)
LOWLOAD_END = LOWLOAD_START + LOWLOAD_DURATION
MEDIUMLOAD_START = LOWLOAD_END + BREAK_DURATION
MEDIUMLOAD_END = MEDIUMLOAD_START + MEDIUMLOAD_DURATION
HIGHLOAD_START = MEDIUMLOAD_END + BREAK_DURATION
HIGHLOAD_END = HIGHLOAD_START + HIGHLOAD_DURATION

def strip_ansi_codes(text: str) -> str:
    """Remove ANSI color codes from text"""
    ansi_escape = re.compile(r'\x1B(?:[@-Z\\-_]|\[[0-?]*[ -/]*[@-~])')
    return ansi_escape.sub('', text)

def format_time_range(start_hours: float, end_hours: float) -> str:
    """Format time range from decimal hours to HH:MM format"""
    def hours_to_time(h):
        hours = int(h)
        minutes = int((h - hours) * 60)
        return f"{hours:02d}:{minutes:02d}"
    
    return f"{hours_to_time(start_hours)}-{hours_to_time(end_hours)}"

def get_load_test_info() -> str:
    """Generate load test information string using current constants"""
    lowload_range = format_time_range(LOWLOAD_START, LOWLOAD_END)
    mediumload_range = format_time_range(MEDIUMLOAD_START, MEDIUMLOAD_END)
    highload_range = format_time_range(HIGHLOAD_START, HIGHLOAD_END)
    
    return f"""   - LowLoad: {lowload_range} ({LOWLOAD_DURATION} hours)
   - MediumLoad: {mediumload_range} ({MEDIUMLOAD_DURATION} hours)
   - HighLoad: {highload_range} ({HIGHLOAD_DURATION} hours)"""

def list_serial_ports() -> List[str]:
    """List all available serial ports"""
    ports = serial.tools.list_ports.comports()
    return [port.device for port in ports]

def find_usbmodem_ports() -> List[str]:
    """Find all serial ports that start with /dev/cu.usbmodem"""
    all_ports = list_serial_ports()
    usbmodem_ports = [port for port in all_ports if port.startswith('/dev/cu.usbmodem')]
    return usbmodem_ports

def capture_serial_log(port: str, baudrate: int = 115200, timeout: float = 1.0, 
                      max_wait_time: int = 120) -> Optional[Tuple[List[str], str]]:
    """
    Capture serial data until we get a complete log from LOGSTART to LOGEND
    
    Args:
        port: Serial port to connect to
        baudrate: Baud rate for serial connection
        timeout: Timeout for serial read operations
        max_wait_time: Maximum time to wait for complete log (seconds)
    
    Returns:
        Tuple of (captured_lines, node_id) or None if failed
    """
    try:
        print(f"Connecting to {port} at {baudrate} baud...")
        ser = serial.Serial(port, baudrate, timeout=timeout)
        print("Connected! Waiting for data...")
        
        captured_lines = []
        start_found = False
        end_found = False
        node_id = None
        start_time = time.time()
        
        while not end_found and (time.time() - start_time) < max_wait_time:
            try:
                line = ser.readline().decode('utf-8', errors='ignore')
                if line:
                    captured_lines.append(line)
                    clean_line = strip_ansi_codes(line)
                    
                    # Look for node ID if we haven't found it yet
                    if not node_id:
                        id_match = re.search(r'ID:(\d+)', clean_line)
                        if id_match:
                            node_id = id_match.group(1)
                            print(f"Detected Node ID: {node_id}")
                    
                    # Check for start marker
                    if '==LOGSTART==' in clean_line and not start_found:
                        start_found = True
                        print("Found LOGSTART marker - capturing log data...")
                    
                    # Check for end marker (only after start is found)
                    if start_found and '==LOGEND==' in clean_line:
                        end_found = True
                        print("Found LOGEND marker - log capture complete!")
                        break
                    
                    # Show progress for long captures
                    if len(captured_lines) % 100 == 0:
                        elapsed = time.time() - start_time
                        print(f"Captured {len(captured_lines)} lines in {elapsed:.1f}s...")
                        
            except serial.SerialTimeoutException:
                continue
            except UnicodeDecodeError:
                # Skip lines that can't be decoded
                continue
                
        ser.close()
        
        if not start_found:
            print("Error: Never found LOGSTART marker")
            return None
        
        if not end_found:
            print(f"Warning: LOGEND marker not found within {max_wait_time} seconds")
            print("Captured data might be incomplete")
        
        print(f"Capture completed: {len(captured_lines)} lines captured")
        return captured_lines, node_id
        
    except serial.SerialException as e:
        print(f"Serial connection error: {e}")
        return None
    except Exception as e:
        print(f"Unexpected error during capture: {e}")
        return None

def ms_to_hours_minutes(ms: int) -> float:
    """Convert milliseconds to hours as decimal (e.g., 1.5 = 1:30)"""
    return ms / (1000 * 60 * 60)

def categorize_log_by_timestamp(log_line: str) -> str:
    """
    Categorize a LOG line based on its timestamp
    
    Args:
        log_line: LOG line in format "LOG:timestamp,value1,value2,..."
    
    Returns:
        Category: "LowLoad", "MediumLoad", "HighLoad", or "Unknown"
    """
    # Extract timestamp from LOG line
    match = re.search(r'LOG:(\d+)', log_line)
    if not match:
        return "Unknown"
    
    timestamp_ms = int(match.group(1))
    hours = ms_to_hours_minutes(timestamp_ms)
    
    # Categorize based on time ranges using constants
    if LOWLOAD_START <= hours <= LOWLOAD_END:
        return "LowLoad"
    elif MEDIUMLOAD_START <= hours <= MEDIUMLOAD_END:
        return "MediumLoad"
    elif HIGHLOAD_START <= hours <= HIGHLOAD_END:
        return "HighLoad"
    else:
        return "Unknown"

def create_experiment_folders(experiment_name: str) -> dict:
    """
    Create folder structure for experiment
    
    Args:
        experiment_name: Name of the experiment
    
    Returns:
        Dictionary with folder paths
    """
    base_folder = experiment_name
    
    folders = {
        'base': base_folder,
        'raw': os.path.join(base_folder, 'raw'),
        'LowLoad': os.path.join(base_folder, 'LowLoad'),
        'MediumLoad': os.path.join(base_folder, 'MediumLoad'),
        'HighLoad': os.path.join(base_folder, 'HighLoad'),
        'Unknown': os.path.join(base_folder, 'Unknown')
    }
    
    # Create all folders
    for folder_type, folder_path in folders.items():
        try:
            os.makedirs(folder_path, exist_ok=True)
            print(f" Created folder: {folder_path}")
        except Exception as e:
            print(f" Error creating folder {folder_path}: {e}")
    
    return folders

def clean_and_categorize_log(lines: List[str]) -> dict:
    """
    Clean the captured log data and categorize by timestamp
    
    Args:
        lines: List of captured lines
    
    Returns:
        Dictionary with categorized cleaned lines
    """
    # Find the start and end markers
    start_index = None
    end_index = None
    
    for i, line in enumerate(lines):
        clean_line = strip_ansi_codes(line)
        if '==LOGSTART==' in clean_line:
            start_index = i
            break
    
    if start_index is None:
        print("Warning: ==LOGSTART== not found in captured data")
        return {}
    
    # Find ==LOGEND== after the start marker
    for i in range(start_index + 1, len(lines)):
        clean_line = strip_ansi_codes(lines[i])
        if '==LOGEND==' in clean_line:
            end_index = i
            break
    
    if end_index is None:
        print("Warning: ==LOGEND== not found in captured data")
        end_index = len(lines) - 1
    
    print(f"Processing log section from line {start_index + 1} to line {end_index + 1}")
    
    # Extract lines between markers and categorize
    log_section = lines[start_index:end_index + 1]
    categorized_lines = {
        'LowLoad': [],
        'MediumLoad': [],
        'HighLoad': [],
        'Unknown': [],
        'metadata': []  # For ID, MAC, LONG_NAME, SHORT_NAME
    }
    
    for line in log_section:
        # Strip ANSI codes and extract the actual content from the log line
        clean_line = strip_ansi_codes(line)
        
        # Look for different types of log entries
        if any(marker in clean_line for marker in ['ID:', 'LOG:', 'MAC:', 'LONG_NAME:', 'SHORT_NAME:']):
            if 'ID:' in clean_line:
                # Find the ID: part
                match = re.search(r'ID:(\d+)', clean_line)
                if match:
                    categorized_lines['metadata'].append(f"ID:{match.group(1)}\n")
            elif 'LOG:' in clean_line:
                # Find the LOG: part and categorize by timestamp
                match = re.search(r'LOG:([-+]?[0-9]*\.?[0-9]+(?:[eE][-+]?[0-9]+)?(?:,[-+]?[0-9]*\.?[0-9]+(?:[eE][-+]?[0-9]+)?)*)', clean_line)
                if match:
                    log_line = f"LOG:{match.group(1)}\n"
                    category = categorize_log_by_timestamp(log_line)
                    categorized_lines[category].append(log_line)
            elif 'MAC:' in clean_line:
                # Find the MAC: part
                match = re.search(r'MAC:([0-9A-Fa-f:.-]+)', clean_line)
                if match:
                    categorized_lines['metadata'].append(f"MAC:{match.group(1)}\n")
            elif 'LONG_NAME:' in clean_line:
                # Find the LONG_NAME: part
                match = re.search(r'LONG_NAME:([A-Za-z0-9_\-\s]+)', clean_line)
                if match:
                    categorized_lines['metadata'].append(f"LONG_NAME:{match.group(1).strip()}\n")
            elif 'SHORT_NAME:' in clean_line:
                # Find the SHORT_NAME: part
                match = re.search(r'SHORT_NAME:([A-Za-z0-9_\-]+)', clean_line)
                if match:
                    categorized_lines['metadata'].append(f"SHORT_NAME:{match.group(1)}\n")
    
    # Print statistics
    total_log_lines = sum(len(categorized_lines[cat]) for cat in ['LowLoad', 'MediumLoad', 'HighLoad', 'Unknown'])
    print(f"Extracted {len(categorized_lines['metadata'])} metadata lines")
    print(f"Categorized {total_log_lines} LOG lines:")
    lowload_range = format_time_range(LOWLOAD_START, LOWLOAD_END)
    mediumload_range = format_time_range(MEDIUMLOAD_START, MEDIUMLOAD_END)
    highload_range = format_time_range(HIGHLOAD_START, HIGHLOAD_END)
    print(f"  - LowLoad ({lowload_range}): {len(categorized_lines['LowLoad'])} lines")
    print(f"  - MediumLoad ({mediumload_range}): {len(categorized_lines['MediumLoad'])} lines")
    print(f"  - HighLoad ({highload_range}): {len(categorized_lines['HighLoad'])} lines")
    print(f"  - Unknown: {len(categorized_lines['Unknown'])} lines")
    
    return categorized_lines

def save_categorized_logs(raw_lines: List[str], categorized_lines: dict, node_id: str, 
                         folders: dict, port_name: str = None) -> dict:
    """
    Save raw and categorized logs to appropriate folders
    
    Args:
        raw_lines: Raw captured lines
        categorized_lines: Dictionary with categorized cleaned lines
        node_id: Node ID for filename
        folders: Dictionary with folder paths
        port_name: Port name to include in filename for multi-port captures
    
    Returns:
        Dictionary with saved filenames
    """
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    
    # Create port suffix for filenames when processing multiple ports
    port_suffix = ""
    if port_name:
        # Convert /dev/cu.usbmodem14101 to _usbmodem14101
        port_clean = port_name.replace('/dev/cu.', '').replace('/', '_')
        port_suffix = f"_{port_clean}"
    
    saved_files = {}
    
    # Save raw capture
    if node_id:
        raw_filename = f"pi{node_id}_raw_{timestamp}{port_suffix}.log"
    else:
        raw_filename = f"capture_raw_{timestamp}{port_suffix}.log"
    
    raw_filepath = os.path.join(folders['raw'], raw_filename)
    try:
        with open(raw_filepath, 'w', encoding='utf-8') as f:
            f.writelines(raw_lines)
        print(f" Raw capture saved to: {raw_filepath}")
        saved_files['raw'] = raw_filepath
    except Exception as e:
        print(f" Error saving raw capture: {e}")
        saved_files['raw'] = None
    
    # Save categorized logs
    for category in ['LowLoad', 'MediumLoad', 'HighLoad']:
        if not categorized_lines[category]:
            continue  # Skip empty categories
        
        # Create filename for this category
        if node_id:
            category_filename = f"pi{node_id}.log"
        else:
            category_filename = f"capture_{timestamp}{port_suffix}.log"
        
        category_filepath = os.path.join(folders[category], category_filename)
        
        try:
            with open(category_filepath, 'w', encoding='utf-8') as f:
                # Write metadata first (ID, MAC, names)
                f.writelines(categorized_lines['metadata'])
                # Then write the LOG lines for this category
                f.writelines(categorized_lines[category])
            
            print(f" {category} log saved to: {category_filepath}")
            saved_files[category] = category_filepath
        except Exception as e:
            print(f" Error saving {category} log: {e}")
            saved_files[category] = None
    
    # Save unknown logs if any
    if categorized_lines['Unknown']:
        if node_id:
            unknown_filename = f"pi{node_id}_unknown_{timestamp}.log"
        else:
            unknown_filename = f"capture_unknown_{timestamp}{port_suffix}.log"
        
        unknown_filepath = os.path.join(folders['Unknown'], unknown_filename)
        
        try:
            with open(unknown_filepath, 'w', encoding='utf-8') as f:
                f.writelines(categorized_lines['metadata'])
                f.writelines(categorized_lines['Unknown'])
            
            print(f" Unknown timestamp logs saved to: {unknown_filepath}")
            saved_files['Unknown'] = unknown_filepath
        except Exception as e:
            print(f" Error saving unknown logs: {e}")
            saved_files['Unknown'] = None
    
    return saved_files

def process_single_port(port: str, baudrate: int, max_wait_time: int, folders: dict) -> bool:
    """
    Process a single port: capture and clean log data
    
    Args:
        port: Serial port to process
        baudrate: Baud rate for serial connection
        max_wait_time: Maximum time to wait for complete log
        folders: Dictionary with experiment folder paths
    
    Returns:
        True if successful, False otherwise
    """
    print(f"\n{'='*60}")
    print(f"PROCESSING PORT: {port}")
    print(f"{'='*60}")
    
    try:
        # Capture the log
        result = capture_serial_log(port, baudrate, max_wait_time=max_wait_time)
        
        if result is None:
            print(f" Failed to capture log data from {port}")
            return False
        
        raw_lines, node_id = result
        
        if not raw_lines:
            print(f" No data captured from {port}")
            return False
        
        # Clean and categorize the captured data
        print(f"\n Cleaning and categorizing captured data from {port}...")
        categorized_lines = clean_and_categorize_log(raw_lines)
        
        if not categorized_lines or all(not lines for category, lines in categorized_lines.items()):
            print(f" No valid log data found in data from {port}")
            # Still save the raw capture for debugging
            timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
            raw_filename = f"failed_capture_{port.replace('/', '_')}_{timestamp}.log"
            raw_filepath = os.path.join(folders['raw'], raw_filename)
            with open(raw_filepath, 'w', encoding='utf-8') as f:
                f.writelines(raw_lines)
            print(f" Raw capture saved for debugging: {raw_filepath}")
            return False
        
        # Save the categorized files
        print(f"\n Saving categorized files for {port}...")
        saved_files = save_categorized_logs(raw_lines, categorized_lines, node_id, folders, port)
        
        print(f"\n SUCCESS for {port}!")
        if node_id:
            print(f"   Node ID: {node_id}")
        print(f"   Total lines captured: {len(raw_lines)}")
        
        # Print summary of saved files
        for category, filepath in saved_files.items():
            if filepath:
                if category == 'raw':
                    print(f"    Raw: {filepath}")
                else:
                    log_count = len(categorized_lines.get(category, []))
                    print(f"    {category}: {filepath} ({log_count} LOG lines)")
        
        return True
        
    except Exception as e:
        print(f" Error processing {port}: {e}")
        return False

def main():
    """Main function to run the automated log capture and cleanup for all usbmodem ports"""
    print("Automated Log Capture and Cleanup Tool - Multi-Load Test Mode")
    print("=" * 70)
    print("This tool will:")
    print("1. Find all ports starting with /dev/cu.usbmodem")
    print("2. For each port: connect and capture data until LOGSTART to LOGEND")
    print("3. Categorize LOG entries by timestamp into load tests:")
    print(get_load_test_info())
    print("4. Save logs in organized folder structure")
    print("=" * 70)
    
    # Get experiment name
    experiment_name = input("Enter experiment name (folder name): ").strip()
    if not experiment_name:
        print(" Experiment name cannot be empty!")
        return
    
    # Create experiment folder structure
    print(f"\n Creating experiment folder structure for '{experiment_name}'...")
    folders = create_experiment_folders(experiment_name)
    
    # Find all usbmodem ports
    usbmodem_ports = find_usbmodem_ports()
    
    if not usbmodem_ports:
        print(" No /dev/cu.usbmodem ports found!")
        print("\nAvailable ports:")
        all_ports = list_serial_ports()
        for port in all_ports:
            print(f"  - {port}")
        return
    
    print(f"\n Found {len(usbmodem_ports)} USB modem port(s):")
    for i, port in enumerate(usbmodem_ports, 1):
        try:
            port_info = next((p for p in serial.tools.list_ports.comports() if p.device == port), None)
            if port_info and port_info.description:
                print(f"  {i}. {port} - {port_info.description}")
            else:
                print(f"  {i}. {port}")
        except:
            print(f"  {i}. {port}")
    
    print("=" * 70)
    
    # Get configuration
    try:
        baudrate_input = input("Enter baud rate for all ports (default: 115200): ").strip()
        baudrate = int(baudrate_input) if baudrate_input else 115200
    except ValueError:
        print("Invalid baud rate, using default 115200")
        baudrate = 115200
    
    try:
        max_wait_input = input("Maximum wait time per port in seconds (default: 120): ").strip()
        max_wait_time = int(max_wait_input) if max_wait_input else 120
    except ValueError:
        print("Invalid wait time, using default 120 seconds")
        max_wait_time = 120
    
    print(f"\n Starting automated capture on {len(usbmodem_ports)} port(s)...")
    print(f"   Experiment: {experiment_name}")
    print(f"   Baud rate: {baudrate}")
    print(f"   Max wait time per port: {max_wait_time} seconds")
    print("   Press Ctrl+C to cancel\n")
    
    # Process each port
    successful_ports = []
    failed_ports = []
    
    try:
        for i, port in enumerate(usbmodem_ports, 1):
            print(f"\n Processing port {i}/{len(usbmodem_ports)}: {port}")
            
            success = process_single_port(port, baudrate, max_wait_time, folders)
            
            if success:
                successful_ports.append(port)
            else:
                failed_ports.append(port)
            
            # Add a small delay between ports to avoid conflicts
            if i < len(usbmodem_ports):
                print(f"\n Waiting 2 seconds before next port...")
                time.sleep(2)
        
        # Final summary
        print(f"\n{'='*70}")
        print("FINAL SUMMARY")
        print(f"{'='*70}")
        print(f" Experiment: {experiment_name}")
        print(f" Base folder: {folders['base']}")
        print(f" Successful ports: {len(successful_ports)}")
        for port in successful_ports:
            print(f"   - {port}")
        
        if failed_ports:
            print(f"\n Failed ports: {len(failed_ports)}")
            for port in failed_ports:
                print(f"   - {port}")
        
        print(f"\n Results saved in:")
        print(f"    Raw files: {folders['raw']}")
        lowload_range = format_time_range(LOWLOAD_START, LOWLOAD_END)
        mediumload_range = format_time_range(MEDIUMLOAD_START, MEDIUMLOAD_END)
        highload_range = format_time_range(HIGHLOAD_START, HIGHLOAD_END)
        print(f"    LowLoad ({lowload_range}): {folders['LowLoad']}")
        print(f"    MediumLoad ({mediumload_range}): {folders['MediumLoad']}")
        print(f"    HighLoad ({highload_range}): {folders['HighLoad']}")
        print(f"    Unknown timestamps: {folders['Unknown']}")
        
        print(f"\n Processing completed for all USB modem ports!")
        print(f"{'='*70}")
        
    except KeyboardInterrupt:
        print("\n\n  Capture cancelled by user")
        print(f"Processed {len(successful_ports + failed_ports)} out of {len(usbmodem_ports)} ports")
    except Exception as e:
        print(f"\n Unexpected error: {e}")

if __name__ == "__main__":
    main()
