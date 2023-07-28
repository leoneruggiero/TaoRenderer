cmake_minimum_required(VERSION 3.12)

project("TaOgl")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_subdirectory("TaOglContext")
add_subdirectory("TaOglPbr")
add_subdirectory("TaOglGizmos")
add_subdirectory("TaOglApp")