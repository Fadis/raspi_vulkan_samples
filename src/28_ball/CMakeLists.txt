add_executable( vulkan-ball vulkan.cpp )
target_compile_definitions( vulkan-ball PRIVATE -DCMAKE_CURRENT_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}" )
add_executable( vulkan_hpp-ball vulkan_hpp.cpp )
target_compile_definitions( vulkan_hpp-ball PRIVATE -DCMAKE_CURRENT_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}" )
add_executable( gct-ball gct.cpp )
target_compile_definitions( gct-ball PRIVATE -DCMAKE_CURRENT_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}" )
add_shader( gct-ball shader.vert )
add_shader( gct-ball shader.frag )
