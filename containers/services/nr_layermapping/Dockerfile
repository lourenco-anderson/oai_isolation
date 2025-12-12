# Multi-stage build for nr_layermapping service
FROM ubuntu:22.04 AS builder

RUN apt-get update && apt-get install -y \
    build-essential cmake git libconfig-dev libyaml-dev \
    libsctp-dev libatlas-base-dev python3 python3-pip \
    libsimde-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build
COPY src/ /build/src/
COPY ext/ /build/ext/
COPY CMakeLists.txt /build/

RUN mkdir -p build && cd build && \
    cmake .. && \
    make -j$(nproc) oai_functions 2>&1 | grep -v "oai_isolation" && \
    cp liboai_functions.so /usr/local/lib/ && \
    ldconfig

FROM ubuntu:24.04

RUN apt-get update && apt-get install -y     python3 python3-venv python3-pip libconfig9 libyaml-0-2     libsctp1 libatlas3-base libforms2 libx11-6     && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY --from=builder /usr/local/lib/liboai_functions.so /usr/local/lib/
COPY --from=builder /build/ext/openair/cmake_targets/ran_build/build/*.so* /usr/local/lib/
RUN ldconfig

COPY containers/services/nr_layermapping/app.py /app/
COPY containers/services/nr_layermapping/requirements.txt /app/

RUN python3 -m venv /app/venv &&     /app/venv/bin/pip install --upgrade pip &&     /app/venv/bin/pip install --no-cache-dir -r requirements.txt

EXPOSE 8006

ENV OAI_LIB_PATH=/usr/local/lib/liboai_functions.so
ENV PORT=8006

ENV VIRTUAL_ENV=/app/venv
ENV PATH="/app/venv/bin:$PATH"

CMD ["/app/venv/bin/python", "app.py"]
