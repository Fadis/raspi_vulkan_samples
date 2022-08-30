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


  // 画像の内容をバッファにロード
  const auto src_buffer = allocator->load_image(
    CMAKE_CURRENT_SOURCE_DIR "/test.png",
    true
  );
  
  // 実行結果を受け取る為のバッファを作る
  const auto dest_buffer = allocator->create_pixel_buffer(
    vk::BufferUsageFlagBits::eTransferDst,
    VMA_MEMORY_USAGE_GPU_TO_CPU,
    src_buffer->get_extent(),
    src_buffer->get_format()
  );

  // 入力イメージを作る
  auto src_image = allocator->create_image(
    gct::image_create_info_t()
      .set_basic(
        vk::ImageCreateInfo()
          // 2次元で
          .setImageType( vk::ImageType::e2D )
          // RGBA各8bitで
          .setFormat( vk::Format::eR8G8B8A8Srgb )
          .setExtent( { 1024, 1024, 1 } )
          // ミップマップは無く
          .setMipLevels( 1 )
          // レイヤーは1枚だけの
          .setArrayLayers( 1 )
          // 1テクセルにつきサンプリング点を1つだけ持つ
          .setSamples( vk::SampleCountFlagBits::e1 )
          // GPUが読みやすいように配置された
          .setTiling( vk::ImageTiling::eOptimal )
          // 転送先とストレージイメージに使う
          .setUsage(
            vk::ImageUsageFlagBits::eTransferDst |
            vk::ImageUsageFlagBits::eSampled
          )
          // 同時に複数のキューから操作しない
          .setSharingMode( vk::SharingMode::eExclusive )
          .setQueueFamilyIndexCount( 0 )
          .setPQueueFamilyIndices( nullptr )
          // 初期状態は不定な
          .setInitialLayout( vk::ImageLayout::eUndefined )
      ),
      // GPUだけから読める
      VMA_MEMORY_USAGE_GPU_ONLY
  );

  // 出力イメージを作る
  auto dest_image = allocator->create_image(
    gct::image_create_info_t()
      .set_basic(
        vk::ImageCreateInfo()
          // 2次元で
          .setImageType( vk::ImageType::e2D )
          // RGBA各8bitで
          .setFormat( vk::Format::eR8G8B8A8Srgb )
          // 1024x1024で
          .setExtent( { 1024, 1024, 1 } )
          // ミップマップは無く
          .setMipLevels( 1 )
          // レイヤーは1枚だけの
          .setArrayLayers( 1 )
          // 1テクセルにつきサンプリング点を1つだけ持つ
          .setSamples( vk::SampleCountFlagBits::e1 )
          // GPUが読みやすいように配置された
          .setTiling( vk::ImageTiling::eOptimal )
          // 転送元とストレージイメージに使う
          .setUsage(
            vk::ImageUsageFlagBits::eTransferSrc |
            vk::ImageUsageFlagBits::eStorage
          )
          // 初期状態は不定な
          .setInitialLayout( vk::ImageLayout::eUndefined )
      ),
      // GPUだけから見える
      VMA_MEMORY_USAGE_GPU_ONLY
  );

  // 入力イメージのイメージビューを作る
  auto src_view = 
    src_image->get_view(
      gct::image_view_create_info_t()
        .set_basic(
          vk::ImageViewCreateInfo()
            .setSubresourceRange(
              vk::ImageSubresourceRange()
                // イメージの色のうち
                .setAspectMask( vk::ImageAspectFlagBits::eColor )
                // 最大のミップマップから
                .setBaseMipLevel( 0 )
                // 1枚の範囲
                .setLevelCount( 1 )
                // 最初のレイヤーから
                .setBaseArrayLayer( 0 )
                // 1枚の範囲
                .setLayerCount( 1 )
            )
            .setViewType( gct::to_image_view_type( src_image->get_props().get_basic().imageType ) )
        )
        .rebuild_chain()
    );
  // 出力イメージのイメージビューを作る
  auto dest_view = 
    dest_image->get_view(
      gct::image_view_create_info_t()
        .set_basic(
          vk::ImageViewCreateInfo()
            .setSubresourceRange(
              vk::ImageSubresourceRange()
                // イメージの色のうち
                .setAspectMask( vk::ImageAspectFlagBits::eColor )
                // 最大のミップマップから
                .setBaseMipLevel( 0 )
                // 1枚の範囲
                .setLevelCount( 1 )
                // 最初のレイヤーから
                .setBaseArrayLayer( 0 )
                // 1枚の範囲
                .setLayerCount( 1 )
            )
            .setViewType( gct::to_image_view_type( dest_image->get_props().get_basic().imageType ) )
        )
        .rebuild_chain()
    );


  {
    const auto command_buffer = queue->get_command_pool()->allocate();
    {
      auto rec = command_buffer->begin();
      // 入力イメージの内容を書く
      rec.copy(
        src_buffer,
        src_image,
        vk::ImageLayout::eShaderReadOnlyOptimal
      );
      // イメージのレイアウトを汎用に変更
      rec.convert_image( dest_image, vk::ImageLayout::eGeneral );
    }
    command_buffer->execute(
      gct::submit_info_t()
    );
    command_buffer->wait_for_executed();
  }

  auto sampler =
    device->get_sampler(
      gct::sampler_create_info_t()
        .set_basic(
          vk::SamplerCreateInfo()
            .setMagFilter( vk::Filter::eLinear )
            .setMinFilter( vk::Filter::eLinear )
            .setMipmapMode( vk::SamplerMipmapMode::eLinear )
            .setAddressModeU( vk::SamplerAddressMode::eClampToBorder )
            .setAddressModeV( vk::SamplerAddressMode::eClampToBorder )
            .setAddressModeW( vk::SamplerAddressMode::eClampToBorder )
            .setAnisotropyEnable( false )
            .setCompareEnable( false )
            .setMipLodBias( 0.f )
            .setMinLod( 0.f )
            .setMaxLod( 5.f )
            .setBorderColor( vk::BorderColor::eFloatOpaqueWhite )
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
            .set_sampler( sampler )
        ),
      gct::write_descriptor_set_t()
        .set_basic(
          // このデスクリプタを
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
            // このイメージビューにする
            .set_image_view( dest_view )
        ),
    }
  );

  // パイプラインを作る
  const auto pipeline = pipeline_cache->get_pipeline(
    gct::compute_pipeline_create_info_t()
      .set_stage(
        gct::pipeline_shader_stage_create_info_t()
          .set_shader_module( shader )
          .set_specialization_info(
            gct::specialization_info_t< spec_t >()
              .set_data(
                // ワークグループを16x16にする
                spec_t{ 16, 16 }
              )
              .add_map< std::uint32_t >( 1, offsetof( spec_t, local_x_size ) )
              .add_map< std::uint32_t >( 2, offsetof( spec_t, local_y_size ) )
          )
      )
      .set_layout( pipeline_layout )
  );

  push_constant_t push_constant;
  push_constant.tex_mat = glm::mat4x4(
    std::cos( M_PI / 4 ), std::sin( M_PI / 4 ), 0.f, 0.f,
    -std::sin( M_PI / 4 ), std::cos( M_PI / 4 ), 0.f, 0.f,
    0.f, 0.f, 1.f, 0.f,
    0.f, 0.f, 0.f, 1.f
  );

  {

    const auto command_buffer = queue->get_command_pool()->allocate();

    {
      auto rec = command_buffer->begin();
    
      // このデスクリプタセットを使う
      rec.bind_descriptor_set(
        vk::PipelineBindPoint::eCompute,
        pipeline_layout,
        descriptor_set
      );
    
      // このパイプラインを使う
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
   
      // 16x16のワークグループを64x64個実行 = 1024x1024
      rec->dispatch( 64, 64, 1 );

      // シェーダから出力イメージへの書き込みが完了した後で
      rec.barrier(
        vk::AccessFlagBits::eShaderWrite,
        vk::AccessFlagBits::eTransferRead,
        vk::PipelineStageFlagBits::eComputeShader,
        vk::PipelineStageFlagBits::eTransfer,
        vk::DependencyFlagBits( 0 ),
        {},
        { dest_image }
      );

      // CPUから見えるバッファに出力イメージの内容をコピー
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

  // バッファの内容を画像としてファイルに保存
  dest_buffer->dump_image( "out.png" );

}

