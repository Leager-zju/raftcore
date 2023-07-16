file(GLOB_RECURSE LIBS "${CMAKE_SOURCE_DIR}/lib/*.lib" "${CMAKE_SOURCE_DIR}/lib/*.dll")
file(GLOB_RECURSE SRCS "${CMAKE_SOURCE_DIR}/src/raft/*.cpp")

aux_source_directory(. MAIN)
add_executable(raftcore ${MAIN})

target_link_libraries(raftcore ${LIBS})

add_library(library ${SRCS})
target_link_libraries(raftcore library)