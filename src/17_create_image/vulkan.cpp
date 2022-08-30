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
  const auto instance = **gct_instance;
  const auto physical_device = **gct_physical_device.devices[ 0 ];
  const auto device = **gct_device;
  const auto memory_props = physical_device.getMemoryProperties();
  
  // アロケータを作る
  VmaAllocatorCreateInfo allocator_create_info;
  // このインスタンスと
  allocator_create_info.instance = instance;
  // この物理デバイスと
  allocator_create_info.physicalDevice = physical_device;
  // この論理デバイスを使う
  allocator_create_info.device = device;
  allocator_create_info.flags = 0u;
  allocator_create_info.preferredLargeHeapBlockSize = 0u;
  allocator_create_info.pAllocationCallbacks = nullptr;
  allocator_create_info.pDeviceMemoryCallbacks = nullptr;
  allocator_create_info.pHeapSizeLimit = nullptr;
  allocator_create_info.pVulkanFunctions = nullptr;
  allocator_create_info.vulkanApiVersion = 0u;
  allocator_create_info.pTypeExternalMemoryHandleTypes = nullptr;
  allocator_create_info.pRecordSettings = nullptr;

  VmaAllocator allocator;
  if( vmaCreateAllocator(
    &allocator_create_info, &allocator
  ) != VK_SUCCESS ) std::abort();
  
  VmaAllocationCreateInfo image_alloc_info = {};
  // GPUのみが読めるイメージが欲しい
  image_alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
  VkImageCreateInfo image_create_info;

  image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  image_create_info.pNext = nullptr;
  image_create_info.flags = 0u;
  // 2次元で
  image_create_info.imageType = VkImageType::VK_IMAGE_TYPE_2D;
  // RGBA各8bitで
  image_create_info.format = VkFormat::VK_FORMAT_R8G8B8A8_UNORM;
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
  VmaAllocation image_allocation;
  if( vmaCreateImage(
    allocator,
    &image_create_info,
    &image_alloc_info,
    &image,
    &image_allocation,
    nullptr
  ) != VK_SUCCESS ) std::abort();

  // イメージを捨てる
  vmaDestroyImage( allocator, image, image_allocation );
  
  // アロケータを捨てる
  vmaDestroyAllocator( allocator );
}

