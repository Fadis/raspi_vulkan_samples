add_executable( gct-compute_pipeline gct.cpp )
target_compile_definitions( gct-compute_pipeline PRIVATE -DCMAKE_CURRENT_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}" )
add_executable( vulkan_hpp-compute_pipeline vulkan_hpp.cpp )
target_compile_definitions( vulkan_hpp-compute_pipeline PRIVATE -DCMAKE_CURRENT_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}" )
add_executable( vulkan-compute_pipeline vulkan.cpp )
target_compile_definitions( vulkan-compute_pipeline PRIVATE -DCMAKE_CURRENT_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}" )
add_shader( vulkan-compute_pipeline shader.comp )

