cmake_minimum_required(VERSION 3.17)
project(OAI_ISOLATION C)

# Gather all C sources in your src/ folder
file(GLOB_RECURSE SOURCES "src/*.c")

# Path to OAI build output (adjust if needed)
set(OAI_BUILD_DIR "${CMAKE_SOURCE_DIR}/ext/openair/cmake_targets/ran_build/build")

# Create the executable (include stubs.c)
add_executable(oai_isolation ${SOURCES} src/stubs.c)

# Include directories (headers)
target_include_directories(oai_isolation PUBLIC 
    "${CMAKE_SOURCE_DIR}/ext/openair"
    "${CMAKE_SOURCE_DIR}/ext/openair/openair1/"
    "${CMAKE_SOURCE_DIR}/ext/openair/openair1/PHY"
    "${CMAKE_SOURCE_DIR}/ext/openair/openair1/PHY/NR_TRANSPORT"
    "${CMAKE_SOURCE_DIR}/ext/openair/openair1/PHY/TOOLS"
    "${CMAKE_SOURCE_DIR}/ext/openair/common"
    "${CMAKE_SOURCE_DIR}/ext/openair/common/utils"
    "${CMAKE_SOURCE_DIR}/ext/openair/executables"
    "${CMAKE_SOURCE_DIR}/ext/openair/radio/COMMON/"
    "${CMAKE_SOURCE_DIR}/ext/openair/nfapi/open-nFAPI/nfapi/public_inc/"
    "${CMAKE_SOURCE_DIR}/ext/openair/openair2/UTIL/OMV/"
)

# Definitions (adapt if your function requires other constants)
target_compile_definitions(oai_isolation PRIVATE
    NB_ANTENNAS_TX=4
    NB_ANTENNAS_RX=4
    MAX_NUM_CCs=2
)

# Add OAI build directory so dynamic libs (-lxxx) are found
target_link_directories(oai_isolation PRIVATE
    ${OAI_BUILD_DIR}
)

# ---- LINK ALL REQUIRED STATIC AND DYNAMIC LIBRARIES ----
target_link_libraries(oai_isolation PRIVATE
    # --- Core OAI static libs ---
    ${OAI_BUILD_DIR}/libPHY_NR_COMMON.a
    ${OAI_BUILD_DIR}/libPHY_COMMON.a
    ${OAI_BUILD_DIR}/libPHY_NR.a
    ${OAI_BUILD_DIR}/libUTIL.a
    ${OAI_BUILD_DIR}/libSCHED_LIB.a
    ${OAI_BUILD_DIR}/libSCHED_NR_LIB.a
    
    # --- Dynamic libs built by OAI (shared .so) ---
    -lnrscope
    -loai_device
    -loai_usrpdevif
    -lparams_libconfig
    -lparams_yaml
    -lrf_emulator
    -lrfsimulator
    -lvrtsim
    
    # --- System libs ---
    m pthread rt dl
)