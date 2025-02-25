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

    # Write parameters to the file
    try:
        os.makedirs('cubiomes', exist_ok=True)
        with open('cubiomes/params.txt', 'w') as f:
            f.write(params)

        # Run the verify_build program
        result = subprocess.run(['./cubiomes/verify_build'], 
                              capture_output=True, 
                              text=True)
        logger.info("Scan completed successfully")
        return result.stdout + result.stderr
    except Exception as e:
        logger.error(f"Error during scan: {str(e)}")
        return str(e), 500

if __name__ == '__main__':
    logger.info("Starting Flask server on port 5000...")
    app.run(host='0.0.0.0', port=5000, debug=True)