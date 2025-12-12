# Multi-stage build for nr_ch_estimation service
FROM ubuntu:22.04 AS builder

RUN apt-get update && apt-get install -y \
    build-essential cmake git libconfig-dev libyaml-dev \
    libsctp-dev libatlas-base-dev python3 python3-pip \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build
COPY src/ /build/src/
COPY ext/ /build/ext/
COPY CMakeLists.txt /build/

RUN mkdir -p build && cd build && \
    cmake .. && \
    make -j$(nproc) oai_functions && \
    make install

FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    python3 python3-pip libconfig9 libyaml-0-2 \
    libsctp1 libatlas3-base \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY --from=builder /usr/local/lib/liboai_functions.so /usr/local/lib/
RUN ldconfig

COPY containers/services/nr_ch_estimation/app.py /app/
COPY containers/services/nr_ch_estimation/requirements.txt /app/

RUN pip3 install --no-cache-dir -r requirements.txt

EXPOSE 8009

ENV OAI_LIB_PATH=/usr/local/lib/liboai_functions.so
ENV PORT=8009

CMD ["python3", "app.py"]
