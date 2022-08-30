add_executable( gct-descriptor_set gct.cpp )
target_compile_definitions( gct-descriptor_set PRIVATE -DCMAKE_CURRENT_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}" )
add_executable( vulkan_hpp-descriptor_set vulkan_hpp.cpp )
add_executable( vulkan-descriptor_set vulkan.cpp )
add_shader( gct-descriptor_set shader.comp )

