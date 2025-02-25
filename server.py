from flask import Flask, request, send_from_directory, jsonify
import subprocess
import os
import logging

# Configure logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

app = Flask(__name__)

@app.route('/')
def home():
    return send_from_directory('.', 'index.html')

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

if __name__ == '__main__':
    logger.info("Starting Flask server on port 5000...")
    app.run(host='0.0.0.0', port=5000, debug=True)