add_executable(bspviewer
	CMakeLists.txt
	src/app.cpp
	src/app.h
	src/demo.cpp
	src/demo.h
	src/main.cpp
	src/this_module_info.h
)

target_include_directories(bspviewer PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(bspviewer
	appfw
	glad
	Freetype::Freetype
	renderer
	SDL2::SDL2
	SDL2::SDL2main
)
