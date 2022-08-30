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
  const auto gct_pipeline_layout = gct_device->get_pipeline_layout(
    gct::pipeline_layout_create_info_t()
      .add_descriptor_set_layout( gct_descriptor_set_layout )
  );

  const auto gct_descriptor_pool = gct_device->get_descriptor_pool(
    gct::descriptor_pool_create_info_t()
      .set_basic(
        vk::DescriptorPoolCreateInfo()
          .setFlags( vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet )
          .setMaxSets( 1 )
      )
      .set_descriptor_pool_size( vk::DescriptorType::eStorageBuffer, 1 )
      .rebuild_chain()
  );
  const auto gct_descriptor_set = gct_descriptor_pool->allocate( gct_descriptor_set_layout );
  const auto gct_pipeline_cache = gct_device->get_pipeline_cache();
  const auto gct_pipeline = gct_pipeline_cache->get_pipeline(
    gct::compute_pipeline_create_info_t()
      .set_stage(
        gct::pipeline_shader_stage_create_info_t()
          .set_shader_module( gct_shader )
          .set_specialization_info(
            gct::specialization_info_t< spec_t >()
              .set_data(
                spec_t{ 256, 1 }
              )
              .add_map< std::uint32_t >( 1, offsetof( spec_t, local_x_size ) )
              .add_map< std::uint32_t >( 2, offsetof( spec_t, local_y_size ) )
          )
      )
      .set_layout( gct_pipeline_layout )
  );
  const auto gct_allocator = gct_device->get_allocator();
  std::uint32_t buffer_size = 256u * sizeof( std::uint32_t );
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
    auto mapped = gct_buffer->map< std::uint32_t >();
    std::iota(
      mapped.begin(),
      mapped.end(),
      0
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
  const auto instance = **gct_instance;
  const auto physical_device = **gct_physical_device.devices[ 0 ];
  const auto device = **gct_device;
  const auto descriptor_set = **gct_descriptor_set;
  const auto queue = **gct_queue;
  const auto queue_family_index = gct_queue->get_available_queue_family_index();
  const auto pipeline_layout = **gct_pipeline_layout;
  const auto pipeline = **gct_pipeline;

 
  // コマンドプールを作る 
  VkCommandPoolCreateInfo command_pool_create_info;
  command_pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  command_pool_create_info.pNext = nullptr;
  command_pool_create_info.flags = 0u;
  // このキューファミリ用のやつを
  command_pool_create_info.queueFamilyIndex = queue_family_index;
  VkCommandPool command_pool;
  if( vkCreateCommandPool(
    device,
    &command_pool_create_info,
    nullptr,
    &command_pool
  ) != VK_SUCCESS ) abort();

  // コマンドバッファを確保する
  VkCommandBufferAllocateInfo command_buffer_allocate_info;
  command_buffer_allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  command_buffer_allocate_info.pNext = nullptr;
  // このコマンドプールから
  command_buffer_allocate_info.commandPool = command_pool;
  // 直接キューにsubmitする用のやつを
  command_buffer_allocate_info.level =
    VkCommandBufferLevel::VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  // 1個
  command_buffer_allocate_info.commandBufferCount = 1u;
  VkCommandBuffer command_buffer;
  if( vkAllocateCommandBuffers(
    device,
    &command_buffer_allocate_info,
    &command_buffer
  ) != VK_SUCCESS ) abort();
  // フェンスを作る
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

  // コマンドバッファにコマンドの記録を開始する
  VkCommandBufferBeginInfo command_buffer_begin_info;
  command_buffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  command_buffer_begin_info.pNext = nullptr;
  command_buffer_begin_info.flags = 0u;
  command_buffer_begin_info.pInheritanceInfo = nullptr;
  if( vkBeginCommandBuffer(
    command_buffer,
    &command_buffer_begin_info
  ) != VK_SUCCESS ) abort();

  // 以降のパイプラインの実行ではこのデスクリプタセットを使う
  VkDescriptorSet raw_descriptor_set = descriptor_set;
  vkCmdBindDescriptorSets(
    command_buffer,
    // コンピュートパイプラインの実行に使うデスクリプタセットを
    VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_COMPUTE,
    pipeline_layout,
    0u,
    1u,
    // これにする
    &raw_descriptor_set,
    0u,
    nullptr
  );

  // 以降のパイプラインの実行ではこのパイプラインを使う
  vkCmdBindPipeline(
    command_buffer,
    // コンピュートパイプラインを
    VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_COMPUTE,
    // これにする
    pipeline
  );

  // コンピュートパイプラインを実行する
  vkCmdDispatch(
    command_buffer,
    1, 1, 1
  );

  // コマンドバッファにコマンドの記録を終了する
  if( vkEndCommandBuffer(
    command_buffer
  ) != VK_SUCCESS ) abort();

  // コマンドバッファの内容をキューに流す
  VkSubmitInfo submit_info;
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit_info.pNext = nullptr;
  submit_info.waitSemaphoreCount = 0u;
  submit_info.pWaitSemaphores = nullptr;
  submit_info.pWaitDstStageMask = nullptr;
  submit_info.commandBufferCount = 1u;
  // このコマンドバッファの内容を流す
  submit_info.pCommandBuffers = &command_buffer;
  submit_info.signalSemaphoreCount = 0u;
  submit_info.pSignalSemaphores = nullptr;
  if( vkQueueSubmit(
    queue,
    1u,
    &submit_info,
    // 実行し終わったらこのフェンスに通知
    fence
  ) != VK_SUCCESS ) abort();

  // フェンスが完了通知を受けるのを待つ
  if( vkWaitForFences(
    device,
    1u,
    // このフェンスを待つ
    &fence,
    // 全部のフェンスに完了通知が来るまで待つ
    true,
    // 1秒でタイムアウト
    1 * 1000 * 1000 * 1000
  ) != VK_SUCCESS ) abort();

  // フェンスを捨てる
  vkDestroyFence(
    device,
    fence,
    nullptr
  );

  // コマンドバッファを捨てる
  vkFreeCommandBuffers(
    device,
    command_pool,
    1u,
    &command_buffer
  );

  // コマンドプールを捨てる
  vkDestroyCommandPool(
    device,
    command_pool,
    nullptr
  );

  std::vector< std::uint32_t > host;
  host.reserve( 256 );
  {
    auto mapped = gct_buffer->map< std::uint32_t >();
    std::copy( mapped.begin(), mapped.end(), std::back_inserter( host ) );
  }
  unsigned int count;
  nlohmann::json json = host;
  std::cout << json.dump( 2 ) << std::endl;
}

