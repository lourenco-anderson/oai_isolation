#!/usr/bin/env python3
"""
Generate Flask REST services for all OAI functions
"""
import os
from pathlib import Path

# List of all 15 functions with metadata
FUNCTIONS = [
    {"name": "nr_scramble", "port": 8001, "desc": "5G NR scrambling sequence generator"},
    {"name": "nr_crc", "port": 8002, "desc": "CRC computation for NR transport channels"},
    {"name": "nr_ofdm_modulation", "port": 8003, "desc": "OFDM modulation for NR downlink"},
    {"name": "nr_precoding", "port": 8004, "desc": "Precoding for MIMO transmission"},
    {"name": "nr_modulation_test", "port": 8005, "desc": "QAM modulation testing (QPSK/16-QAM/64-QAM/256-QAM)"},
    {"name": "nr_layermapping", "port": 8006, "desc": "Layer mapping for MIMO"},
    {"name": "nr_ldpc", "port": 8007, "desc": "LDPC encoding for NR channels"},
    {"name": "nr_ofdm_demo", "port": 8008, "desc": "OFDM modulation demonstration"},
    {"name": "nr_ch_estimation", "port": 8009, "desc": "Channel estimation for PDSCH"},
    {"name": "nr_descrambling", "port": 8010, "desc": "DLSCH descrambling with SIMD"},
    {"name": "nr_layer_demapping_test", "port": 8011, "desc": "Layer demapping for receiver"},
    {"name": "nr_crc_check", "port": 8012, "desc": "CRC validation for received data"},
    {"name": "nr_soft_demod", "port": 8013, "desc": "Soft demodulation (LLR computation)"},
    {"name": "nr_mmse_eq", "port": 8014, "desc": "MMSE equalization for MIMO reception"},
    {"name": "nr_ldpc_dec", "port": 8015, "desc": "LDPC decoding with belief propagation"},
]

APP_PY_TEMPLATE = '''#!/usr/bin/env python3
"""
{func_name_upper} REST Service
Exposes {func_name}() function via REST API
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
lib.{func_name}.argtypes = []
lib.{func_name}.restype = None


@app.route('/health', methods=['GET'])
def health():
    """Health check endpoint for Kubernetes liveness probe"""
    return jsonify({{"status": "healthy", "service": "{func_name}"}}), 200


@app.route('/ready', methods=['GET'])
def ready():
    """Readiness check endpoint for Kubernetes readiness probe"""
    try:
        if lib:
            return jsonify({{"status": "ready", "service": "{func_name}"}}), 200
    except Exception as e:
        return jsonify({{"status": "not_ready", "error": str(e)}}), 503


@app.route('/{endpoint}', methods=['POST'])
def execute_function():
    """
    Execute {func_name} function
    
    Response:
    {{
        "status": "success",
        "function": "{func_name}",
        "message": "Execution completed"
    }}
    """
    try:
        old_stdout = sys.stdout
        sys.stdout = captured_output = StringIO()
        
        # Execute function
        lib.{func_name}()
        
        sys.stdout = old_stdout
        output = captured_output.getvalue()
        
        return jsonify({{
            "status": "success",
            "function": "{func_name}",
            "message": "Execution completed",
            "output": output[:500]
        }}), 200
        
    except Exception as e:
        sys.stdout = old_stdout
        return jsonify({{
            "status": "error",
            "function": "{func_name}",
            "error": str(e)
        }}), 500


@app.route('/info', methods=['GET'])
def info():
    """Service information endpoint"""
    return jsonify({{
        "service": "{func_name}",
        "version": "1.0",
        "description": "{description}",
        "endpoints": {{
            "POST /{endpoint}": "Execute {func_name} function",
            "GET /health": "Health check",
            "GET /ready": "Readiness check",
            "GET /info": "Service information"
        }}
    }}), 200


if __name__ == '__main__':
    port = int(os.environ.get('PORT', {port}))
    app.run(host='0.0.0.0', port=port, debug=False)
'''

DOCKERFILE_TEMPLATE = '''# Multi-stage build for {func_name} service
FROM ubuntu:22.04 AS builder

RUN apt-get update && apt-get install -y \\
    build-essential cmake git libconfig-dev libyaml-dev \\
    libsctp-dev libatlas-base-dev python3 python3-pip \\
    libsimde-dev \\
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build
COPY src/ /build/src/
COPY ext/ /build/ext/
COPY CMakeLists.txt /build/

RUN mkdir -p build && cd build && \\
    cmake .. && \\
    make -j$(nproc) oai_functions 2>&1 | grep -v "oai_isolation" && \\
    cp liboai_functions.so /usr/local/lib/ && \\
    ldconfig

FROM ubuntu:24.04

RUN apt-get update && apt-get install -y \
    python3 python3-venv python3-pip libconfig9 libyaml-0-2 \
    libsctp1 libatlas3-base libforms2 libx11-6 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY --from=builder /usr/local/lib/liboai_functions.so /usr/local/lib/
COPY --from=builder /build/ext/openair/cmake_targets/ran_build/build/*.so* /usr/local/lib/
RUN ldconfig

COPY containers/services/{func_name}/app.py /app/
COPY containers/services/{func_name}/requirements.txt /app/

RUN python3 -m venv /app/venv && \
    /app/venv/bin/pip install --upgrade pip && \
    /app/venv/bin/pip install --no-cache-dir -r requirements.txt

EXPOSE {port}

ENV OAI_LIB_PATH=/usr/local/lib/liboai_functions.so
ENV PORT={port}

ENV VIRTUAL_ENV=/app/venv
ENV PATH="/app/venv/bin:$PATH"

CMD ["/app/venv/bin/python", "app.py"]
'''

REQUIREMENTS_TXT = '''Flask==3.0.0
Werkzeug==3.0.1
'''


def generate_service(func_info):
    """Generate Flask service for a function"""
    func_name = func_info["name"]
    port = func_info["port"]
    desc = func_info["desc"]
    
    # Create directory
    service_dir = Path(f"containers/services/{func_name}")
    service_dir.mkdir(parents=True, exist_ok=True)
    
    # Generate endpoint name (remove nr_ prefix)
    endpoint = func_name.replace("nr_", "")
    
    # Generate app.py
    app_content = APP_PY_TEMPLATE.format(
        func_name=func_name,
        func_name_upper=func_name.upper().replace("_", " "),
        port=port,
        description=desc,
        endpoint=endpoint
    )
    (service_dir / "app.py").write_text(app_content)
    
    # Generate Dockerfile
    dockerfile_content = DOCKERFILE_TEMPLATE.format(
        func_name=func_name,
        port=port
    )
    (service_dir / "Dockerfile").write_text(dockerfile_content)
    
    # Generate requirements.txt
    (service_dir / "requirements.txt").write_text(REQUIREMENTS_TXT)
    
    print(f"✅ Generated service: {func_name} (port {port})")


def main():
    """Generate all services"""
    print("=" * 50)
    print("Generating Flask REST services for OAI functions")
    print("=" * 50)
    
    for func in FUNCTIONS:
        generate_service(func)
    
    print("=" * 50)
    print(f"✅ Successfully generated {len(FUNCTIONS)} services")
    print("=" * 50)


if __name__ == "__main__":
    main()
