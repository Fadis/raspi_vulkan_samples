add_executable( vulkan_hpp-pipeline_cache vulkan_hpp.cpp )
target_compile_definitions( vulkan_hpp-pipeline_cache PRIVATE -DCMAKE_CURRENT_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}" )
add_executable( vulkan-pipeline_cache vulkan.cpp )
target_compile_definitions( vulkan-pipeline_cache PRIVATE -DCMAKE_CURRENT_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}" )
add_shader( vulkan_hpp-pipeline_cache shader.comp )

