add_executable( gct-environment gct.cpp )
target_compile_definitions( gct-environment PRIVATE -DCMAKE_CURRENT_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}" )
target_compile_definitions( gct-environment PRIVATE -DCMAKE_CURRENT_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}" )
add_shader( gct-environment shader.vert )
add_shader( gct-environment shader.frag )

