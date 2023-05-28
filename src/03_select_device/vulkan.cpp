#include <iostream>
#include <nlohmann/json.hpp>
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
  auto instance = VkInstance( **gct_instance );
  
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

  // デバイスに備わっているキューを取得
  uint32_t queue_props_count = 0u;
  vkGetPhysicalDeviceQueueFamilyProperties( physical_device, &queue_props_count, nullptr );
  std::vector< VkQueueFamilyProperties > queue_props( queue_props_count );
  vkGetPhysicalDeviceQueueFamilyProperties( physical_device, &queue_props_count, queue_props.data() );

  std::cout << "デバイスに備わっているキュー" << std::endl;
  std::cout << nlohmann::json( queue_props ).dump( 2 ) << std::endl;

  uint32_t queue_family_index = 0u;
  // 計算要求を受け付けるキューを探す
  for( uint32_t i = 0; i < queue_props.size(); ++i ) {
    if( queue_props[ i ].queueFlags & VkQueueFlagBits::VK_QUEUE_COMPUTE_BIT ) {
      queue_family_index = i;
      break;
    }
  }

  const float priority = 0.0f;
  // 計算要求を受け付けるキューを1つください
  VkDeviceQueueCreateInfo queue_create_info;
  queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queue_create_info.pNext = nullptr;
  queue_create_info.flags = 0;
  queue_create_info.queueFamilyIndex = queue_family_index;
  queue_create_info.queueCount = 1;
  queue_create_info.pQueuePriorities = &priority;

  // 論理デバイスを作る
  VkDeviceCreateInfo device_create_info;
  device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  device_create_info.pNext = nullptr;
  device_create_info.flags = 0;
  device_create_info.queueCreateInfoCount = 1;
  device_create_info.pQueueCreateInfos = &queue_create_info;
  device_create_info.enabledLayerCount = 0;
  device_create_info.ppEnabledLayerNames = nullptr;
  device_create_info.enabledExtensionCount = 0;
  device_create_info.ppEnabledExtensionNames = nullptr;
  device_create_info.pEnabledFeatures = nullptr;
  VkDevice device;
  if( vkCreateDevice(
    physical_device,
    &device_create_info,
    nullptr,
    &device
  ) != VK_SUCCESS ) abort();
  
  // デバイスからキューを取得
  VkQueue queue;
  vkGetDeviceQueue(
    device,
    queue_family_index,
    0u,
    &queue
  );

  // 論理デバイスを捨てる
  vkDestroyDevice(
    device,
    nullptr
  );
}


