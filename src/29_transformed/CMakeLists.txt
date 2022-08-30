add_executable( gct-transformed gct.cpp )
target_compile_definitions( gct-transformed PRIVATE -DCMAKE_CURRENT_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}" )
add_shader( gct-transformed shader.vert )
add_shader( gct-transformed shader.frag )

