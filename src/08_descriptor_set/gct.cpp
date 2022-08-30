#include <iostream>
#include <gct/instance.hpp>
#include <gct/device.hpp>
#include <gct/allocator.hpp>
#include <gct/device_create_info.hpp>
#include <gct/descriptor_pool.hpp>
#include <gct/descriptor_set_layout.hpp>
#include <gct/write_descriptor_set.hpp>
#include <gct/shader_module.hpp>

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
  const auto allocator = device->get_allocator();
  std::uint32_t buffer_size = 6u * sizeof( float );
  const auto buffer = allocator->create_buffer(
    gct::buffer_create_info_t()
      .set_basic(
        vk::BufferCreateInfo()
          .setSize( buffer_size )
          .setUsage( vk::BufferUsageFlagBits::eStorageBuffer )
      ),
    VMA_MEMORY_USAGE_CPU_TO_GPU
  );

  // シェーダモジュールを作る
  const auto shader = device->get_shader_module(
    // このファイルから読む
    CMAKE_CURRENT_BINARY_DIR "/shader.comp.spv"
  );


  // デスクリプタプールを作る
  const auto descriptor_pool = device->get_descriptor_pool(
    gct::descriptor_pool_create_info_t()
      .set_basic(
        vk::DescriptorPoolCreateInfo()
          // vkFreeDescriptorSetでデスクリプタセットを解放できる
          .setFlags( vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet )
          // 最大1セットのデスクリプタセットを確保できる
          .setMaxSets( 1 )
      )
      // デスクリプタセット内にはストレージバッファのデスクリプタが最大1個
      .set_descriptor_pool_size( vk::DescriptorType::eStorageBuffer, 1 )
      .rebuild_chain()
  );

  // デスクリプタセットを作る
  const auto descriptor_set = descriptor_pool->allocate(
    // デスクリプタセットレイアウトを作る
    device->get_descriptor_set_layout(
      gct::descriptor_set_layout_create_info_t()
        // このシェーダに必要なデスクリプタを追加
        .add_binding( shader->get_props().get_reflection() )
        .rebuild_chain()
    )
  );
  
  // デスクリプタの内容を更新
  descriptor_set->update(
    {
      gct::write_descriptor_set_t()
        .set_basic(
          // シェーダ上でlayout1という名前のデスクリプタを
          (*descriptor_set)[ "layout1" ]
        )
        .add_buffer(
          gct::descriptor_buffer_info_t()
            // このバッファのものに変更する
            .set_buffer( buffer )
            .set_basic(
              vk::DescriptorBufferInfo()
                // バッファの先頭から
                .setOffset( 0 )
                // 24バイトの範囲をシェーダから触れるようにする
                .setRange( buffer_size )
            )
        )
    }
  );

}

