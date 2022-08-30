add_executable( gct-diffuse gct.cpp )
target_compile_definitions( gct-diffuse PRIVATE -DCMAKE_CURRENT_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}" )
add_shader( gct-diffuse shader.vert )
add_shader( gct-diffuse shader.frag )
