set(EXE_NAME "TaOglApp")

set(SRC_FOLDER "src")
set(LIB_FOLDER "lib")

# ImGui source files
# -------------------------
set(IMGUI_FOLDER "${SRC_FOLDER}/ImGui")
file(GLOB_RECURSE IMGUI_SRC_FILES
		"${IMGUI_FOLDER}/*.cpp"
		"${IMGUI_FOLDER}/*.h"
		"${IMGUI_FOLDER}/*.ini")

# Assimp include files
# -------------------------
set(ASSIMP_FOLDER "${SRC_FOLDER}/assimp")
file(GLOB_RECURSE ASSIMP_INCLUDE_FILES
		"${ASSIMP_FOLDER}/*.hpp"
		"${ASSIMP_FOLDER}/*.h")

add_executable(	${EXE_NAME} "${SRC_FOLDER}/main.cpp" "${SRC_FOLDER}/TaoScene.h" "${SRC_FOLDER}/TaoScene.cpp"
				${IMGUI_SRC_FILES} ${ASSIMP_INCLUDE_FILES})

target_include_directories(${EXE_NAME} PRIVATE ${SRC_FOLDER})

# include  assimp
target_link_directories(${EXE_NAME} PRIVATE ${LIB_FOLDER})

target_link_libraries(${EXE_NAME} PRIVATE "TaOglContext" "TaOglGizmos" "TaOglPbr" "assimp-vc143-mt.lib")


# set a constant with the path to the assets folder
set(ASSETS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Assets")
set(  HDRI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Assets/HDRI")
set(MODELS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Assets/Models")
configure_file("config/TaOglAppConfig.h.in" "TaOglAppConfig.h")
target_include_directories(${EXE_NAME} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

# copy assimp .dll to bin dir
add_custom_command(TARGET ${EXE_NAME} POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
		"${CMAKE_CURRENT_SOURCE_DIR}/dll/assimp-vc143-mt.dll"   # <--this is in-file
		${CMAKE_CURRENT_BINARY_DIR})                 			# <--this is out-file path