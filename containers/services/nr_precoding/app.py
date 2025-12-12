#!/usr/bin/env python3
"""
NR PRECODING REST Service
Exposes nr_precoding() function via REST API
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
lib.nr_precoding.argtypes = []
lib.nr_precoding.restype = None


@app.route('/health', methods=['GET'])
def health():
    """Health check endpoint for Kubernetes liveness probe"""
    return jsonify({"status": "healthy", "service": "nr_precoding"}), 200


@app.route('/ready', methods=['GET'])
def ready():
    """Readiness check endpoint for Kubernetes readiness probe"""
    try:
        if lib:
            return jsonify({"status": "ready", "service": "nr_precoding"}), 200
    except Exception as e:
        return jsonify({"status": "not_ready", "error": str(e)}), 503


@app.route('/precoding', methods=['POST'])
def execute_function():
    """
    Execute nr_precoding function
    
    Response:
    {
        "status": "success",
        "function": "nr_precoding",
        "message": "Execution completed"
    }
    """
    try:
        old_stdout = sys.stdout
        sys.stdout = captured_output = StringIO()
        
        # Execute function
        lib.nr_precoding()
        
        sys.stdout = old_stdout
        output = captured_output.getvalue()
        
        return jsonify({
            "status": "success",
            "function": "nr_precoding",
            "message": "Execution completed",
            "output": output[:500]
        }), 200
        
    except Exception as e:
        sys.stdout = old_stdout
        return jsonify({
            "status": "error",
            "function": "nr_precoding",
            "error": str(e)
        }), 500


@app.route('/info', methods=['GET'])
def info():
    """Service information endpoint"""
    return jsonify({
        "service": "nr_precoding",
        "version": "1.0",
        "description": "Precoding for MIMO transmission",
        "endpoints": {
            "POST /precoding": "Execute nr_precoding function",
            "GET /health": "Health check",
            "GET /ready": "Readiness check",
            "GET /info": "Service information"
        }
    }), 200


if __name__ == '__main__':
    port = int(os.environ.get('PORT', 8004))
    app.run(host='0.0.0.0', port=port, debug=False)
