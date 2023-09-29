set(EXE_NAME "TaOglApp")

# ImGui source files
# -------------------------
set(IMGUI_FOLDER "ImGui")
set(IMGUI_SRC_FILES
	"${IMGUI_FOLDER}/imconfig.h"
	"${IMGUI_FOLDER}/imgui.cpp"
	"${IMGUI_FOLDER}/imgui.h"
	"${IMGUI_FOLDER}/imgui_demo.cpp"
	"${IMGUI_FOLDER}/imgui_draw.cpp"
	"${IMGUI_FOLDER}/imgui_impl_glfw.cpp"
	"${IMGUI_FOLDER}/imgui_impl_glfw.h"
	"${IMGUI_FOLDER}/imgui_impl_opengl3.cpp"
	"${IMGUI_FOLDER}/imgui_impl_opengl3.h"
	"${IMGUI_FOLDER}/imgui_impl_opengl3_loader.h"
	"${IMGUI_FOLDER}/imgui_internal.h"
	"${IMGUI_FOLDER}/imgui_tables.cpp"
	"${IMGUI_FOLDER}/imgui_widgets.cpp"
	"${IMGUI_FOLDER}/imstb_rectpack.h"
	"${IMGUI_FOLDER}/imstb_textedit.h"
	"${IMGUI_FOLDER}/imstb_truetype.h"	)

add_executable(${EXE_NAME} "main.cpp" ${IMGUI_SRC_FILES})

target_link_libraries(${EXE_NAME} PRIVATE "TaOglContext" "TaOglGizmos" "TaOglPbr")

# set a constant with the path to the assets folder
set(ASSETS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Assets")
set(  HDRI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Assets/HDRI")
configure_file("config/TaOglAppConfig.h.in" "TaOglAppConfig.h")
target_include_directories(${EXE_NAME} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})