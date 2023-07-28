# .lib name
set(LIB_NAME "TaOglPbr")

# collecting .cpp files	  
set(MY_SOURCE
		"src/PbrRenderer.cpp"
		"src/stb_image_cpp.cpp")
	
# some .h files are internal details
# not relevant for a client (placed into ./src)
set(PRIVATE_INCLUDES "src")
set(PUBLIC_INCLUDES "include")

add_library(${LIB_NAME} STATIC ${MY_SOURCE})

target_include_directories(${LIB_NAME} 
	PRIVATE ${PRIVATE_INCLUDES} 
	PUBLIC ${PUBLIC_INCLUDES})
	
# TaOglContext
target_link_libraries(${LIB_NAME} PRIVATE "TaOglContext")

# group glsl shaders source files
file(GLOB SHADERS_INCLUDE "shaders/*.glsl")
file(GLOB SHADERS_SOURCE 
	"shaders/*.vert"
	"shaders/*.geom"
	"shaders/*.frag")

source_group("shaders" FILES ${SHADERS_SOURCE})
source_group("shaders/include" FILES ${SHADERS_INCLUDE})

# set a constant with the path to shaders source files
set(SHADER_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders")
configure_file("config/TaOglPbrConfig.h.in" "TaOglPbrConfig.h")
target_include_directories(${LIB_NAME} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})