#!/usr/bin/env python3
"""
NR SOFT DEMOD REST Service
Exposes nr_soft_demod() function via REST API
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
lib.nr_soft_demod.argtypes = []
lib.nr_soft_demod.restype = None


@app.route('/health', methods=['GET'])
def health():
    """Health check endpoint for Kubernetes liveness probe"""
    return jsonify({"status": "healthy", "service": "nr_soft_demod"}), 200


@app.route('/ready', methods=['GET'])
def ready():
    """Readiness check endpoint for Kubernetes readiness probe"""
    try:
        if lib:
            return jsonify({"status": "ready", "service": "nr_soft_demod"}), 200
    except Exception as e:
        return jsonify({"status": "not_ready", "error": str(e)}), 503


@app.route('/soft_demod', methods=['POST'])
def execute_function():
    """
    Execute nr_soft_demod function
    
    Response:
    {
        "status": "success",
        "function": "nr_soft_demod",
        "message": "Execution completed"
    }
    """
    try:
        old_stdout = sys.stdout
        sys.stdout = captured_output = StringIO()
        
        # Execute function
        lib.nr_soft_demod()
        
        sys.stdout = old_stdout
        output = captured_output.getvalue()
        
        return jsonify({
            "status": "success",
            "function": "nr_soft_demod",
            "message": "Execution completed",
            "output": output[:500]
        }), 200
        
    except Exception as e:
        sys.stdout = old_stdout
        return jsonify({
            "status": "error",
            "function": "nr_soft_demod",
            "error": str(e)
        }), 500


@app.route('/info', methods=['GET'])
def info():
    """Service information endpoint"""
    return jsonify({
        "service": "nr_soft_demod",
        "version": "1.0",
        "description": "Soft demodulation (LLR computation)",
        "endpoints": {
            "POST /soft_demod": "Execute nr_soft_demod function",
            "GET /health": "Health check",
            "GET /ready": "Readiness check",
            "GET /info": "Service information"
        }
    }), 200


if __name__ == '__main__':
    port = int(os.environ.get('PORT', 8013))
    app.run(host='0.0.0.0', port=port, debug=False)
