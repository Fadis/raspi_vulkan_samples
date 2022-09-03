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
  const auto instance = **gct_instance;
  const auto physical_device = **gct_physical_device.devices[ 0 ];
  const auto device = **gct_device;
  const auto descriptor_set = **gct_descriptor_set;
  const auto queue = **gct_queue;
  const auto queue_family_index = gct_queue->get_available_queue_family_index();
  const auto descriptor_set_layout = **gct_descriptor_set_layout;
  const auto shader = **gct_shader;
  const auto pipeline_cache = **gct_pipeline_cache;
  const auto buffer = **gct_buffer;

  auto push_constant_range = vk::PushConstantRange()
    .setStageFlags( vk::ShaderStageFlagBits::eCompute )
    .setOffset( 0 )
    .setSize( sizeof( push_constant_t ) );

  auto pipeline_layout = device.createPipelineLayoutUnique(
    vk::PipelineLayoutCreateInfo()
      .setSetLayoutCount( 1u )
      .setPSetLayouts( &descriptor_set_layout )
      .setPushConstantRangeCount( 1u )
      .setPPushConstantRanges( &push_constant_range )
  );

  const std::vector< vk::SpecializationMapEntry > specialization_map{
    vk::SpecializationMapEntry()
      .setConstantID( 1 )
      .setOffset( offsetof( spec_t, local_x_size ) )
      .setSize( sizeof( std::uint32_t ) ),
    vk::SpecializationMapEntry()
      .setConstantID( 2 )
      .setOffset( offsetof( spec_t, local_y_size ) )
      .setSize( sizeof( std::uint32_t ) )
  };
  
  const spec_t specialization_values{ 6, 1 };
  
  const auto specialization_info = vk::SpecializationInfo()
    .setMapEntryCount( specialization_map.size() )
    .setPMapEntries( specialization_map.data() )
    .setDataSize( sizeof( spec_t ) )
    .setPData( &specialization_values );

  const std::vector< vk::ComputePipelineCreateInfo > pipeline_create_info{
    vk::ComputePipelineCreateInfo()
    .setStage(
      vk::PipelineShaderStageCreateInfo()
        .setStage( vk::ShaderStageFlagBits::eCompute )
        .setModule( shader )
        .setPName( "main" )
        .setPSpecializationInfo(
          &specialization_info
        )
    )
    .setLayout( *pipeline_layout )
  };
  auto wrapped = device.createComputePipelinesUnique(
    VK_NULL_HANDLE,
    pipeline_create_info
  );

  if( wrapped.result != vk::Result::eSuccess )
    vk::throwResultException( wrapped.result, "createComputePipeline failed" );
  auto pipeline = std::move( wrapped.value[ 0 ] );

  const auto command_pool = device.createCommandPoolUnique(
    vk::CommandPoolCreateInfo()
      .setQueueFamilyIndex( queue_family_index )
  );
  auto command_buffers = device.allocateCommandBuffersUnique(
    vk::CommandBufferAllocateInfo()
      .setCommandPool( *command_pool )
      .setLevel( vk::CommandBufferLevel::ePrimary )
      .setCommandBufferCount( 1u )
  );
  const auto command_buffer = std::move( command_buffers[ 0 ] );
  const auto fence = device.createFenceUnique(
    vk::FenceCreateInfo()
  );

  push_constant_t push_constant;
  push_constant.value = 3.f;

  command_buffer->begin(
    vk::CommandBufferBeginInfo()
  );

  command_buffer->bindDescriptorSets(
    vk::PipelineBindPoint::eCompute,
    *pipeline_layout,
    0u,
    { descriptor_set },
    {}
  );

  command_buffer->bindPipeline(
    vk::PipelineBindPoint::eCompute,
    *pipeline
  );

  // 以降のDispatchではプッシュコンスタントの値を3にする
  command_buffer->pushConstants(
    *pipeline_layout,
    vk::ShaderStageFlagBits::eCompute,
    0u,
    sizeof( push_constant_t ),
    reinterpret_cast< const void* >( &push_constant )
  );

  // 6個の値に対して実行する
  command_buffer->dispatch( 1, 1, 1 );

  // ここまでのbufferを触るシェーダが完了するまで以降のbufferを触るシェーダは
  // 開始してはいけない
  // 要するに上のDispatchが完了するまで下のDispatchが始まらないようにする
  {
    command_buffer->pipelineBarrier(
      vk::PipelineStageFlagBits::eComputeShader,
      vk::PipelineStageFlagBits::eComputeShader,
      vk::DependencyFlagBits( 0 ),
      {},
      {
        vk::BufferMemoryBarrier()
          .setSrcAccessMask( vk::AccessFlagBits::eShaderWrite )
          .setDstAccessMask( vk::AccessFlagBits::eShaderRead )
          .setBuffer( buffer )
	  .setOffset( 0 )
	  .setSize( buffer_size )
      },
      {}
    );
  }

  // 以降のDispatchではプッシュコンスタントの値を2にする
  push_constant.value = 2.f;
  command_buffer->pushConstants(
    *pipeline_layout,
    vk::ShaderStageFlagBits::eCompute,
    0u,
    sizeof( push_constant_t ),
    reinterpret_cast< const void* >( &push_constant )
  );

  // 12個の値に対して実行する
  command_buffer->dispatch( 2, 1, 1 );

  command_buffer->end();

  queue.submit(
    {
      vk::SubmitInfo()
        .setCommandBufferCount( 1u )
        .setPCommandBuffers( &*command_buffer )
    },
    *fence
  );

  if( device.waitForFences(
    {
      *fence
    },
    true,
    1*1000*1000*1000
  ) != vk::Result::eSuccess ) abort();

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

