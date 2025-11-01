cmake_minimum_required(VERSION 3.17)

project(OAI_ISOLATION)

file(GLOB_RECURSE sources "src/*.c")
file(GLOB OPENAIR_LIBS "ext/openair/cmake_targets/ran_build/build/*.a")
message("[INFO] sources:\n")
message("${sources}")
message("[INFO] libs:\n")
message("${OPENAIR_LIBS}")

add_executable(oai_isolation ${sources})

target_include_directories(oai_isolation PUBLIC "ext/openair/openair1/" "ext/openair/common/")
target_link_libraries(oai_isolation PUBLIC ${OPENAIR_LIBS})

