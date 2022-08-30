add_executable( gct-roughness_mask gct.cpp )
target_compile_definitions( gct-roughness_mask PRIVATE -DCMAKE_CURRENT_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}" )
target_compile_definitions( gct-roughness_mask PRIVATE -DCMAKE_CURRENT_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}" )
add_shader( gct-roughness_mask shader.vert )
add_shader( gct-roughness_mask shader.frag )

