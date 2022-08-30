#include <iostream>
#include <cstdint>
#include <vector>
#include <vulkan/vulkan.h>

int main( int argc, const char *argv[] ) {
  VkApplicationInfo application_info;
  application_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  application_info.pNext = nullptr;
  // アプリケーションの名前
  application_info.pApplicationName = argc ? argv[ 0 ] : "my_application";
  // アプリケーションのバージョン
  application_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
  // エンジンの名前
  application_info.pEngineName ="my_engine";
  // エンジンのバージョン;
  application_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
  // 使用するVulkanのバージョンをVulkan 1.2にする
  application_info.apiVersion = VK_API_VERSION_1_2;
  // バリデーションレイヤーを使う
  const std::vector< const char * > layers{
    "VK_LAYER_KHRONOS_validation"
  };
  VkInstanceCreateInfo create_instance_info;
  create_instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  create_instance_info.pNext = nullptr;
  create_instance_info.flags = 0;
  // アプリケーションの情報を指定
  create_instance_info.pApplicationInfo = &application_info;
  // 使用するレイヤーを指定
  create_instance_info.enabledLayerCount = layers.size();
  create_instance_info.ppEnabledLayerNames = layers.data();
  // 使用する拡張はなし
  create_instance_info.enabledExtensionCount = 0;
  create_instance_info.ppEnabledExtensionNames = nullptr;
  VkInstance instance;
  // インスタンスを作成
  if( vkCreateInstance(
    &create_instance_info,
    nullptr,
    &instance
  ) != VK_SUCCESS ) return -1;

  // インスタンスがサポートするVulkanのバージョンを取得  
  std::uint32_t version;
  if( vkEnumerateInstanceVersion( &version ) != VK_SUCCESS ) return -1;
  std::cout <<
    VK_VERSION_MAJOR( version ) << "." <<
    VK_VERSION_MINOR( version ) << "." <<
    VK_VERSION_PATCH( version ) << std::endl; 

  // インスタンスを破棄
  vkDestroyInstance(
    instance,
    nullptr
  );

}

