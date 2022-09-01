#include <iostream>
#include <gct/instance.hpp>
#include <gct/device.hpp>
#include <gct/allocator.hpp>
#include <gct/device_create_info.hpp>
#include <gct/descriptor_pool.hpp>
#include <gct/descriptor_set_layout.hpp>
#include <gct/write_descriptor_set.hpp>
#include <gct/shader_module.hpp>
#include <vulkan/vulkan.h>
#include <vulkan2json/MemoryRequirements.hpp>

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
  auto groups = gct_instance->get_physical_devices( {} );
  auto gct_physical_device = groups[ 0 ].with_extensions( {} );
  auto gct_device =
    gct_physical_device
      .create_device(
        std::vector< gct::queue_requirement_t >{
          gct::queue_requirement_t{
            vk::QueueFlagBits::eCompute,
            0u,
            vk::Extent3D(),
#ifdef VK_EXT_GLOBAL_PRIORITY_EXTENSION_NAME
            vk::QueueGlobalPriorityEXT(),
#endif
            {},
            vk::CommandPoolCreateFlagBits::eResetCommandBuffer
          }
        },
        gct::device_create_info_t()
      );
  const auto instance = VkInstance( **gct_instance );
  const auto physical_device = VkPhysicalDevice( **gct_physical_device.devices[ 0 ] );
  const auto device = VkDevice( **gct_device );

  VkImageCreateInfo image_create_info;

  image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  image_create_info.pNext = nullptr;
  image_create_info.flags = 0u;
  // 2次元で
  image_create_info.imageType = VkImageType::VK_IMAGE_TYPE_2D;
  // RGBA各8bitで
  image_create_info.format = VkFormat::VK_FORMAT_R8G8B8A8_SRGB;
  // 1024x1024の
  image_create_info.extent.width = 1024;
  image_create_info.extent.height = 1024;
  image_create_info.extent.depth = 1;
  // ミップマップは無く
  image_create_info.mipLevels = 1u;
  // レイヤーは1枚だけの
  image_create_info.arrayLayers = 1u;
  // 1テクセルにつきサンプリング点を1つだけ持つ
  image_create_info.samples = VkSampleCountFlagBits::VK_SAMPLE_COUNT_1_BIT;
  // GPUが読みやすいように配置された
  image_create_info.tiling = VkImageTiling::VK_IMAGE_TILING_OPTIMAL;
  // 転送先とストレージイメージに使う
  image_create_info.usage =
    VkImageUsageFlagBits::VK_IMAGE_USAGE_TRANSFER_DST_BIT |
    VkImageUsageFlagBits::VK_IMAGE_USAGE_STORAGE_BIT;
  // 同時に複数のキューから操作しない
  image_create_info.sharingMode = 
  image_create_info.sharingMode = VkSharingMode::VK_SHARING_MODE_EXCLUSIVE;
  image_create_info.queueFamilyIndexCount = 0;
  image_create_info.pQueueFamilyIndices = nullptr;
  // 初期状態は不定な
  image_create_info.initialLayout = VkImageLayout::VK_IMAGE_LAYOUT_UNDEFINED;
  VkImage image;

  if( vkCreateImage(
    device,
    &image_create_info,
    nullptr,
     &image
  ) != VK_SUCCESS ) std::abort();

  // イメージに必要なメモリの要件を調べる
  VkMemoryRequirements image_memory_reqs;
  vkGetImageMemoryRequirements(
    device,
    image,
    &image_memory_reqs
  );

  // このイメージに必要なメモリのサイズ、アラインメント、利用可能なメモリタイプを取得
  VkPhysicalDeviceMemoryProperties memory_props;
  vkGetPhysicalDeviceMemoryProperties( physical_device, &memory_props );
  
  std::cout << nlohmann::json( image_memory_reqs ).dump( 2 ) << std::endl;
  
  // 利用可能なメモリタイプの中からStorageImageに使えてGPUから触るのに適した物を選ぶ
  // VideoCore VIにはメモリタイプが1つしか無いのでこれは必ず0になる
  std::uint32_t memory_index = 0u;
  for( std::uint32_t i = 0u; i != memory_props.memoryTypeCount; ++i ) {
    if( memory_props.memoryTypes[ i ].propertyFlags & VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT ) {
      if( ( image_memory_reqs.memoryTypeBits >> i ) & 0x1 ) {
        memory_index = i;
        break;
      }
    }
  }
  
  std::cout << memory_index << "番目のメモリタイプが選ばれました" << std::endl;

  // イメージ用のメモリを確保する
  VkMemoryAllocateInfo device_memory_allocate_info;
  device_memory_allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  device_memory_allocate_info.pNext = nullptr;
  // このサイズのメモリを
  device_memory_allocate_info.allocationSize = image_memory_reqs.size;
  // このメモリタイプで
  device_memory_allocate_info.memoryTypeIndex = memory_index;
  VkDeviceMemory device_memory;
  if( vkAllocateMemory(
    device,
    &device_memory_allocate_info,
    nullptr,
    &device_memory
  ) != VK_SUCCESS ) std::abort();

  // メモリをイメージに結びつける
  if( vkBindImageMemory(
    device,
    image,
    device_memory,
    0u
  ) != VK_SUCCESS ) std::abort();

  // イメージを捨てる
  vkDestroyImage(
    device,
    image,
    nullptr
  );

  // イメージ用のメモリを捨てる
  vkFreeMemory(
    device,
    device_memory,
    nullptr
  );
}

