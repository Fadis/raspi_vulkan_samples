add_executable( gct-bump gct.cpp )
target_compile_definitions( gct-bump PRIVATE -DCMAKE_CURRENT_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}" )
target_compile_definitions( gct-bump PRIVATE -DCMAKE_CURRENT_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}" )
add_shader( gct-bump shader.vert )
add_shader( gct-bump shader.frag )

