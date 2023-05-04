#include <iostream>
#include <nlohmann/json.hpp>
#include <gct/get_extensions.hpp>
#include <glm/mat4x4.hpp>
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
#include <gct/pipeline_layout.hpp>
#include <gct/pipeline_layout_create_info.hpp>
#include <gct/buffer_view_create_info.hpp>
#include <gct/submit_info.hpp>
#include <gct/shader_module_create_info.hpp>
#include <gct/shader_module.hpp>
#include <gct/sampler_create_info.hpp>
#include <gct/compute_pipeline_create_info.hpp>
#include <gct/compute_pipeline.hpp>
#include <gct/write_descriptor_set.hpp>

struct spec_t {
  std::uint32_t local_x_size = 0u;
  std::uint32_t local_y_size = 0u;
};

struct push_constant_t {
  glm::mat4x4 tex_mat;
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
          .setMaxSets( 1 )
      )
      .set_descriptor_pool_size( vk::DescriptorType::eStorageImage, 1 )
      .set_descriptor_pool_size( vk::DescriptorType::eCombinedImageSampler, 1 )
      .rebuild_chain()
  );
  const auto descriptor_set = descriptor_pool->allocate( descriptor_set_layout );
  const auto pipeline_cache = device->get_pipeline_cache();
  const auto allocator = device->get_allocator();


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
          .setFormat( vk::Format::eR8G8B8A8Unorm )
          .setExtent( { 1024, 1024, 1 } )
          .setMipLevels( 1 )
          .setArrayLayers( 1 )
          .setSamples( vk::SampleCountFlagBits::e1 )
          .setTiling( vk::ImageTiling::eOptimal )
          .setUsage(
            vk::ImageUsageFlagBits::eTransferDst |
            vk::ImageUsageFlagBits::eSampled
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
          .setFormat( vk::Format::eR8G8B8A8Unorm )
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
      rec.copy(
        src_buffer,
        src_image,
        vk::ImageLayout::eShaderReadOnlyOptimal
      );
      rec.convert_image( dest_image, vk::ImageLayout::eGeneral );
    }
    command_buffer->execute(
      gct::submit_info_t()
    );
    command_buffer->wait_for_executed();
  }

  // サンプラーを作る
  auto sampler =
    device->get_sampler(
      gct::sampler_create_info_t()
        .set_basic(
          vk::SamplerCreateInfo()
            // 拡大するときは線形補間
            .setMagFilter( vk::Filter::eLinear )
            // 縮小する時も線形補間
            .setMinFilter( vk::Filter::eLinear )
            // ミップマップレベル間での合成も線形補間
            .setMipmapMode( vk::SamplerMipmapMode::eLinear )
            // 横方向に範囲外を読んだら境界色を返す
            .setAddressModeU( vk::SamplerAddressMode::eClampToBorder )
            // 縦方向に範囲外を読んだら境界色を返す
            .setAddressModeV( vk::SamplerAddressMode::eClampToBorder )
            // 奥行き方向に範囲外を読んだら境界色を返す
            .setAddressModeW( vk::SamplerAddressMode::eClampToBorder )
            // 異方性フィルタリングを使わない
            .setAnisotropyEnable( false )
            // 比較演算を使わない
            .setCompareEnable( false )
            // ミップマップの選択にバイアスをかけない
            .setMipLodBias( 0.f )
            // 最小のミップマップレベル(最も大きい画像)は0枚目
            .setMinLod( 0.f )
            // 最大のミップマップレベル(最も小さい画像)は5枚目
            .setMaxLod( 5.f )
            // 境界色は不透明の白
            .setBorderColor( vk::BorderColor::eFloatOpaqueWhite )
            // 画像の端が(1.0,1.0)になるようなテクスチャ座標を使う
            .setUnnormalizedCoordinates( false )
        )
    );

  // デスクリプタセットを更新
  descriptor_set->update(
    {
      gct::write_descriptor_set_t()
        .set_basic(
          // このデスクリプタを
          (*descriptor_set)[ "src_texture" ]
        )
        .add_image(
          gct::descriptor_image_info_t()
            .set_basic(
              vk::DescriptorImageInfo()
                .setImageLayout(
                  src_image->get_layout().get_uniform_layout()
                )
            )
            // このイメージビューにする
            .set_image_view( src_view )
	    // このサンプラーにする
            .set_sampler( sampler )
        ),
      gct::write_descriptor_set_t()
        .set_basic(
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
            .set_image_view( dest_view )
        ),
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

  push_constant_t push_constant;
  push_constant.tex_mat =
  glm::mat4x4(
    1.f, 0.f, 0.f,  0.f,
    0.f, 1.f, 0.f,  0.f,
    0.f, 0.f, 1.f,  0.f,
    0.5f, 0.5f, 0.f,  1.f
  ) *
  glm::mat4x4(
    std::cos( M_PI / 4 ), std::sin( M_PI / 4 ), 0.f, 0.f,
    -std::sin( M_PI / 4 ), std::cos( M_PI / 4 ), 0.f, 0.f,
    0.f, 0.f, 1.f, 0.f,
    0.f, 0.f, 0.f, 1.f
  ) *
  glm::mat4x4(
    1.f, 0.f, 0.f,  0.f,
    0.f, 1.f, 0.f,  0.f,
    0.f, 0.f, 1.f,  0.f,
    -0.5f, -0.5f, 0.f,  1.f
  ) *
  glm::mat4x4(
    2.f, 0.f, 0.f,  0.f,
    0.f, 2.f, 0.f,  0.f,
    0.f, 0.f, 2.f,  0.f,
    0.f, 0.f, 0.f,  1.f
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
 
      rec->pushConstants(
        **pipeline_layout,
        vk::ShaderStageFlagBits::eCompute,
        0u,
        sizeof( push_constant_t ),
        reinterpret_cast< void* >( &push_constant )
      );
   
      rec->dispatch( 64, 64, 1 );

      rec.barrier(
        vk::AccessFlagBits::eShaderWrite,
        vk::AccessFlagBits::eTransferRead,
        vk::PipelineStageFlagBits::eComputeShader,
        vk::PipelineStageFlagBits::eTransfer,
        vk::DependencyFlagBits( 0 ),
        {},
        { dest_image }
      );

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

  dest_buffer->dump_image( "out.png" );

}

