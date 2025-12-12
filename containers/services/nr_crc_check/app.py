#!/usr/bin/env python3
"""
NR CRC CHECK REST Service
Exposes nr_crc_check() function via REST API
"""
import ctypes
import os
from flask import Flask, jsonify, request
from io import StringIO
import sys

app = Flask(__name__)

# Load shared library
lib_path = os.environ.get('OAI_LIB_PATH', '/usr/local/lib/liboai_functions.so')
lib = ctypes.CDLL(lib_path)

# Declare function signature
lib.nr_crc_check.argtypes = []
lib.nr_crc_check.restype = None


@app.route('/health', methods=['GET'])
def health():
    """Health check endpoint for Kubernetes liveness probe"""
    return jsonify({"status": "healthy", "service": "nr_crc_check"}), 200


@app.route('/ready', methods=['GET'])
def ready():
    """Readiness check endpoint for Kubernetes readiness probe"""
    try:
        if lib:
            return jsonify({"status": "ready", "service": "nr_crc_check"}), 200
    except Exception as e:
        return jsonify({"status": "not_ready", "error": str(e)}), 503


@app.route('/crc_check', methods=['POST'])
def execute_function():
    """
    Execute nr_crc_check function
    
    Response:
    {
        "status": "success",
        "function": "nr_crc_check",
        "message": "Execution completed"
    }
    """
    try:
        old_stdout = sys.stdout
        sys.stdout = captured_output = StringIO()
        
        # Execute function
        lib.nr_crc_check()
        
        sys.stdout = old_stdout
        output = captured_output.getvalue()
        
        return jsonify({
            "status": "success",
            "function": "nr_crc_check",
            "message": "Execution completed",
            "output": output[:500]
        }), 200
        
    except Exception as e:
        sys.stdout = old_stdout
        return jsonify({
            "status": "error",
            "function": "nr_crc_check",
            "error": str(e)
        }), 500


@app.route('/info', methods=['GET'])
def info():
    """Service information endpoint"""
    return jsonify({
        "service": "nr_crc_check",
        "version": "1.0",
        "description": "CRC validation for received data",
        "endpoints": {
            "POST /crc_check": "Execute nr_crc_check function",
            "GET /health": "Health check",
            "GET /ready": "Readiness check",
            "GET /info": "Service information"
        }
    }), 200


if __name__ == '__main__':
    port = int(os.environ.get('PORT', 8012))
    app.run(host='0.0.0.0', port=port, debug=False)
