add_executable( gct-textured gct.cpp )
target_compile_definitions( gct-textured PRIVATE -DCMAKE_CURRENT_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}" )
target_compile_definitions( gct-textured PRIVATE -DCMAKE_CURRENT_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}" )
add_shader( gct-textured shader.vert )
add_shader( gct-textured shader.frag )

