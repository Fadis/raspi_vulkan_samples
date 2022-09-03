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

  const std::shared_ptr< gct::instance_t > gct_instance(
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
  auto groups = gct_instance->get_physical_devices( {} );
  auto gct_physical_device = groups[ 0 ].with_extensions( {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    VK_KHR_SWAPCHAIN_MUTABLE_FORMAT_EXTENSION_NAME
  } );
  
  std::uint32_t width = 1024; 
  std::uint32_t height = 1024; 

  gct::glfw_window window( width, height, argc ? argv[ 0 ] : "my_application", false );

  const auto gct_surface = window.get_surface( *groups[ 0 ].devices[ 0 ] );
  const auto surface = **gct_surface;
  
  const auto gct_device = gct_physical_device.create_device(
    std::vector< gct::queue_requirement_t >{
      gct::queue_requirement_t{
        vk::QueueFlagBits::eGraphics,
        0u,
        vk::Extent3D(),
#ifdef VK_EXT_GLOBAL_PRIORITY_EXTENSION_NAME
        vk::QueueGlobalPriorityEXT(),
#endif
        { surface },
        vk::CommandPoolCreateFlagBits::eResetCommandBuffer
      }
    },
    gct::device_create_info_t()
  );
  const auto instance = **gct_instance;
  const auto physical_device = **gct_physical_device.devices[ 0 ];
  const auto device = **gct_device;
  
  const auto surface_capabilities = physical_device.getSurfaceCapabilitiesKHR(
    surface
  );

  const auto available_formats = physical_device.getSurfaceFormatsKHR(
    surface
  );

  if( available_formats.empty() ) {
    std::cout << "利用可能なフォーマットがない" << std::endl;
    std::abort();
  }

  // スワップチェーンを作る
  const auto swapchain = device.createSwapchainKHRUnique(
    vk::SwapchainCreateInfoKHR()
      .setSurface( surface )
      // スワップチェーンのイメージの数
      .setMinImageCount( std::max( surface_capabilities.minImageCount, 3u ) )
      // スワップチェーンのイメージのフォーマット
      .setImageFormat( available_formats[ 0 ].format )
      // スワップチェーンのイメージの色空間
      .setImageColorSpace( available_formats[ 0 ].colorSpace ) 
      // スワップチェーンのイメージの大きさ
      .setImageExtent( surface_capabilities.currentExtent )
      // レイヤーは1つ
      .setImageArrayLayers( 1u )
      // グラフィクスパイプラインから色を吐くのに使える
      .setImageUsage( vk::ImageUsageFlagBits::eColorAttachment )
      // 投げたイメージが投げた順にサーフェスに送られるようなスワップチェーン
      .setPresentMode( vk::PresentModeKHR::eFifo )
      .setClipped( false )
  );

  // スワップチェーンのイメージを取得する
  const auto swapchain_images = device.getSwapchainImagesKHR( *swapchain );
  
  // スワップチェーンのイメージの数を表示する
  std::cout << "swapchain images : " << swapchain_images.size() << std::endl;
}

