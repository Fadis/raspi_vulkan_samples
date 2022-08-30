#include <iostream>
#include <gct/instance.hpp>
#include <gct/device.hpp>
#include <gct/device_create_info.hpp>
#include <gct/vk_mem_alloc.h>
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
 
  std::uint32_t buffer_size = 6u * sizeof( float );

  // バッファを作る
  VmaAllocationCreateInfo buffer_alloc_info = {};
  // CPUからGPUへの転送に適したバッファが欲しい
  buffer_alloc_info.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

  VkBufferCreateInfo buffer_create_info;
  buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  buffer_create_info.pNext = nullptr;
  buffer_create_info.flags = 0;
  // float 6個を置くのに十分なサイズがあり
  buffer_create_info.size = buffer_size;
  // StorageBufferとして使える
  buffer_create_info.usage = VkBufferUsageFlagBits::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
  // 複数のキューから操作される事を考慮しなくて良い
  buffer_create_info.sharingMode = VkSharingMode::VK_SHARING_MODE_EXCLUSIVE;
  buffer_create_info.queueFamilyIndexCount = 0;
  buffer_create_info.pQueueFamilyIndices = nullptr;
  VkBuffer buffer;
  VmaAllocation buffer_allocation;
  if( vmaCreateBuffer(
    allocator,
    &buffer_create_info,
    &buffer_alloc_info,
    &buffer,
    &buffer_allocation,
    nullptr
  ) != VK_SUCCESS ) std::abort();

  // バッファをプロセスのアドレス空間にマップする
  void* mapped = nullptr;
  if( vmaMapMemory(
    allocator, buffer_allocation, &mapped
  ) != VK_SUCCESS ) std::abort();

  // メモリに適当な値を書く
  std::vector< float > data{ 0.f, 1.f, 2.f, 3.f, 4.f, 5.f };
  std::copy(
    data.begin(),
    data.end(),
    reinterpret_cast< float* >( mapped )
  );

  // バッファをプロセスのアドレス空間から外す
  vmaUnmapMemory( allocator, buffer_allocation );

  // バッファを捨てる
  vmaDestroyBuffer( allocator, buffer, buffer_allocation );
  
  // アロケータを捨てる
  vmaDestroyAllocator( allocator );
}

