set(EXE_NAME "TaOglApp")

add_executable(${EXE_NAME} "main.cpp")

target_link_libraries(${EXE_NAME} PRIVATE "TaOglContext" "TaOglGizmos")