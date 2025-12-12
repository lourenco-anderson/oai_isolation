#!/usr/bin/env python3
"""
NR MODULATION TEST REST Service
Exposes nr_modulation_test() function via REST API
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
lib.nr_modulation_test.argtypes = []
lib.nr_modulation_test.restype = None


@app.route('/health', methods=['GET'])
def health():
    """Health check endpoint for Kubernetes liveness probe"""
    return jsonify({"status": "healthy", "service": "nr_modulation_test"}), 200


@app.route('/ready', methods=['GET'])
def ready():
    """Readiness check endpoint for Kubernetes readiness probe"""
    try:
        if lib:
            return jsonify({"status": "ready", "service": "nr_modulation_test"}), 200
    except Exception as e:
        return jsonify({"status": "not_ready", "error": str(e)}), 503


@app.route('/modulation_test', methods=['POST'])
def execute_function():
    """
    Execute nr_modulation_test function
    
    Response:
    {
        "status": "success",
        "function": "nr_modulation_test",
        "message": "Execution completed"
    }
    """
    try:
        old_stdout = sys.stdout
        sys.stdout = captured_output = StringIO()
        
        # Execute function
        lib.nr_modulation_test()
        
        sys.stdout = old_stdout
        output = captured_output.getvalue()
        
        return jsonify({
            "status": "success",
            "function": "nr_modulation_test",
            "message": "Execution completed",
            "output": output[:500]
        }), 200
        
    except Exception as e:
        sys.stdout = old_stdout
        return jsonify({
            "status": "error",
            "function": "nr_modulation_test",
            "error": str(e)
        }), 500


@app.route('/info', methods=['GET'])
def info():
    """Service information endpoint"""
    return jsonify({
        "service": "nr_modulation_test",
        "version": "1.0",
        "description": "QAM modulation testing (QPSK/16-QAM/64-QAM/256-QAM)",
        "endpoints": {
            "POST /modulation_test": "Execute nr_modulation_test function",
            "GET /health": "Health check",
            "GET /ready": "Readiness check",
            "GET /info": "Service information"
        }
    }), 200


if __name__ == '__main__':
    port = int(os.environ.get('PORT', 8005))
    app.run(host='0.0.0.0', port=port, debug=False)
