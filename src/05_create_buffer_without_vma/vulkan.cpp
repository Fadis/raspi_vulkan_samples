#include <iostream>
#include <vector>
#include <vulkan/vulkan.h>
#include <gct/instance.hpp>
#include <gct/device.hpp>
#include <gct/physical_device.hpp>
#include <gct/device_create_info.hpp>
#include <gct/instance_create_info.hpp>
#include <vulkan2json/MemoryRequirements.hpp>

int main( int argc, const char *argv[] ) {
  std::shared_ptr< gct::instance_t > instance(
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
  auto groups = instance->get_physical_devices( {} );
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
  auto physical_device = VkPhysicalDevice( **gct_physical_device.devices[ 0 ] );
  const auto device = VkDevice( **gct_device );

  VkPhysicalDeviceMemoryProperties memory_props;
  vkGetPhysicalDeviceMemoryProperties(
    physical_device,
    &memory_props
  );

  std::uint32_t buffer_size = 6u * sizeof( float );

  // バッファを作る
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
  if( vkCreateBuffer(
    device,
    &buffer_create_info,
    nullptr,
    &buffer
  ) != VK_SUCCESS ) std::abort();

  // バッファに必要なメモリの要件を調べる
  VkMemoryRequirements buffer_memory_reqs;
  vkGetBufferMemoryRequirements(
    device,
    buffer,
    &buffer_memory_reqs
  );
  std::cout << buffer_size << "バイトのデータを置くバッファに必要なメモリの要件" << std::endl;
  std::cout << nlohmann::json( buffer_memory_reqs ).dump( 2 ) << std::endl;

  // 利用可能なメモリタイプの中からStorageBufferに使えてCPUから見える物を探す
  // VideoCore VIにはメモリタイプが1つしか無いのでこれは必ず0になる
  std::uint32_t memory_index = 0u;
  for( std::uint32_t i = 0u; i != memory_props.memoryTypeCount; ++i ) {
    // StorageBufferに使えて、かつCPUから見えるメモリタイプ
    if( memory_props.memoryTypes[ i ].propertyFlags & VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT ) {
      if( ( buffer_memory_reqs.memoryTypeBits >> i ) & 0x1 ) {
        memory_index = i;
        break;
      }
    }
  }
  std::cout << memory_index << "番目のメモリタイプが選ばれました" << std::endl;
  
  // バッファ用のメモリを確保する
  VkMemoryAllocateInfo memory_allocate_info;
  memory_allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  memory_allocate_info.pNext = nullptr;
  // このサイズのメモリを
  memory_allocate_info.allocationSize = buffer_memory_reqs.size;
  // このメモリタイプで
  memory_allocate_info.memoryTypeIndex = memory_index;
  VkDeviceMemory memory;
  if( vkAllocateMemory(
    device,
    &memory_allocate_info,
    nullptr,
    &memory
  ) != VK_SUCCESS ) std::abort();

  // メモリをバッファに結びつける
  if( vkBindBufferMemory(
    device,
    buffer,
    memory,
    0u
  ) != VK_SUCCESS ) std::abort();
  
  // メモリをプロセスのアドレス空間にマップする
  void *mapped = nullptr;
  if( vkMapMemory(
    device,
    memory,
    0u, // メモリの先頭から
    buffer_memory_reqs.size, // このサイズの範囲を
    0u,
    &mapped
  ) != VK_SUCCESS ) std::abort();

  // メモリに適当な値を書く
  std::vector< float > data{ 0.f, 1.f, 2.f, 3.f, 4.f, 5.f };
  std::copy(
    data.begin(),
    data.end(),
    reinterpret_cast< float* >( mapped )
  );

  // メモリをプロセスのアドレス空間から外す
  vkUnmapMemory(
    device,
    memory
  );

  // バッファを捨てる
  vkDestroyBuffer( device, buffer, nullptr );
  // バッファ用のメモリを捨てる
  vkFreeMemory( device, memory, nullptr );
}

