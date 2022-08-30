#include <iostream>
#include <gct/instance.hpp>
#include <gct/device.hpp>
#include <gct/allocator.hpp>
#include <gct/device_create_info.hpp>
#include <vulkan/vulkan.hpp>

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
  const auto allocator = gct_device->get_allocator();
  std::uint32_t buffer_size = 6u * sizeof( float );
  const auto gct_buffer = allocator->create_buffer(
    gct::buffer_create_info_t()
      .set_basic(
        vk::BufferCreateInfo()
          .setSize( buffer_size )
          .setUsage( vk::BufferUsageFlagBits::eStorageBuffer )
      ),
    VMA_MEMORY_USAGE_CPU_TO_GPU
  );
  const auto instance = **gct_instance;
  const auto physical_device = **gct_physical_device.devices[ 0 ];
  const auto device = **gct_device;
  const auto buffer = **gct_buffer; 
 
  // デスクリプタセット内にはストレージバッファのデスクリプタが最大1個
  const std::vector< vk::DescriptorPoolSize > descriptor_pool_size{
    vk::DescriptorPoolSize()
      .setType( vk::DescriptorType::eStorageBuffer )
      .setDescriptorCount( 1 )
  };

  // デスクリプタプールを作る
  const auto descriptor_pool = device.createDescriptorPoolUnique(
    vk::DescriptorPoolCreateInfo()
      // vkFreeDescriptorSetでデスクリプタセットを解放できる
      .setFlags( vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet )
      // 最大1セットのデスクリプタセットを確保できる
      .setMaxSets( 1 )
      .setPoolSizeCount( descriptor_pool_size.size() )
      .setPPoolSizes( descriptor_pool_size.data() )
  );
  
  // 必要なデスクリプタを指定
  const auto descriptor_set_layout_create_info =
    vk::DescriptorSetLayoutBinding()
      // binding=0に結びつける
      .setBinding( 0 )
      // ストレージバッファのデスクリプタが
      .setDescriptorType( vk::DescriptorType::eStorageBuffer )
      // 1個あって
      .setDescriptorCount( 1 )
      // コンピュートシェーダで使える
      .setStageFlags( vk::ShaderStageFlagBits::eCompute );
  
  // デスクリプタセットレイアウトを作る
  const auto descriptor_set_layout = device.createDescriptorSetLayoutUnique(
    vk::DescriptorSetLayoutCreateInfo()
      // bindingが1つ
      .setBindingCount( 1u )
      // 各bindingの設定はこれ
      .setPBindings( &descriptor_set_layout_create_info )
  );

  // デスクリプタセットを作る
  const auto descriptor_set = std::move( device.allocateDescriptorSetsUnique(
    vk::DescriptorSetAllocateInfo()
      // このデスクリプタプールから
      .setDescriptorPool( *descriptor_pool )
      // 1セット
      .setDescriptorSetCount( 1 )
      // この内容のデスクリプタセットを
      .setPSetLayouts( &*descriptor_set_layout )
  )[ 0 ] );

  // 更新するデスクリプタの情報
  const auto descriptor_buffer_info =
    vk::DescriptorBufferInfo()
      // このバッファの
      .setBuffer( buffer )
      // 先頭から
      .setOffset( 0 )
      // 24バイトの範囲をシェーダから触れるようにする
      .setRange( buffer_size );

  // デスクリプタの内容を更新
  device.updateDescriptorSets(
    {
      vk::WriteDescriptorSet()
        // このデスクリプタセットの
        .setDstSet( *descriptor_set )
        // binding=0の
        .setDstBinding( 0 )
        // 0要素目から
        .setDstArrayElement( 0 )
        // 1個の
        .setDescriptorCount( 1u )
        // ストレージバッファのデスクリプタを
        .setDescriptorType( vk::DescriptorType::eStorageBuffer )
        // この内容にする
        .setPBufferInfo( &descriptor_buffer_info )
    },
    {}
  );

}

