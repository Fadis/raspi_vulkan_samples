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
#include <gct/buffer_view_create_info.hpp>
#include <gct/submit_info.hpp>
#include <gct/shader_module_create_info.hpp>
#include <gct/shader_module.hpp>
#include <gct/compute_pipeline_create_info.hpp>
#include <gct/compute_pipeline.hpp>
#include <gct/write_descriptor_set.hpp>
#include <gct/command_buffer.hpp>
#include <gct/command_pool.hpp>

struct spec_t {
  std::uint32_t local_x_size = 0u;
  std::uint32_t local_y_size = 0u;
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
  );
  const auto descriptor_pool = device->get_descriptor_pool(
    gct::descriptor_pool_create_info_t()
      .set_basic(
        vk::DescriptorPoolCreateInfo()
          .setFlags( vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet )
          .setMaxSets( 1 )
      )
      .set_descriptor_pool_size( vk::DescriptorType::eStorageImage, 2 )
      .rebuild_chain()
  );
  const auto descriptor_set = descriptor_pool->allocate( descriptor_set_layout );
  const auto pipeline_cache = device->get_pipeline_cache();
  const auto allocator = device->get_allocator();


  // 入力用のバッファに画像ファイルの内容を書く
  const auto src_buffer = allocator->load_image(
    CMAKE_CURRENT_SOURCE_DIR "/test.png",
    true
  );
  
  const auto dest_buffer = allocator->create_pixel_buffer(
    vk::BufferUsageFlagBits::eTransferDst,
    VMA_MEMORY_USAGE_GPU_TO_CPU,
    src_buffer->get_extent(),
    src_buffer->get_format()
  );

  auto src_image = allocator->create_image(
    gct::image_create_info_t()
      .set_basic(
        vk::ImageCreateInfo()
          .setImageType( vk::ImageType::e2D )
          .setFormat( vk::Format::eR8G8B8A8Srgb )
          .setExtent( { 1024, 1024, 1 } )
          .setMipLevels( 1 )
          .setArrayLayers( 1 )
          .setSamples( vk::SampleCountFlagBits::e1 )
          .setTiling( vk::ImageTiling::eOptimal )
          .setUsage(
            vk::ImageUsageFlagBits::eTransferDst |
            vk::ImageUsageFlagBits::eStorage
          )
          .setSharingMode( vk::SharingMode::eExclusive )
          .setQueueFamilyIndexCount( 0 )
          .setPQueueFamilyIndices( nullptr )
          .setInitialLayout( vk::ImageLayout::eUndefined )
      ),
      VMA_MEMORY_USAGE_GPU_ONLY
  );

  auto dest_image = allocator->create_image(
    gct::image_create_info_t()
      .set_basic(
        vk::ImageCreateInfo()
          .setImageType( vk::ImageType::e2D )
          .setFormat( vk::Format::eR8G8B8A8Srgb )
          .setExtent( { 1024, 1024, 1 } )
          .setMipLevels( 1 )
          .setArrayLayers( 1 )
          .setSamples( vk::SampleCountFlagBits::e1 )
          .setTiling( vk::ImageTiling::eOptimal )
          .setUsage(
            vk::ImageUsageFlagBits::eTransferSrc |
            vk::ImageUsageFlagBits::eStorage
          )
          .setInitialLayout( vk::ImageLayout::eUndefined )
      ),
      VMA_MEMORY_USAGE_GPU_ONLY
  );

  // 入力用のイメージビューを作る
  auto src_view = 
    src_image->get_view(
      gct::image_view_create_info_t()
        .set_basic(
          vk::ImageViewCreateInfo()
            .setSubresourceRange(
              vk::ImageSubresourceRange()
                .setAspectMask( vk::ImageAspectFlagBits::eColor )
                .setBaseMipLevel( 0 )
                .setLevelCount( 1 )
                .setBaseArrayLayer( 0 )
                .setLayerCount( 1 )
            )
            .setViewType( gct::to_image_view_type( src_image->get_props().get_basic().imageType, src_image->get_props().get_basic().arrayLayers ) )
        )
        .rebuild_chain()
    );
  
  // 出力用のイメージビューを作る
  auto dest_view = 
    dest_image->get_view(
      gct::image_view_create_info_t()
        .set_basic(
          vk::ImageViewCreateInfo()
            .setSubresourceRange(
              vk::ImageSubresourceRange()
                .setAspectMask( vk::ImageAspectFlagBits::eColor )
                .setBaseMipLevel( 0 )
                .setLevelCount( 1 )
                .setBaseArrayLayer( 0 )
                .setLayerCount( 1 )
            )
            .setViewType( gct::to_image_view_type( dest_image->get_props().get_basic().imageType, dest_image->get_props().get_basic().arrayLayers ) )
        )
        .rebuild_chain()
    );

  {
    const auto command_buffer = queue->get_command_pool()->allocate();
    {
      auto rec = command_buffer->begin();
      // 入力バッファの内容を入力イメージにコピーしてレイアウトを汎用的に使える物に変更する
      rec.copy(
        src_buffer,
        src_image,
        vk::ImageLayout::eGeneral
      );
      // 出力イメージのレイアウトを汎用的に使える物に変更する
      rec.convert_image( dest_image, vk::ImageLayout::eGeneral );
    }
    command_buffer->execute(
      gct::submit_info_t()
    );
    command_buffer->wait_for_executed();
  }

  // デスクリプタの内容を更新
  descriptor_set->update(
    {
      gct::write_descriptor_set_t()
        .set_basic(
          // シェーダ上でsrc_imageという名前の変数に
          (*descriptor_set)[ "src_image" ]
        )
        .add_image(
          gct::descriptor_image_info_t()
            .set_basic(
              vk::DescriptorImageInfo()
                .setImageLayout(
                  src_image->get_layout().get_uniform_layout()
                )
            )
	    // このイメージを結びつける
            .set_image_view( src_view )
        ),
      gct::write_descriptor_set_t()
        .set_basic(
          // シェーダ上でdest_imageという名前の変数に
          (*descriptor_set)[ "dest_image" ]
        )
        .add_image(
          gct::descriptor_image_info_t()
            .set_basic(
              vk::DescriptorImageInfo()
                .setImageLayout(
                  dest_image->get_layout().get_uniform_layout()
                )
            )
	    // このイメージを結びつける
            .set_image_view( dest_view )
        )
    }
  );

  const auto pipeline = pipeline_cache->get_pipeline(
    gct::compute_pipeline_create_info_t()
      .set_stage(
        gct::pipeline_shader_stage_create_info_t()
          .set_shader_module( shader )
          .set_specialization_info(
            gct::specialization_info_t< spec_t >()
              .set_data(
                spec_t{ 16, 16 }
              )
              .add_map< std::uint32_t >( 1, offsetof( spec_t, local_x_size ) )
              .add_map< std::uint32_t >( 2, offsetof( spec_t, local_y_size ) )
          )
      )
      .set_layout( pipeline_layout )
  );

  {

    const auto command_buffer = queue->get_command_pool()->allocate();

    {
      auto rec = command_buffer->begin();
    
      rec.bind_descriptor_set(
        vk::PipelineBindPoint::eCompute,
        pipeline_layout,
        descriptor_set
      );
    
      rec.bind_pipeline(
        pipeline
      );
   
      // 16x16のワークグループを64x64個 = 1024x1024要素で実行する
      rec->dispatch( 64, 64, 1 );

      // 出力用のイメージのレイアウトをコピー元に適した物に変更する
      rec.barrier(
        vk::AccessFlagBits::eShaderWrite,
        vk::AccessFlagBits::eTransferRead,
        vk::PipelineStageFlagBits::eComputeShader,
        vk::PipelineStageFlagBits::eTransfer,
        vk::DependencyFlagBits( 0 ),
        {},
        { dest_image }
      );

      // イメージの内容をバッファに変換しながらコピーする
      rec.copy(
        dest_image,
        dest_buffer
      );
    }
    command_buffer->execute(
      gct::submit_info_t()
    );
    command_buffer->wait_for_executed();
  }

  // バッファの内容を画像ファイルに書く
  dest_buffer->dump_image( "out.png" );

}

