add_executable( gct-shader_module gct.cpp )
target_compile_definitions( gct-shader_module PRIVATE -DCMAKE_CURRENT_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}" )
add_executable( vulkan_hpp-shader_module vulkan_hpp.cpp )
target_compile_definitions( vulkan_hpp-shader_module PRIVATE -DCMAKE_CURRENT_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}" )
add_executable( vulkan-shader_module vulkan.cpp )
target_compile_definitions( vulkan-shader_module PRIVATE -DCMAKE_CURRENT_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}" )
add_shader( vulkan-shader_module shader.comp )

