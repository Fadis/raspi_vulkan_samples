add_executable( gct-graphics_pipeline gct.cpp )
target_compile_definitions( gct-graphics_pipeline PRIVATE -DCMAKE_CURRENT_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}" )
add_executable( vulkan_hpp-graphics_pipeline vulkan_hpp.cpp )
target_compile_definitions( vulkan_hpp-graphics_pipeline PRIVATE -DCMAKE_CURRENT_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}" )
add_executable( vulkan-graphics_pipeline vulkan.cpp )
target_compile_definitions( vulkan-graphics_pipeline PRIVATE -DCMAKE_CURRENT_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}" )
add_shader( gct-graphics_pipeline shader.vert )
add_shader( gct-graphics_pipeline shader.frag )

