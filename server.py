from flask import Flask, request, send_from_directory, jsonify
import subprocess
import os
import logging
import socket
import time

# Configure logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

app = Flask(__name__, static_url_path='')

@app.route('/')
def home():
    return send_from_directory('.', 'index.html')

@app.route('/CubioSeedFinderIcon.png')
def icon():
    return send_from_directory('.', 'CubioSeedFinderIcon.png')

@app.route('/scan', methods=['POST'])
def scan():
    data = request.get_json()
    params = data.get('params', '')

    logger.info(f"Received scan request with params: {params}")

    try:
        # Create a process pipe to send parameters to verify_build
        process = subprocess.Popen(
            ['./verify_build'], #Corrected executable name
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            cwd='cubiomes'
        )

        # Send parameters to stdin and get output
        stdout, stderr = process.communicate(input=params)

        if process.returncode != 0:
            logger.error(f"verify_build failed with return code {process.returncode}")
            return f"Error: {stderr}", 500

        logger.info("Scan completed successfully")
        return stdout + stderr

    except Exception as e:
        logger.error(f"Error during scan: {str(e)}")
        return str(e), 500

def is_port_in_use(port):
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        try:
            s.bind(('0.0.0.0', port))
            return False
        except socket.error:
            return True

if __name__ == '__main__':
    port = 5000
    retries = 3

    logger.info("Starting Flask server...")
    while retries > 0:
        if not is_port_in_use(port):
            logger.info(f"Starting Flask server on port {port}...")
            app.run(
                host='0.0.0.0',
                port=port,
                debug=True,
                use_reloader=False,
                threaded=True
            )
            break
        else:
            logger.warning(f"Port {port} is in use, waiting...")
            retries -= 1
            time.sleep(2)

    if retries == 0:
        logger.error(f"Could not start server: port {port} is in use")
        raise RuntimeError(f"Port {port} is in use and could not be bound after retries")