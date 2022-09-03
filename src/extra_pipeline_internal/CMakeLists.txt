add_executable( gct-pipeline_internal gct.cpp )
target_compile_definitions( gct-pipeline_internal PRIVATE -DCMAKE_CURRENT_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}" )
add_shader( gct-pipeline_internal shader.vert )
add_shader( gct-pipeline_internal shader.frag )

