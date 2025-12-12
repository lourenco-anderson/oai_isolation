cmake_minimum_required(VERSION 3.17)
project(OAI_ISOLATION C)

# Gather all C sources in src/
file(GLOB_RECURSE SOURCES "src/*.c")

# Explicitly add the new onelayer file
list(APPEND SOURCES "src/nr_dlsch_onelayer.c")

# Note: LDPC decoder is complex and requires many OAI dependencies
# Instead, we link against the compiled OAI PHY libraries that contain it
# list(APPEND SOURCES "ext/openair/openair1/PHY/CODING/nrLDPC_decoder/nrLDPC_decoder.c")

# NOTE: LDPC encoder real compilation is disabled due to memory corruption issues
# with OAI library linkage. Use mock implementation in stubs.c instead.
# list(APPEND SOURCES "ext/openair/openair1/PHY/CODING/nrLDPC_encoder/ldpc_encoder_optim8segmulti.c")

# Path to OAI build output
set(OAI_BUILD_DIR "${CMAKE_SOURCE_DIR}/ext/openair/cmake_targets/ran_build/build")

# Create shared library for containerization
add_library(oai_functions SHARED 
    src/functions.c 
    src/stubs.c
    src/nr_dlsch_onelayer.c
)

# Create the executable (for local testing)
add_executable(oai_isolation src/main.c)

# Include directories (shared for both library and executable)
set(OAI_INCLUDE_DIRS
    "${CMAKE_SOURCE_DIR}/ext/openair/executables"
    "${CMAKE_SOURCE_DIR}/ext/openair"
    "${CMAKE_SOURCE_DIR}/ext/openair/openair1/"
    "${CMAKE_SOURCE_DIR}/ext/openair/openair1/PHY"
    "${CMAKE_SOURCE_DIR}/ext/openair/openair1/PHY/NR_TRANSPORT"
    "${CMAKE_SOURCE_DIR}/ext/openair/openair1/PHY/TOOLS"
    "${CMAKE_SOURCE_DIR}/ext/openair/openair1/PHY/CODING"
    "${CMAKE_SOURCE_DIR}/ext/openair/openair1/PHY/CODING/nrLDPC_encoder"
    "${CMAKE_SOURCE_DIR}/ext/oai/openair1/PHY/CODING/nrLDPC_decoder"
    "${CMAKE_SOURCE_DIR}/ext/openair/openair2/"
    "${CMAKE_SOURCE_DIR}/ext/openair/common"
    "${CMAKE_SOURCE_DIR}/ext/openair/common/utils"
    "${CMAKE_SOURCE_DIR}/ext/openair/executables"
    "${CMAKE_SOURCE_DIR}/ext/openair/radio/COMMON/"
    "${CMAKE_SOURCE_DIR}/ext/openair/nfapi/open-nFAPI/nfapi/public_inc/"
    "${CMAKE_SOURCE_DIR}/ext/openair/openair2/UTIL/OMV/"
)

target_include_directories(oai_functions PUBLIC ${OAI_INCLUDE_DIRS})
target_include_directories(oai_isolation PUBLIC ${OAI_INCLUDE_DIRS})

# Compile definitions
target_compile_definitions(oai_functions PRIVATE
    NB_ANTENNAS_TX=4
    NB_ANTENNAS_RX=4
    MAX_NUM_CCs=2
)

target_compile_definitions(oai_isolation PRIVATE
    NB_ANTENNAS_TX=4
    NB_ANTENNAS_RX=4
    MAX_NUM_CCs=2
)

# Enable AVX instructions
target_compile_options(oai_functions PRIVATE -mavx)
target_compile_options(oai_isolation PRIVATE -mavx)

# Add OAI build directory so static libs are found
target_link_directories(oai_functions PRIVATE ${OAI_BUILD_DIR})
target_link_directories(oai_isolation PRIVATE ${OAI_BUILD_DIR})

# OAI libraries (shared for both)
set(OAI_LIBRARIES
    PHY_NR_COMMON     
    PHY_COMMON         
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

target_link_libraries(oai_functions PRIVATE ${OAI_LIBRARIES})
target_link_libraries(oai_isolation PRIVATE oai_functions)

# Install shared library for containerization
install(TARGETS oai_functions
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
