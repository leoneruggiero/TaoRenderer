# .lib name
set(LIB_NAME "TaOglContext")

# collecting .h files
set(MY_HEADERS
	"include/TaOglContext/RenderContext.h"
	"include/TaOglContext/RenderContextUtils.h"
	"include/TaOglContext/Resources.h"
	"include/TaOglContext/OglUtils.h"
	"include/TaOglContext/Input.h"
	"include/TaOglContext/GeometricPrimitives.h"
	"include/TaOglContext/Instrumentation.h")

# collecting .cpp files	  
set(MY_SOURCE
	"src/RenderContext.cpp"
	"src/Resources.cpp"
	"src/Instrumentation.cpp"
	"src/glad.c"
	"src/RenderContextUtils.cpp")
	
add_library(${LIB_NAME} STATIC ${MY_SOURCE})
	
# include directories
set(PRIVATE_INCLUDE "src")
	
set(PUBLIC_INCLUDE "include")

target_include_directories(${LIB_NAME} PRIVATE ${PRIVATE_INCLUDE})
target_include_directories(${LIB_NAME} PUBLIC ${PUBLIC_INCLUDE})

# include and link GLFW
target_link_directories(${LIB_NAME} PUBLIC "lib")
target_link_libraries(${LIB_NAME} "glfw3.lib")

# include opengl32.lib
target_link_libraries(${LIB_NAME} "opengl32.lib")