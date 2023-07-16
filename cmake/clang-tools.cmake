# clang-tools get all project files

file(GLOB_RECURSE ALL_SOURCE_FILES ${CMAKE_SOURCE_DIR}/src/*.cpp
     ${CMAKE_SOURCE_DIR}/test/*.cpp ${CMAKE_SOURCE_DIR}/include/*.hpp)
     
set(CLANG_TOOLS_PATH /usr/bin)

add_custom_target(format COMMAND ${CLANG_TOOLS_PATH}/clang-format -style=Google
                                 -i ${ALL_SOURCE_FILES})

add_custom_target(
  check-clang-tidy
  COMMAND
    ${CLANG_TOOLS_PATH}/clang-tidy
    --config-file=${CMAKE_CURRENT_SOURCE_DIR}/.clang-tidy -p
    ${CMAKE_CURRENT_SOURCE_DIR}/build ${ALL_SOURCE_FILES})
