#include <iostream>
#include <gct/instance.hpp>
#include <gct/physical_device.hpp>
#include <vulkan2json/MemoryType.hpp>
#include <vulkan2json/MemoryHeap.hpp>

int main( int argc, const char *argv[] ) {
  std::shared_ptr< gct::instance_t > gct_instance(
    new gct::instance_t(
      gct::instance_create_info_t()
        .set_application_info(
          vk::ApplicationInfo()
            .setPApplicationName( argc ? argv[ 0 ] : "my_application" )
            .setApplicationVersion(  VK_MAKE_VERSION( 1, 0, 0 ) )
            .setApiVersion( VK_API_VERSION_1_2 )
        )
        .add_layer(
          "VK_LAYER_KHRONOS_validation"
        )
    )
  );
  auto instance = **gct_instance;
  
  uint32_t device_count = 0u;
  if( vkEnumeratePhysicalDevices( instance, &device_count, nullptr ) != VK_SUCCESS )
    return 1;
  std::vector< VkPhysicalDevice > devices( device_count );
  if( vkEnumeratePhysicalDevices( instance, &device_count, devices.data() ) != VK_SUCCESS )
    return 1;

  // デバイスが1つも見つからなかったらabort
  if( devices.empty() ) std::abort();

  // 1つ目のGPUを使う
  const auto physical_device = devices[ 0 ];

  VkPhysicalDeviceMemoryProperties memory_props;
  vkGetPhysicalDeviceMemoryProperties(
    physical_device,
    &memory_props
  );
  std::cout << "Heaps" << std::endl;
  for( std::uint32_t i = 0u; i != memory_props.memoryHeapCount; ++i ) {
    std::cout << nlohmann::json( memory_props.memoryHeaps[ i ] ).dump( 2 ) << std::endl;
  }
  std::cout << "Types" << std::endl;
  for( std::uint32_t i = 0u; i != memory_props.memoryTypeCount; ++i ) {
    std::cout << nlohmann::json( memory_props.memoryTypes[ i ] ).dump( 2 ) << std::endl;
  }

}


