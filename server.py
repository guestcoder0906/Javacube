from flask import Flask, request, send_from_directory, jsonify, session
import subprocess
import os
import logging
import socket
import time
import psutil
from datetime import datetime
import threading
import re

# Configure logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

app = Flask(__name__, static_url_path='')

# Use environment variable for session secret or generate a random one
app.secret_key = os.environ.get('SESSION_SECRET', os.urandom(24))

# Dictionary to store scan statistics and processes per session
scan_stats = {}
active_processes = {}

def parse_structure_line(line):
    """Parse a single structure requirement line with the new format."""
    logger.debug(f"Parsing structure line: {line}")
    try:
        # Extract structure ID and requirements
        match = re.match(r'(\d+)\s*\(([^)]+)\)', line.strip())
        if not match:
            logger.error(f"Failed to parse structure line: {line}")
            return None

        structure_id = int(match.group(1))
        requirements = match.group(2)

        # Parse individual requirements
        result = {
            'structure_id': structure_id,
            'min_amount': 1,  # default
            'min_height': -9999,  # default
            'max_height': 9999,  # default
            'biome': -1,  # default
            'min_size': -1,  # default
            'max_size': -1,  # default
            'next_to_biomes': [],  # default empty
            'biome_proximity': -1,  # default
        }

        # Split requirements into key-value pairs
        for pair in requirements.split(','):
            pair = pair.strip()
            if ':' in pair:
                key, value = [p.strip() for p in pair.split(':', 1)]

                if key == 'next to biome':
                    # Split biomes on 'or' and convert to integers
                    biomes = [int(b.strip()) for b in value.split('or')]
                    result['next_to_biomes'] = biomes
                elif key == 'biome proximity':
                    result['biome_proximity'] = int(value)
                elif key == 'min amount':
                    result['min_amount'] = int(value)
                elif key == 'min height':
                    result['min_height'] = int(value)
                elif key == 'max height':
                    result['max_height'] = int(value)
                elif key == 'biome':
                    result['biome'] = int(value)
                elif key == 'min size':
                    result['min_size'] = int(value)
                elif key == 'max size':
                    result['max_size'] = int(value)

        logger.debug(f"Parsed structure requirements: {result}")
        return result
    except Exception as e:
        logger.error(f"Error parsing structure line: {str(e)}")
        return None

def format_structure_params(requirements):
    """Convert parsed structure requirements back to the format expected by verify_build."""
    lines = []
    for req in requirements:
        # Format the basic parameters
        params = [
            f"min amount: {req['min_amount']}",
            f"min height: {req['min_height']}",
            f"max height: {req['max_height']}",
            f"biome: {req['biome']}",
            f"min size: {req['min_size']}",
            f"max size: {req['max_size']}"
        ]

        # Add the new parameters if they exist
        if req['next_to_biomes']:
            params.append(f"next to biome: {' or '.join(map(str, req['next_to_biomes']))}")
        if req['biome_proximity'] != -1:
            params.append(f"biome proximity: {req['biome_proximity']}")

        line = f"{req['structure_id']} ({', '.join(params)})"
        lines.append(line)

    return '\n'.join(lines)

def cleanup_session(user_id):
    """Clean up resources for a session"""
    logger.info(f"Starting cleanup for session {user_id}")

    # Clean up active process if it exists
    if user_id in active_processes:
        try:
            process = active_processes[user_id]
            if process and process.poll() is None:
                process.terminate()
                process.wait(timeout=5)  # Wait up to 5 seconds
                if process.poll() is None:
                    process.kill()  # Force kill if still running
            logger.info(f"Cleaned up process for session {user_id}")
        except Exception as e:
            logger.error(f"Error cleaning up process for session {user_id}: {str(e)}")
        finally:
            try:
                active_processes.pop(user_id, None)  # Safely remove from dictionary
            except Exception as e:
                logger.error(f"Error removing process from active_processes: {str(e)}")

    # Clean up scan stats if they exist
    try:
        scan_stats.pop(user_id, None)  # Safely remove from dictionary
        logger.info(f"Cleaned up scan stats for session {user_id}")
    except Exception as e:
        logger.error(f"Error cleaning up scan stats for session {user_id}: {str(e)}")

@app.route('/')
def home():
    try:
        # Clean up old session if it exists
        old_user_id = session.get('user_id')
        if old_user_id:
            cleanup_session(old_user_id)
            logger.info(f"Cleaned up old session: {old_user_id}")

        # Create new session
        session['user_id'] = os.urandom(16).hex()
        session['last_active'] = datetime.now().timestamp()
        return send_from_directory('.', 'index.html')
    except Exception as e:
        logger.error(f"Error in home route: {str(e)}")
        return send_from_directory('.', 'index.html')  # Still return the page even if cleanup fails

@app.route('/CubioSeedFinderIcon.png')
def icon():
    return send_from_directory('.', 'CubioSeedFinderIcon.png')

def update_seeds_scanned(user_id, stdout_line):
    if user_id in scan_stats:
        # Look for seed count in the output
        seed_match = re.search(r'Seeds scanned: (\d+)', stdout_line)
        if seed_match:
            scan_stats[user_id]['seeds_scanned'] = int(seed_match.group(1))

@app.route('/scan', methods=['POST'])
def scan():
    user_id = session.get('user_id')
    if not user_id:
        return "Session expired", 401

    # Clean up any existing scan for this session
    cleanup_session(user_id)

    data = request.get_json()
    params = data.get('params', '')

    logger.info(f"Received scan request with params: {params}")

    try:
        # Initialize scan statistics for this session
        scan_stats[user_id] = {
            'seeds_scanned': 0,
            'start_time': datetime.now().timestamp()
        }

        # Parse and process the structure requirements
        lines = params.split('\n')
        current_section = None
        structure_requirements = []
        modified_lines = []

        for line in lines:
            line = line.strip()
            if not line:
                modified_lines.append(line)
                continue

            if line.startswith('====='):
                current_section = line
                modified_lines.append(line)
                continue

            if current_section == "===== Required structures =====":
                parsed_req = parse_structure_line(line)
                if parsed_req:
                    structure_requirements.append(parsed_req)
                    logger.debug(f"Parsed structure requirement: {parsed_req}")
                    # Format the line with all parameters
                    modified_lines.append(format_structure_params([parsed_req]))
                else:
                    # If parsing fails, keep the original line
                    modified_lines.append(line)
            else:
                modified_lines.append(line)

        # Join the modified lines back together
        modified_params = '\n'.join(modified_lines)
        logger.debug(f"Modified params: {modified_params}")

        # Get absolute path to verify_build
        app_root = os.path.dirname(os.path.abspath(__file__))
        verify_build_path = os.path.join(app_root, 'verify_build')

        # Log paths for debugging
        logger.info(f"App root path: {app_root}")
        logger.info(f"Current working directory: {os.getcwd()}")
        logger.info(f"Looking for verify_build at: {verify_build_path}")
        logger.info(f"Directory contents: {os.listdir(app_root)}")

        # Check if verify_build exists
        if not os.path.exists(verify_build_path):
            error_msg = f"verify_build not found at {verify_build_path}"
            logger.error(error_msg)
            return jsonify({
                'error': error_msg,
                'stats': {
                    'seeds_scanned': 0,
                    'elapsed_time': 0
                }
            }), 500

        # Create a process pipe to send parameters to verify_build
        process = subprocess.Popen(
            [verify_build_path],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            cwd=app_root,
            bufsize=1,
            universal_newlines=True
        )

        # Store process for cleanup
        active_processes[user_id] = process

        # Send modified parameters to stdin
        process.stdin.write(modified_params)
        process.stdin.close()

        # Read output line by line to update stats in real-time
        output_lines = []
        while True:
            line = process.stdout.readline()
            if not line and process.poll() is not None:
                break
            output_lines.append(line)
            update_seeds_scanned(user_id, line)
            logger.debug(f"Read line: {line.strip()}")

        stdout = ''.join(output_lines)
        stderr = process.stderr.read()

        # Calculate elapsed time before cleaning up stats
        end_time = datetime.now().timestamp()
        stats = scan_stats.pop(user_id, {})
        stats['elapsed_time'] = end_time - stats.get('start_time', end_time)
        if 'start_time' in stats:
            del stats['start_time']

        # Clean up process
        del active_processes[user_id]

        if process.returncode != 0:
            logger.error(f"verify_build failed with return code {process.returncode}")
            return jsonify({
                'error': stderr,
                'stats': stats
            }), 500

        logger.info("Scan completed successfully")
        return jsonify({
            'output': stdout + stderr,
            'stats': stats
        })

    except Exception as e:
        logger.error(f"Error during scan: {str(e)}")
        # Clean up scan stats and process on error
        cleanup_session(user_id)
        return jsonify({
            'error': str(e),
            'stats': {
                'seeds_scanned': 0,
                'elapsed_time': 0
            }
        }), 500

@app.route('/scan_stats')
def get_scan_stats():
    user_id = session.get('user_id')
    if not user_id or user_id not in scan_stats:
        return jsonify({
            'seeds_scanned': 0,
            'elapsed_time': 0
        })

    # Update last active timestamp
    session['last_active'] = datetime.now().timestamp()

    stats = scan_stats[user_id]
    elapsed_time = datetime.now().timestamp() - stats['start_time']
    return jsonify({
        'seeds_scanned': stats['seeds_scanned'],
        'elapsed_time': elapsed_time
    })

@app.route('/heartbeat')
def heartbeat():
    """Endpoint to keep session alive and check if scan should continue"""
    user_id = session.get('user_id')
    if not user_id:
        return jsonify({'status': 'expired'}), 401

    session['last_active'] = datetime.now().timestamp()
    return jsonify({'status': 'active'})

# Session cleanup thread
def cleanup_inactive_sessions():
    while True:
        try:
            current_time = datetime.now().timestamp()
            inactive_threshold = 30  # 30 seconds of inactivity

            for user_id in list(active_processes.keys()):
                last_active = session.get('last_active', 0)
                if current_time - last_active > inactive_threshold:
                    logger.info(f"Cleaning up inactive session {user_id}")
                    cleanup_session(user_id)

            time.sleep(10)  # Check every 10 seconds
        except Exception as e:
            logger.error(f"Error in cleanup thread: {str(e)}")

# Start cleanup thread
cleanup_thread = threading.Thread(target=cleanup_inactive_sessions, daemon=True)
cleanup_thread.start()

def is_port_in_use(port):
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        try:
            s.bind(('0.0.0.0', port))
            return False
        except OSError as e:
            if e.errno == 98: #Address already in use
                logger.error(f"Port {port} is already in use.")
                return True
            else:
                logger.error(f"Port {port} check failed: {str(e)}")
                return True

def kill_port_processes(port):
    for proc in psutil.process_iter(['pid', 'name', 'cmdline']):
        try:
            for conns in proc.net_connections(kind='inet'):
                if conns.laddr.port == port:
                    logger.warning(f"Killing process {proc.info['pid']} ({proc.info['name']}) using port {port}")
                    proc.kill()
        except (psutil.NoSuchProcess, psutil.AccessDenied, psutil.ZombieProcess):
            pass

if __name__ == '__main__':
    port = 5000
    retries = 3
    retry_delay = 2

    logger.info("Starting Flask server...")

    while retries > 0:
        if not is_port_in_use(port):
            logger.info(f"Starting Flask server on port {port}...")
            try:
                app.run(
                    host='0.0.0.0',
                    port=port,
                    debug=True,
                    use_reloader=False,
                    threaded=True
                )
                break
            except Exception as e:
                logger.error(f"Failed to start server: {str(e)}")
                retries -= 1
                time.sleep(retry_delay)
        else:
            logger.warning(f"Port {port} is in use, waiting {retry_delay} seconds...")
            retries -= 1
            time.sleep(retry_delay)

    if retries == 0:
        logger.error(f"Could not start server: port {port} is in use")
        raise RuntimeError(f"Port {port} is in use and could not be bound after retries")