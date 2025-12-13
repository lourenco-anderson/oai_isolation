# Multi-stage build to isolate and run a single OAI function (default: nr_crc)
# Stage 1: build OAI + isolation wrapper
FROM ubuntu:22.04 AS builder

# Avoid tzdata interactive prompt during package installs
ENV DEBIAN_FRONTEND=noninteractive TZ=Etc/UTC

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake ninja-build pkg-config python3 git \
    libconfig-dev libyaml-dev libsctp-dev libfftw3-dev libblas-dev liblapack-dev libnuma-dev libgfortran5 \
    tzdata \
    && ln -sf /usr/share/zoneinfo/Etc/UTC /etc/localtime \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . .
# Disable SSL verification for submodule fetch (CI without CA bundle)
RUN git config --global http.sslverify false
RUN git submodule update --init --recursive

WORKDIR /app/ext/openair/cmake_targets
# Build the OAI libs needed by the isolation binary. Adjust flags as required for your hardware.
RUN ./build_oai -I --ninja --nrUE --gNB --build-lib "nrscope" -w USRP

WORKDIR /app
# Point CMake to the location where OAI produced the libs
RUN cmake -B build -DOAI_BUILD_DIR=/app/ext/openair/cmake_targets/ran_build/build \
    && cmake --build build --target oai_isolation -j"$(nproc)"

# Stage 2: minimal runtime image
FROM ubuntu:22.04
RUN apt-get update && apt-get install -y --no-install-recommends \
    libconfig9 libyaml-0-2 libsctp1 libfftw3-double3 libblas3 liblapack3 libnuma1 libgfortran5 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY --from=builder /app/build/oai_isolation /app/oai_isolation
COPY --from=builder /app/ext/openair/cmake_targets/ran_build/build/*.so /usr/lib/
ENV LD_LIBRARY_PATH=/usr/lib

# Default to running nr_crc; override with another function name, e.g., CMD ["/app/oai_isolation", "nr_ldpc_dec"]
CMD ["/app/oai_isolation", "nr_crc"]
