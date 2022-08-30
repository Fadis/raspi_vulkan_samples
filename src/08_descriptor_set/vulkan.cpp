#include <iostream>
#include <gct/instance.hpp>
#include <gct/device.hpp>
#include <gct/allocator.hpp>
#include <gct/device_create_info.hpp>
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

  // デスクリプタプールを作る
  VkDescriptorPoolSize descriptor_pool_size;
  // デスクリプタセット内にはストレージバッファのデスクリプタが最大1個
  descriptor_pool_size.type = VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  descriptor_pool_size.descriptorCount = 1;
  VkDescriptorPoolCreateInfo descriptor_pool_create_info;
  descriptor_pool_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  descriptor_pool_create_info.pNext = nullptr;
  // vkFreeDescriptorSetでデスクリプタセットを解放できる
  descriptor_pool_create_info.flags = VkDescriptorPoolCreateFlagBits::VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
  // 最大1セットのデスクリプタセットを確保できる
  descriptor_pool_create_info.maxSets = 1;
  descriptor_pool_create_info.poolSizeCount = 1u;
  descriptor_pool_create_info.pPoolSizes = &descriptor_pool_size;
  VkDescriptorPool descriptor_pool;
  if( vkCreateDescriptorPool(
    device,
    &descriptor_pool_create_info,
    nullptr,
    &descriptor_pool
  ) != VK_SUCCESS ) std::abort();

  // 必要なデスクリプタを指定
  VkDescriptorSetLayoutBinding descriptor_set_layout_binding;
  // binding=0に結びつける
  descriptor_set_layout_binding.binding = 0;
  // ストレージバッファのデスクリプタが
  descriptor_set_layout_binding.descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  // 1個あって
  descriptor_set_layout_binding.descriptorCount = 1u;
  // コンピュートシェーダで使える
  descriptor_set_layout_binding.stageFlags = VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT;
  descriptor_set_layout_binding.pImmutableSamplers = nullptr;
  
  // デスクリプタセットレイアウトを作る
  VkDescriptorSetLayoutCreateInfo descriptor_set_create_info;
  descriptor_set_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  descriptor_set_create_info.pNext = nullptr;
  descriptor_set_create_info.flags = 0u;
  // bindingが1つ
  descriptor_set_create_info.bindingCount = 1u;
  // 各bindingの設定はこれ
  descriptor_set_create_info.pBindings = &descriptor_set_layout_binding;
  VkDescriptorSetLayout descriptor_set_layout;
  if( vkCreateDescriptorSetLayout(
    device,
    &descriptor_set_create_info,
    nullptr,
    &descriptor_set_layout
  ) != VK_SUCCESS ) std::abort();

  // デスクリプタセットを作る
  VkDescriptorSetAllocateInfo descriptor_set_allocate_info;
  descriptor_set_allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  descriptor_set_allocate_info.pNext = nullptr;
  // このデスクリプタプールから
  descriptor_set_allocate_info.descriptorPool = descriptor_pool;
  // 1セット
  descriptor_set_allocate_info.descriptorSetCount = 1u;
  // この内容のデスクリプタセットを
  descriptor_set_allocate_info.pSetLayouts = &descriptor_set_layout;
  VkDescriptorSet descriptor_set;
  if( vkAllocateDescriptorSets(
    device,
    &descriptor_set_allocate_info,
    &descriptor_set
  ) != VK_SUCCESS ) abort();

  // 更新するデスクリプタの情報
  VkDescriptorBufferInfo descriptor_buffer_info;
  // このバッファの
  descriptor_buffer_info.buffer = buffer;
  // 先頭から
  descriptor_buffer_info.offset = 0u;
  // 24バイトの範囲をシェーダから触れるようにする
  descriptor_buffer_info.range = buffer_size;

  // デスクリプタの内容を更新
  VkWriteDescriptorSet write_descriptor_set;
  write_descriptor_set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  write_descriptor_set.pNext = nullptr;
  // このデスクリプタセットの
  write_descriptor_set.dstSet = descriptor_set;
  // binding=0の
  write_descriptor_set.dstBinding = 0u;
  // 0要素目から
  write_descriptor_set.dstArrayElement = 0u;
  // 1個の
  write_descriptor_set.descriptorCount = 1u;
  // ストレージバッファのデスクリプタを
  write_descriptor_set.descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  write_descriptor_set.pImageInfo = nullptr;
  // この内容にする
  write_descriptor_set.pBufferInfo = &descriptor_buffer_info;
  write_descriptor_set.pTexelBufferView = nullptr;
  vkUpdateDescriptorSets(
    device,
    1u,
    &write_descriptor_set,
    0u,
    nullptr
  );

  // デスクリプタセットを解放
  if( vkFreeDescriptorSets(
    device,
    descriptor_pool,
    1u,
    &descriptor_set
  ) != VK_SUCCESS ) abort();

  // デスクリプタセットレイアウトを捨てる
  vkDestroyDescriptorSetLayout(
    device,
    descriptor_set_layout,
    nullptr
  );

  // デスクリプタプールを捨てる
  vkDestroyDescriptorPool(
    device,
    descriptor_pool,
    nullptr
  );
}

