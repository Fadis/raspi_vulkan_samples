#include <cmath>
#include <iostream>
#include <chrono>
#include <thread>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <gct/get_extensions.hpp>
#include <gct/instance.hpp>
#include <gct/queue.hpp>
#include <gct/device.hpp>
#include <gct/device_create_info.hpp>
#include <gct/glfw.hpp>
#include <gct/swapchain.hpp>

int main( int argc, const char *argv[] ) {

  gct::glfw::get();
  std::uint32_t required_extension_count = 0u;
  const char **required_extensions_begin = glfwGetRequiredInstanceExtensions( &required_extension_count );
  const auto required_extensions_end = std::next( required_extensions_begin, required_extension_count );
  std::for_each(
    required_extensions_begin,
    required_extensions_end,
    []( const char *v ) {
      std::cout << "このプラットフォームでサーフェスを作るには " << v << " 拡張が必要です" << std::endl;
    }
  );

  const std::shared_ptr< gct::instance_t > instance(
    new gct::instance_t(
      gct::instance_create_info_t()
        .set_application_info(
          vk::ApplicationInfo()
            .setPApplicationName( argc ? argv[ 0 ] : "my_application" )
            .setApplicationVersion(  VK_MAKE_VERSION( 1, 0, 0 ) )
            .setApiVersion( VK_API_VERSION_1_2 )
        )
	.add_extension(
          required_extensions_begin,
	  required_extensions_end
	)
        .add_layer(
          "VK_LAYER_KHRONOS_validation"
        )
    )
  );
  auto groups = instance->get_physical_devices( {} );
  auto physical_device = groups[ 0 ].with_extensions( {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    VK_KHR_SWAPCHAIN_MUTABLE_FORMAT_EXTENSION_NAME
  } );
  
  std::uint32_t width = 1024; 
  std::uint32_t height = 1024; 
  
  gct::glfw_window window( width, height, argc ? argv[ 0 ] : "my_application", false );

  auto surface = window.get_surface( *groups[ 0 ].devices[ 0 ] );
  
  const auto device = physical_device.create_device(
    std::vector< gct::queue_requirement_t >{
      gct::queue_requirement_t{
        vk::QueueFlagBits::eGraphics,
        0u,
        vk::Extent3D(),
#ifdef VK_EXT_GLOBAL_PRIORITY_EXTENSION_NAME
        vk::QueueGlobalPriorityEXT(),
#endif
        { **surface },
        vk::CommandPoolCreateFlagBits::eResetCommandBuffer
      }
    },
    gct::device_create_info_t()
  );
  
  // スワップチェーンを作る
  auto swapchain = device->get_swapchain( surface );

  // スワップチェーンのイメージを取得する
  auto swapchain_images = swapchain->get_images();
  
  // スワップチェーンのイメージの数を表示する
  std::cout << "swapchain images : " << swapchain_images.size() << std::endl;

}

