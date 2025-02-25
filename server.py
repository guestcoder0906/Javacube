from flask import Flask, request, send_from_directory, jsonify
import subprocess
import os

app = Flask(__name__)

@app.route('/')
def home():
    return send_from_directory('.', 'index.html')

@app.route('/scan', methods=['POST'])
def scan():
    data = request.get_json()
    params = data.get('params', '')
    
    # Write parameters to the file
    with open('cubiomes/params.txt', 'w') as f:
        f.write(params)
    
    try:
        # Run the verify_build program
        result = subprocess.run(['./cubiomes/verify_build'], 
                              capture_output=True, 
                              text=True)
        return result.stdout + result.stderr
    except Exception as e:
        return str(e), 500

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000)
