#include <iostream>
#include <nlohmann/json.hpp>
#include <gct/get_extensions.hpp>
#include <gct/instance.hpp>
#include <gct/queue.hpp>
#include <gct/device.hpp>
#include <gct/allocator.hpp>
#include <gct/device_create_info.hpp>
#include <gct/image_create_info.hpp>
#include <gct/swapchain.hpp>
#include <gct/descriptor_pool.hpp>
#include <gct/descriptor_set_layout.hpp>
#include <gct/pipeline_cache.hpp>
#include <gct/pipeline_layout_create_info.hpp>
#include <gct/pipeline_layout.hpp>
#include <gct/buffer_view_create_info.hpp>
#include <gct/submit_info.hpp>
#include <gct/shader_module_create_info.hpp>
#include <gct/shader_module.hpp>
#include <gct/compute_pipeline_create_info.hpp>
#include <gct/compute_pipeline.hpp>
#include <gct/write_descriptor_set.hpp>

struct spec_t {
  std::uint32_t local_x_size = 0u;
  std::uint32_t local_y_size = 0u;
};

struct push_constant_t {
  float value;
};

int main( int argc, const char *argv[] ) {
  const std::shared_ptr< gct::instance_t > instance(
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
  auto selected = groups[ 0 ].with_extensions( {} );
 
  const auto device = selected.create_device(
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
  const auto queue = device->get_queue( 0u );
  const auto shader = device->get_shader_module(
    CMAKE_CURRENT_BINARY_DIR "/shader.comp.spv"
  );
  const auto descriptor_set_layout = device->get_descriptor_set_layout(
    gct::descriptor_set_layout_create_info_t()
      .add_binding( shader->get_props().get_reflection() )
      .rebuild_chain()
  );
  const auto pipeline_layout = device->get_pipeline_layout(
    gct::pipeline_layout_create_info_t()
      .add_descriptor_set_layout( descriptor_set_layout )
      .add_push_constant_range(
        vk::PushConstantRange()
          .setStageFlags( vk::ShaderStageFlagBits::eCompute )
          .setOffset( 0 )
          .setSize( sizeof( push_constant_t ) )
      )
  );

  const auto descriptor_pool = device->get_descriptor_pool(
    gct::descriptor_pool_create_info_t()
      .set_basic(
        vk::DescriptorPoolCreateInfo()
          .setFlags( vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet )
          .setMaxSets( 10 )
      )
      .set_descriptor_pool_size( vk::DescriptorType::eStorageBuffer, 5 )
      .rebuild_chain()
  );
  const auto descriptor_set = descriptor_pool->allocate( descriptor_set_layout );
  const auto pipeline_cache = device->get_pipeline_cache();
  const auto pipeline = pipeline_cache->get_pipeline(
    gct::compute_pipeline_create_info_t()
      .set_stage(
        gct::pipeline_shader_stage_create_info_t()
          .set_shader_module( shader )
          .set_specialization_info(
            gct::specialization_info_t< spec_t >()
              .set_data(
                spec_t{ 6, 1 }
              )
              .add_map< std::uint32_t >( 1, offsetof( spec_t, local_x_size ) )
              .add_map< std::uint32_t >( 2, offsetof( spec_t, local_y_size ) )
          )
      )
      .set_layout( pipeline_layout )
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
  {
    auto mapped = buffer->map< float >();
    std::vector< float > data{ 0.f, 1.f, 2.f, 3.f, 4.f, 5.f };
    std::copy(
      data.begin(),
      data.end(),
      mapped.begin()
    );
  }
  descriptor_set->update(
    {
      gct::write_descriptor_set_t()
        .set_basic( (*descriptor_set)[ "layout1" ] )
        .add_buffer(
          gct::descriptor_buffer_info_t()
            .set_buffer( buffer )
            .set_basic(
              vk::DescriptorBufferInfo()
                .setOffset( 0 )
                .setRange( buffer_size )
            )
        )
    }
  );

  // コマンドバッファを作る
  const auto command_buffer = queue->get_command_pool()->allocate();

  push_constant_t push_constant;
  push_constant.value = 3.f;

  {
    // コマンドバッファにコマンドの記録を開始する
    auto rec = command_buffer->begin();
    
    // 以降のパイプラインの実行ではこのデスクリプタセットを使う
    rec.bind_descriptor_set(
      // コンピュートパイプラインの実行に使うデスクリプタセットを
      vk::PipelineBindPoint::eCompute,
      pipeline_layout,
      // これにする
      descriptor_set
    );
    
    // 以降のパイプラインの実行ではこのパイプラインを使う
    rec.bind_pipeline(
      // これにする
      pipeline
    );

    rec->pushConstants(
      **pipeline_layout,
      vk::ShaderStageFlagBits::eCompute,
      0u,
      sizeof( push_constant_t ),
      reinterpret_cast< void* >( &push_constant )
    );
    
    // コンピュートパイプラインを実行する
    rec->dispatch( 1, 1, 1 );
  }
  
  // コマンドバッファの内容をキューに流す
  command_buffer->execute(
    gct::submit_info_t()
  );
  
  // コマンドバッファの内容の実行が完了するのを待つ
  command_buffer->wait_for_executed();

  std::vector< float > host;
  host.reserve( 6 );
  {
    // ステージングバッファをプロセスのアドレス空間にマップする
    auto mapped = buffer->map< float >();
    // ステージングバッファからホストのメモリにコピー
    std::copy( mapped.begin(), mapped.end(), std::back_inserter( host ) );
  }
  
  // ホストのメモリの内容をJSONにしてダンプ
  nlohmann::json json = host;
  std::cout << json.dump( 2 ) << std::endl;
}

