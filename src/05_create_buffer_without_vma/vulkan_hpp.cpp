#include <iostream>
#include <vector>
#include <vulkan/vulkan.hpp>
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
  auto physical_device = **gct_physical_device.devices[ 0 ];
  const auto device = **gct_device;

  const auto memory_props = physical_device.getMemoryProperties();

  std::uint32_t buffer_size = 6u * sizeof( float );
  
  // バッファを作る
  auto buffer = device.createBufferUnique(
    vk::BufferCreateInfo()
      // float 6個を置くのに十分なサイズがあり
      .setSize( buffer_size )
      // StorageBufferとして使える
      .setUsage( vk::BufferUsageFlagBits::eStorageBuffer )
  );

  // バッファに必要なメモリの要件を調べる
  auto buffer_memory_reqs = device.getBufferMemoryRequirements( *buffer );
  std::cout << buffer_size << "バイトのデータを置くバッファに必要なメモリの要件" << std::endl;
  std::cout << nlohmann::json( buffer_memory_reqs ).dump( 2 ) << std::endl;

  // 利用可能なメモリタイプの中からStorageBufferに使えてCPUから見える物を探す
  // VideoCore VIにはメモリタイプが1つしか無いのでこれは必ず0になる
  std::uint32_t memory_index = 0u;
  for( std::uint32_t i = 0u; i != memory_props.memoryTypeCount; ++i ) {
    // StorageBufferに使えて、かつCPUから見えるメモリタイプ
    if( memory_props.memoryTypes[ i ].propertyFlags & vk::MemoryPropertyFlagBits::eHostVisible ) {
      if( ( buffer_memory_reqs.memoryTypeBits >> i ) & 0x1 ) {
        memory_index = i;
        break;
      }
    }
  }
  std::cout << memory_index << "番目のメモリタイプが選ばれました" << std::endl;

  
  // バッファ用のメモリを確保する
  auto memory = device.allocateMemoryUnique(
    vk::MemoryAllocateInfo()
      // このサイズのメモリを
      .setAllocationSize( buffer_memory_reqs.size )
      // このメモリタイプで
      .setMemoryTypeIndex( memory_index )
  );

  // メモリをバッファに結びつける
  device.bindBufferMemory(
    *buffer,
    *memory,
    0u
  );
  
  // メモリをプロセスのアドレス空間にマップする
  auto mapped = device.mapMemory(
    *memory,
    0u, // メモリの先頭から
    buffer_memory_reqs.size, // このサイズの範囲を
    vk::MemoryMapFlags( 0u )
  );

  // メモリに適当な値を書く
  std::vector< float > data{ 0.f, 1.f, 2.f, 3.f, 4.f, 5.f };
  std::copy(
    data.begin(),
    data.end(),
    reinterpret_cast< float* >( mapped )
  );

  // メモリをプロセスのアドレス空間から外す
  device.unmapMemory(
    *memory
  );

}

