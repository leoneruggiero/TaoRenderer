# .lib name
set(LIB_NAME "TaOglPbr")

# collecting source files
set(MY_SOURCE "src/PbrRenderer.cpp")

# stb_image source files
set(STB_IMAGE_FOLDER_NAME "src/stb_image")
file(GLOB_RECURSE STB_IMAGE_SOURCE CONFIGURE_DEPENDS
		"${STB_IMAGE_FOLDER_NAME}/*.cpp"
		"${STB_IMAGE_FOLDER_NAME}/*.h" )

# gli source files
set(GLI_FOLDER_NAME "src/gli")
file(GLOB_RECURSE GLI_SOURCE CONFIGURE_DEPENDS
		"${GLI_FOLDER_NAME}/*.cpp"
		"${GLI_FOLDER_NAME}/*.hpp"
		"${GLI_FOLDER_NAME}/*.inl" )

# some .h files are internal details
# not relevant for a client (placed into ./src)
set(PRIVATE_INCLUDES "src")
set(PUBLIC_INCLUDES "include")

add_library(${LIB_NAME} STATIC
		${MY_SOURCE}
		${STB_IMAGE_SOURCE}
		${GLI_SOURCE} )

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

# write to configuration file
set(SHADER_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders")		# shaders source files
set(RESOURCES_DIR  "${CMAKE_CURRENT_SOURCE_DIR}/resources")		# additional resources (ibl textures, ...)
configure_file("config/TaOglPbrConfig.h.in" "TaOglPbrConfig.h")
target_include_directories(${LIB_NAME} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})