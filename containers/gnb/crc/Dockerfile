# Runtime-only image to run prebuilt oai_isolation (default: nr_crc)
# Copies host-built binary and libs instead of building OAI inside the container.

FROM ubuntu:24.04
ENV LD_LIBRARY_PATH=/usr/lib:/app

RUN apt-get update && apt-get install -y --no-install-recommends \
    libforms2 libx11-6 libxpm4 libxcb1 libbsd0 libmd0 \
    libconfig9 libyaml-0-2 libsctp1 libfftw3-double3 libblas3 liblapack3 libnuma1 libgfortran5 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
# Expect the host build artifacts to be present in the build context:
# - build/oai_isolation
# - ext/openair/cmake_targets/ran_build/build/*.so (includes libnrscope.so)
COPY ./build/oai_isolation /app/oai_isolation
COPY ./ext/openair/cmake_targets/ran_build/build/*.so /usr/lib/

# Default to nr_crc; change the last arg to run another function.
CMD ["/app/oai_isolation", "nr_crc"]
