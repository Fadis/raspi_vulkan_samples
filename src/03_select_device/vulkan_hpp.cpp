#include <iostream>
#include <gct/instance.hpp>
#include <gct/physical_device.hpp>
#include <nlohmann/json.hpp>
#include <vulkan2json/QueueFamilyProperties.hpp>

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
  auto devices = instance.enumeratePhysicalDevices();

  // デバイスが1つも見つからなかったらabort
  if( devices.empty() ) std::abort();

  // 1つ目のGPUを使う
  const auto physical_device = devices[ 0 ];

  // デバイスに備わっているキューを取得
  const auto queue_props = physical_device.getQueueFamilyProperties();
  
  std::cout << "デバイスに備わっているキュー" << std::endl;
  std::cout << nlohmann::json( queue_props ).dump( 2 ) << std::endl;
  
  uint32_t queue_family_index = 0u;
  // 計算要求を受け付けるキューを探す
  for( uint32_t i = 0; i < queue_props.size(); ++i ) {
    if( queue_props[ i ].queueFlags & vk::QueueFlagBits::eCompute ) {
      queue_family_index = i;
      break;
    }
  }

  const float priority = 0.0f;
  // 計算要求を受け付けるキューを1つください
  std::vector< vk::DeviceQueueCreateInfo > queues{
    vk::DeviceQueueCreateInfo()
      .setQueueFamilyIndex( queue_family_index )
      .setQueueCount( 1 )
      .setPQueuePriorities( &priority )
  };

  // 論理デバイスを作る
  auto device = physical_device.createDeviceUnique(
    vk::DeviceCreateInfo()
      .setQueueCreateInfoCount( queues.size() )
      .setPQueueCreateInfos( queues.data() )
  );
  
  // デバイスからキューを取得
  auto queue = device->getQueue( queue_family_index, 0u );
}


