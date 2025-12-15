# Runtime image for isolated function
FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive TZ=Etc/UTC

# Install only runtime dependencies
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
    libforms2 \
    libx11-6 \
    libxpm4 \
    libxcb1 \
    libbsd0 \
    libmd0 \
    libconfig9 \
    libyaml-0-2 \
    libsctp1 \
    libfftw3-double3 \
    libblas3 \
    liblapack3 \
    libnuma1 \
    libgfortran5 \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy the prebuilt binary and shared libs from the host build tree (context = repo root)
COPY build/oai_isolation /app/oai_isolation
COPY ext/openair/cmake_targets/ran_build/build/libnrscope.so /usr/lib/
COPY ext/openair/cmake_targets/ran_build/build/*.so /usr/lib/

# Make sure dynamic linker can find libs
ENV LD_LIBRARY_PATH=/usr/lib:/app:$LD_LIBRARY_PATH

# Run selected function
CMD ["/app/oai_isolation", "nr_crc"]
