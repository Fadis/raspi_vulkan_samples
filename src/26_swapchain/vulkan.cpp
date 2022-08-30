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
 
  std::uint32_t available_format_count = 0u;
  if( vkGetPhysicalDeviceSurfaceFormatsKHR(
    physical_device,
    surface,
    &available_format_count,
    nullptr
  ) != VK_SUCCESS ) std::abort();
  std::vector< VkSurfaceFormatKHR > available_formats( available_format_count );
  if( vkGetPhysicalDeviceSurfaceFormatsKHR(
    physical_device,
    surface,
    &available_format_count,
    available_formats.data()
  ) != VK_SUCCESS ) std::abort();

  if( available_formats.empty() ) {
    std::cout << "利用可能なフォーマットがない" << std::endl;
    std::abort();
  }

  VkSwapchainCreateInfoKHR swapchain_create_info;
  swapchain_create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  swapchain_create_info.pNext = nullptr;
  swapchain_create_info.flags = 0u;
  swapchain_create_info.surface = surface;
  swapchain_create_info.minImageCount = std::max( surface_capabilities.minImageCount, 3u );
  swapchain_create_info.imageFormat = available_formats[ 0 ].format;
  swapchain_create_info.imageColorSpace = available_formats[ 0 ].colorSpace;
  swapchain_create_info.imageExtent.width = surface_capabilities.currentExtent.width;
  swapchain_create_info.imageExtent.height = surface_capabilities.currentExtent.height;
  swapchain_create_info.imageArrayLayers = 1u;
  swapchain_create_info.imageUsage = VkImageUsageFlagBits::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  swapchain_create_info.imageSharingMode = VkSharingMode::VK_SHARING_MODE_EXCLUSIVE;
  swapchain_create_info.queueFamilyIndexCount = 0u;
  swapchain_create_info.pQueueFamilyIndices = nullptr;
  swapchain_create_info.preTransform = VkSurfaceTransformFlagBitsKHR::VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
  swapchain_create_info.compositeAlpha = VkCompositeAlphaFlagBitsKHR::VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  swapchain_create_info.presentMode = VkPresentModeKHR::VK_PRESENT_MODE_FIFO_KHR;
  swapchain_create_info.clipped = VK_FALSE;
  swapchain_create_info.oldSwapchain = VK_NULL_HANDLE;

  VkSwapchainKHR swapchain;
  if( vkCreateSwapchainKHR(
    device,
    &swapchain_create_info,
    nullptr,
    &swapchain
  ) != VK_SUCCESS ) std::abort();

  std::uint32_t swapchain_image_count = 0u;
  if( vkGetSwapchainImagesKHR(
    device,
    swapchain,
    &swapchain_image_count,
    nullptr
  ) != VK_SUCCESS ) std::abort();
  std::vector< VkImage > swapchain_images( swapchain_image_count );
  if( vkGetSwapchainImagesKHR(
    device,
    swapchain,
    &swapchain_image_count,
    swapchain_images.data()
  ) != VK_SUCCESS ) std::abort();

  std::cout << "swapchain images : " << swapchain_images.size() << std::endl;

  vkDestroySwapchainKHR( device, swapchain, nullptr );
}

