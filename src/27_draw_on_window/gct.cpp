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
#include <gct/wait_for_sync.hpp>
#include <gct/allocator.hpp>
#include <gct/submit_info.hpp>
#include <gct/present_info.hpp>
#include <nlohmann/json.hpp>

struct fb_resources_t {
  std::shared_ptr< gct::semaphore_t > image_acquired;
  std::shared_ptr< gct::semaphore_t > image_ownership;
};

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
  bool close_app = false;
  window.set_on_closed( [&]( auto & ) { close_app = true; } );
  window.set_on_key( [&]( auto &, int key, int, int action, int ) {
    if( action == GLFW_PRESS ) {
      if( key == GLFW_KEY_Q ) close_app = true;
    }
  } );

  const auto physical_device_ = **physical_device.devices[ 0 ];
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
  
  const auto queue = device->get_queue( 0u );
  
  const auto allocator = device->get_allocator();

  const auto surface_capabilities = physical_device_.getSurfaceCapabilitiesKHR(
    **surface
  );
  const auto available_formats = physical_device_.getSurfaceFormatsKHR(
    **surface
  );

  const auto swapchain = device->get_swapchain(
    gct::swapchain_create_info_t()
      .set_basic(
        vk::SwapchainCreateInfoKHR()
          .setSurface( **surface )
          .setMinImageCount( std::max( surface_capabilities.minImageCount, 3u ) )
          .setImageFormat( available_formats[ 0 ].format )
          .setImageColorSpace( available_formats[ 0 ].colorSpace ) 
          .setImageExtent( surface_capabilities.currentExtent )
          .setImageArrayLayers( 1u )
          .setImageUsage(
            vk::ImageUsageFlagBits::eTransferDst |
            vk::ImageUsageFlagBits::eColorAttachment
          )
          .setPresentMode( vk::PresentModeKHR::eFifo )
          .setClipped( false )
      )
      .rebuild_chain()
  );

  auto swapchain_images = swapchain->get_images();

  // 画像の内容をバッファにロード
  const auto src_buffer = allocator->load_image(
    CMAKE_CURRENT_SOURCE_DIR "/test.png",
    true
  );
  
  {
    const auto command_buffer = queue->get_command_pool()->allocate();
    {
      auto rec = command_buffer->begin();
      for( auto &image: swapchain_images ) {
        rec.copy(
          src_buffer,
          image,
          vk::ImageLayout::ePresentSrcKHR
        );
      }
    }
    command_buffer->execute(
      gct::submit_info_t()
    );
    command_buffer->wait_for_executed();
  }

  std::vector< fb_resources_t > framebuffers;

  for( std::size_t i = 0u; i != swapchain_images.size(); ++i ) {
    framebuffers.emplace_back(
      fb_resources_t{
        device->get_semaphore(),
        device->get_semaphore()
      }
    );
  }

  uint32_t current_frame = 0u;
  while( !close_app ) {
    const auto begin_time = std::chrono::high_resolution_clock::now();

    auto &sync = framebuffers[ current_frame ];
    auto image_index = swapchain->acquire_next_image( sync.image_acquired );
    queue->present(
      gct::present_info_t()
        .add_wait_for( sync.image_acquired )
        .add_swapchain( swapchain, image_index )
    );
    glfwPollEvents();
    ++current_frame;
    current_frame %= framebuffers.size();
    gct::wait_for_sync( begin_time );
  }
}

