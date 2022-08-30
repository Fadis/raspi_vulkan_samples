add_executable( gct-astc gct.cpp )
target_compile_definitions( gct-astc PRIVATE -DCMAKE_CURRENT_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}" )
target_compile_definitions( gct-astc PRIVATE -DCMAKE_CURRENT_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}" )
add_shader( gct-astc shader.vert )
add_shader( gct-astc shader.frag )
add_astc( gct-astc globe_color.png TRUE )
add_astc( gct-astc globe_normal.png FALSE )
add_astc( gct-astc globe_roughness.png FALSE )

