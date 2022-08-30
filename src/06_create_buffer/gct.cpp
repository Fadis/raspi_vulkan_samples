#include <iostream>
#include <nlohmann/json.hpp>
#include <gct/get_extensions.hpp>
#include <gct/instance.hpp>
#include <gct/queue.hpp>
#include <gct/device.hpp>
#include <gct/allocator.hpp>
#include <gct/device_create_info.hpp>
#include <gct/buffer.hpp>


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
  auto physical_device = groups[ 0 ].with_extensions( {} );
  auto device =
    physical_device
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

  // アロケータを作る
  const auto allocator = device->get_allocator();
  
  std::uint32_t buffer_size = 6u * sizeof( float );

  // バッファを作る
  const auto buffer = allocator->create_buffer(
    gct::buffer_create_info_t()
      .set_basic(
        vk::BufferCreateInfo()
          // float 6個を置くのに十分なサイズがあり
          .setSize( buffer_size )
          // StorageBufferとして使える
          .setUsage( vk::BufferUsageFlagBits::eStorageBuffer )
      ),
    // CPUからGPUへの転送に適したバッファが欲しい
    VMA_MEMORY_USAGE_CPU_TO_GPU
  );
  
  {
    // バッファをプロセスのアドレス空間にマップする
    auto mapped = buffer->map< float >();
    std::vector< float > data{ 0.f, 1.f, 2.f, 3.f, 4.f, 5.f };
    std::copy(
      data.begin(),
      data.end(),
      mapped.begin()
    );
  }
}

