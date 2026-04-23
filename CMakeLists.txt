cmake_minimum_required(VERSION 3.17)
project(OAI_ISOLATION C)

# Gather all C sources in src/
file(GLOB_RECURSE SOURCES "src/*.c")

# ldpc_v_oai/oai_encoder contains multiple alternative LDPCencoder implementations.
# Keep only the adapter (which includes the selected optimized implementation) to
# avoid duplicate symbol definitions at link time.
list(FILTER SOURCES EXCLUDE REGEX "src/ldpc_v_oai/src/oai_encoder/.*\\.c$")
list(APPEND SOURCES
    "src/ldpc_v_oai/src/oai_encoder/oai_encoder_adapter.c"
    "src/ldpc_v_oai/src/ldpc_helpers.c"
)

# Explicitly add the new onelayer file
list(APPEND SOURCES "src/nr_dlsch_onelayer.c")

# Note: LDPC decoder is complex and requires many OAI dependencies
# Instead, we link against the compiled OAI PHY libraries that contain it
# list(APPEND SOURCES "ext/openair/openair1/PHY/CODING/nrLDPC_decoder/nrLDPC_decoder.c")

# NOTE: We use the standalone OAI LDPC adapter from src/ldpc_v_oai/src/oai_encoder.

# Path to OAI build output
set(OAI_BUILD_DIR "${CMAKE_SOURCE_DIR}/ext/openair/cmake_targets/ran_build/build")

# Create the executable
add_executable(oai_isolation ${SOURCES})

# Include directories
target_include_directories(oai_isolation PUBLIC 
    "${CMAKE_SOURCE_DIR}/ext/openair/executables"
    "${CMAKE_SOURCE_DIR}/ext/openair"
    "${CMAKE_SOURCE_DIR}/ext/openair/openair1/"
    "${CMAKE_SOURCE_DIR}/ext/openair/openair1/PHY"
    "${CMAKE_SOURCE_DIR}/ext/openair/openair1/PHY/NR_TRANSPORT"
    "${CMAKE_SOURCE_DIR}/ext/openair/openair1/PHY/TOOLS"
    "${CMAKE_SOURCE_DIR}/ext/openair/openair1/PHY/CODING"
    "${CMAKE_SOURCE_DIR}/ext/openair/openair1/PHY/CODING/nrLDPC_encoder"
    "${CMAKE_SOURCE_DIR}/ext/openair/openair1/PHY/CODING/nrLDPC_decoder"
    "${CMAKE_SOURCE_DIR}/ext/openair/openair2/"
    "${CMAKE_SOURCE_DIR}/ext/openair/common"
    "${CMAKE_SOURCE_DIR}/ext/openair/common/utils"
    "${CMAKE_SOURCE_DIR}/ext/openair/common/utils/barrier"
    "${CMAKE_SOURCE_DIR}/ext/openair/common/utils/actor"
    "${CMAKE_SOURCE_DIR}/ext/openair/common/utils/threadPool"
    "${CMAKE_SOURCE_DIR}/ext/openair/openair2/NR_UE_PHY_INTERFACE"
    "${CMAKE_SOURCE_DIR}/ext/openair/openair2/NR_PHY_INTERFACE"
    "${CMAKE_SOURCE_DIR}/ext/openair/executables"
    "${CMAKE_SOURCE_DIR}/ext/openair/radio/COMMON/"
    "${CMAKE_SOURCE_DIR}/ext/openair/nfapi/open-nFAPI/nfapi/public_inc/"
    "${CMAKE_SOURCE_DIR}/ext/openair/openair2/UTIL/OMV/"
    "${CMAKE_SOURCE_DIR}/src/ldpc_v_oai/include"
    "${CMAKE_SOURCE_DIR}/src/ldpc_v_oai/src/oai_encoder"
)

# Compile definitions
target_compile_definitions(oai_isolation PRIVATE
    NB_ANTENNAS_TX=4
    NB_ANTENNAS_RX=4
    MAX_NUM_CCs=2
)

# Enable AVX instructions to fix sse_intrin.h warnings
target_compile_options(oai_isolation PRIVATE -mavx)

# Add OAI build directory so static libs are found
target_link_directories(oai_isolation PRIVATE ${OAI_BUILD_DIR})

# Restored OAI libraries for layer mapping and other functions
target_link_libraries(oai_isolation PRIVATE
    PHY_NR_COMMON     
    PHY_COMMON         
    ${OAI_BUILD_DIR}/common/utils/nr/libnr_common.a
    CONFIG_LIB             
    PHY_NR             
    PHY               
    UTIL           
    SCHED_LIB              
    SCHED_NR_LIB
    ${OAI_BUILD_DIR}/openair1/PHY/nr_phy_common/libnr_phy_common.a
    PHY_COMMON
    UTIL

    # Dynamic libs
    nrscope
    oai_device
    oai_usrpdevif
    params_libconfig    
    params_yaml
    rf_emulator
    rfsimulator
    vrtsim

    # System libs
    m pthread rt dl
)
