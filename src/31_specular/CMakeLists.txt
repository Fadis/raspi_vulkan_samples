add_executable( gct-specular gct.cpp )
target_compile_definitions( gct-specular PRIVATE -DCMAKE_CURRENT_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}" )
add_shader( gct-specular shader.vert )
add_shader( gct-specular shader.frag )

