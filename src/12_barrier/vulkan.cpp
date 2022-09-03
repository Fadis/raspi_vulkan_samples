#include <iostream>
#include <nlohmann/json.hpp>
#include <gct/get_extensions.hpp>
#include <gct/instance.hpp>
#include <gct/queue.hpp>
#include <gct/device.hpp>
#include <gct/allocator.hpp>
#include <gct/device_create_info.hpp>
#include <gct/descriptor_pool.hpp>
#include <gct/descriptor_set_layout.hpp>
#include <gct/pipeline_cache.hpp>
#include <gct/pipeline_layout_create_info.hpp>
#include <gct/pipeline_layout.hpp>
#include <gct/submit_info.hpp>
#include <gct/shader_module_create_info.hpp>
#include <gct/shader_module.hpp>
#include <gct/compute_pipeline_create_info.hpp>
#include <gct/compute_pipeline.hpp>
#include <gct/write_descriptor_set.hpp>
#include <vulkan/vulkan.hpp>

struct spec_t {
  std::uint32_t local_x_size = 0u;
  std::uint32_t local_y_size = 0u;
};

struct push_constant_t {
  float value;
};

int main( int argc, const char *argv[] ) {
  const std::shared_ptr< gct::instance_t > gct_instance(
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
 
  const auto gct_device = gct_physical_device.create_device(
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
  const auto gct_queue = gct_device->get_queue( 0u );
  const auto gct_shader = gct_device->get_shader_module(
    CMAKE_CURRENT_BINARY_DIR "/shader.comp.spv"
  );
  const auto gct_descriptor_set_layout = gct_device->get_descriptor_set_layout(
    gct::descriptor_set_layout_create_info_t()
      .add_binding( gct_shader->get_props().get_reflection() )
      .rebuild_chain()
  );

  const auto gct_descriptor_pool = gct_device->get_descriptor_pool(
    gct::descriptor_pool_create_info_t()
      .set_basic(
        vk::DescriptorPoolCreateInfo()
          .setFlags( vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet )
          .setMaxSets( 10 )
      )
      .set_descriptor_pool_size( vk::DescriptorType::eStorageBuffer, 5 )
      .rebuild_chain()
  );
  const auto gct_descriptor_set = gct_descriptor_pool->allocate( gct_descriptor_set_layout );
  const auto gct_pipeline_cache = gct_device->get_pipeline_cache();

  const auto gct_allocator = gct_device->get_allocator();
  std::uint32_t buffer_size = 12u * sizeof( float );
  const auto gct_buffer = gct_allocator->create_buffer(
    gct::buffer_create_info_t()
      .set_basic(
        vk::BufferCreateInfo()
          .setSize( buffer_size )
          .setUsage( vk::BufferUsageFlagBits::eStorageBuffer )
      ),
    VMA_MEMORY_USAGE_CPU_TO_GPU
  );
  {
    auto mapped = gct_buffer->map< float >();
    std::vector< float > data{ 0.f, 1.f, 2.f, 3.f, 4.f, 5.f, 6.f, 7.f, 8.f, 9.f, 10.f, 11.f };
    std::copy(
      data.begin(),
      data.end(),
      mapped.begin()
    );
  }
  gct_descriptor_set->update(
    {
      gct::write_descriptor_set_t()
        .set_basic( (*gct_descriptor_set)[ "layout1" ] )
        .add_buffer(
          gct::descriptor_buffer_info_t()
            .set_buffer( gct_buffer )
            .set_basic(
              vk::DescriptorBufferInfo()
                .setOffset( 0 )
                .setRange( buffer_size )
            )
        )
    }
  );
  const auto instance = VkInstance( **gct_instance );
  const auto physical_device = VkPhysicalDevice( **gct_physical_device.devices[ 0 ] );
  const auto device = VkDevice( **gct_device );
  const auto descriptor_set = VkDescriptorSet( **gct_descriptor_set );
  const auto queue = VkQueue( **gct_queue );
  const auto queue_family_index = gct_queue->get_available_queue_family_index();
  const auto descriptor_set_layout = VkDescriptorSetLayout( **gct_descriptor_set_layout );
  const auto shader = VkShaderModule( **gct_shader );
  const auto pipeline_cache = VkPipelineCache( **gct_pipeline_cache );
  const auto buffer = VkBuffer( **gct_buffer );

  VkPushConstantRange push_constant_range;
  push_constant_range.stageFlags = VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT;
  push_constant_range.offset = 0u;
  push_constant_range.size = sizeof( push_constant_t );
  
  VkDescriptorSetLayout descriptor_set_layout_ = descriptor_set_layout;
  VkPipelineLayoutCreateInfo pipeline_layout_create_info;
  pipeline_layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipeline_layout_create_info.pNext = nullptr;
  pipeline_layout_create_info.flags = 0u;
  pipeline_layout_create_info.setLayoutCount = 1u;
  pipeline_layout_create_info.pSetLayouts = &descriptor_set_layout_;
  pipeline_layout_create_info.pushConstantRangeCount = 1u;
  pipeline_layout_create_info.pPushConstantRanges = &push_constant_range;
  VkPipelineLayout pipeline_layout;
  if( vkCreatePipelineLayout(
    device,
    &pipeline_layout_create_info,
    nullptr,
    &pipeline_layout
  ) != VK_SUCCESS ) abort();

  std::vector< VkSpecializationMapEntry > specialization_map( 2u );
  specialization_map[ 0 ].constantID = 1u;
  specialization_map[ 0 ].offset = offsetof( spec_t, local_x_size );
  specialization_map[ 0 ].size = sizeof( std::uint32_t );
  specialization_map[ 1 ].constantID = 2u;
  specialization_map[ 1 ].offset = offsetof( spec_t, local_y_size );
  specialization_map[ 1 ].size = sizeof( std::uint32_t );
  
  const spec_t specialization_values{ 6, 1 };
 
  VkSpecializationInfo specialization_info;
  specialization_info.mapEntryCount = specialization_map.size();
  specialization_info.pMapEntries = specialization_map.data();
  specialization_info.dataSize = sizeof( spec_t );
  specialization_info.pData = &specialization_values;

  VkComputePipelineCreateInfo pipeline_create_info;
  pipeline_create_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
  pipeline_create_info.pNext = nullptr;
  pipeline_create_info.flags = 0u;
  pipeline_create_info.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  pipeline_create_info.stage.pNext = nullptr;
  pipeline_create_info.stage.flags = 0u;
  pipeline_create_info.stage.stage = VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT;
  pipeline_create_info.stage.module = shader;
  pipeline_create_info.stage.pName = "main";
  pipeline_create_info.stage.pSpecializationInfo = &specialization_info;
  pipeline_create_info.layout = pipeline_layout;
  pipeline_create_info.basePipelineHandle = VK_NULL_HANDLE;
  pipeline_create_info.basePipelineIndex = 0;

  VkPipeline pipeline;
  if( vkCreateComputePipelines(
    device,
    pipeline_cache,
    1u,
    &pipeline_create_info,
    nullptr,
    &pipeline
  ) != VK_SUCCESS ) abort();
 
  VkCommandPoolCreateInfo command_pool_create_info;
  command_pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  command_pool_create_info.pNext = nullptr;
  command_pool_create_info.flags = 0u;
  command_pool_create_info.queueFamilyIndex = queue_family_index;
  VkCommandPool command_pool;
  if( vkCreateCommandPool(
    device,
    &command_pool_create_info,
    nullptr,
    &command_pool
  ) != VK_SUCCESS ) abort();

  VkCommandBufferAllocateInfo command_buffer_allocate_info;
  command_buffer_allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  command_buffer_allocate_info.pNext = nullptr;
  command_buffer_allocate_info.commandPool = command_pool;
  command_buffer_allocate_info.level =
    VkCommandBufferLevel::VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  command_buffer_allocate_info.commandBufferCount = 1u;
  VkCommandBuffer command_buffer;
  if( vkAllocateCommandBuffers(
    device,
    &command_buffer_allocate_info,
    &command_buffer
  ) != VK_SUCCESS ) abort();
  VkFenceCreateInfo fence_create_info;
  fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fence_create_info.pNext = nullptr;
  fence_create_info.flags = 0u;
  VkFence fence;
  if( vkCreateFence(
    device,
    &fence_create_info,
    nullptr,
    &fence
  ) != VK_SUCCESS ) abort();

  push_constant_t push_constant;
  push_constant.value = 3.f;

  VkCommandBufferBeginInfo command_buffer_begin_info;
  command_buffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  command_buffer_begin_info.pNext = nullptr;
  command_buffer_begin_info.flags = 0u;
  command_buffer_begin_info.pInheritanceInfo = nullptr;
  if( vkBeginCommandBuffer(
    command_buffer,
    &command_buffer_begin_info
  ) != VK_SUCCESS ) abort();

  VkDescriptorSet raw_descriptor_set = descriptor_set;
  vkCmdBindDescriptorSets(
    command_buffer,
    VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_COMPUTE,
    pipeline_layout,
    0u,
    1u,
    &raw_descriptor_set,
    0u,
    nullptr
  );

  vkCmdBindPipeline(
    command_buffer,
    VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_COMPUTE,
    pipeline
  );

  // 以降のDispatchではプッシュコンスタントの値を3にする
  vkCmdPushConstants(
    command_buffer,
    pipeline_layout,
    VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT,
    0u,
    sizeof( push_constant_t ),
    reinterpret_cast< const void* >( &push_constant )
  );

  // 6個の値に対して実行する
  vkCmdDispatch(
    command_buffer,
    1, 1, 1
  );

  // ここまでのbufferを触るシェーダが完了するまで以降のbufferを触るシェーダは
  // 開始してはいけない
  // 要するに上のDispatchが完了するまで下のDispatchが始まらないようにする
  {
    VkBufferMemoryBarrier buffer_memory_barrier;
    buffer_memory_barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    buffer_memory_barrier.pNext = nullptr;
    buffer_memory_barrier.srcAccessMask = VkAccessFlagBits::VK_ACCESS_SHADER_WRITE_BIT;
    buffer_memory_barrier.dstAccessMask = VkAccessFlagBits::VK_ACCESS_SHADER_READ_BIT;
    buffer_memory_barrier.srcQueueFamilyIndex = 0u;
    buffer_memory_barrier.dstQueueFamilyIndex = 0u;
    buffer_memory_barrier.buffer = buffer;
    buffer_memory_barrier.offset = 0u;
    buffer_memory_barrier.size = buffer_size;
    vkCmdPipelineBarrier(
      command_buffer,
      VkPipelineStageFlagBits::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
      VkPipelineStageFlagBits::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
      VkDependencyFlagBits::VK_DEPENDENCY_BY_REGION_BIT,
      0u,
      nullptr,
      1u,
      &buffer_memory_barrier,
      0u,
      nullptr
    );
  }

  // 以降のDispatchではプッシュコンスタントの値を2にする
  push_constant.value = 2.f;
  vkCmdPushConstants(
    command_buffer,
    pipeline_layout,
    VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT,
    0u,
    sizeof( push_constant_t ),
    reinterpret_cast< const void* >( &push_constant )
  );

  // 12個の値に対して実行する
  vkCmdDispatch(
    command_buffer,
    2, 1, 1
  );

  if( vkEndCommandBuffer(
    command_buffer
  ) != VK_SUCCESS ) abort();

  VkSubmitInfo submit_info;
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit_info.pNext = nullptr;
  submit_info.waitSemaphoreCount = 0u;
  submit_info.pWaitSemaphores = nullptr;
  submit_info.pWaitDstStageMask = nullptr;
  submit_info.commandBufferCount = 1u;
  submit_info.pCommandBuffers = &command_buffer;
  submit_info.signalSemaphoreCount = 0u;
  submit_info.pSignalSemaphores = nullptr;
  if( vkQueueSubmit(
    queue,
    1u,
    &submit_info,
    fence
  ) != VK_SUCCESS ) abort();

  if( vkWaitForFences(
    device,
    1u,
    &fence,
    true,
    1 * 1000 * 1000 * 1000
  ) != VK_SUCCESS ) abort();

  vkDestroyFence(
    device,
    fence,
    nullptr
  );

  vkFreeCommandBuffers(
    device,
    command_pool,
    1u,
    &command_buffer
  );

  vkDestroyCommandPool(
    device,
    command_pool,
    nullptr
  );

  std::vector< float > host;
  host.reserve( 6 );
  {
    auto mapped = gct_buffer->map< float >();
    std::copy( mapped.begin(), mapped.end(), std::back_inserter( host ) );
  }
  unsigned int count;
  nlohmann::json json = host;
  std::cout << json.dump( 2 ) << std::endl;
}

