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
#include <gct/pipeline_layout.hpp>
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
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/version.h>

struct spec_t {
  std::uint32_t local_x_size = 0u;
  std::uint32_t local_y_size = 0u;
  float value = 0.f;
};

int main() {
  uint32_t iext_count = 0u;
  std::vector< const char* > iext{};
  const std::shared_ptr< gct::instance_t > gct_instance(
    new gct::instance_t(
      gct::instance_create_info_t()
        .set_application_info(
          vk::ApplicationInfo()
            .setPApplicationName( "my_application" )
            .setApplicationVersion(  VK_MAKE_VERSION( 1, 0, 0 ) )
            .setApiVersion( VK_MAKE_VERSION( 1, 2, 0 ) )
        )
        .add_layer(
          "VK_LAYER_KHRONOS_validation"
        )
        .add_extension(
          iext.begin(), iext.end()
        )
    )
  );
  auto groups = gct_instance->get_physical_devices( {} );
  auto gct_physical_device = groups[ 0 ].with_extensions( {
    VK_EXT_PIPELINE_CREATION_FEEDBACK_EXTENSION_NAME
  } );
 
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

  const auto gct_descriptor_pool = gct_device->get_descriptor_pool(
    gct::descriptor_pool_create_info_t()
      .set_basic(
        vk::DescriptorPoolCreateInfo()
          .setFlags( vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet )
          .setMaxSets( 1 )
      )
      .set_descriptor_pool_size( vk::DescriptorType::eStorageImage, 2 )
      .rebuild_chain()
  );
  
  const auto gct_shader = gct_device->get_shader_module(
    CMAKE_CURRENT_BINARY_DIR "/shader.comp.spv"
  );
  
  const auto gct_descriptor_set_layout = gct_device->get_descriptor_set_layout(
    gct::descriptor_set_layout_create_info_t()
      .add_binding(
        gct_shader->get_props().get_reflection()
      )
      .rebuild_chain()
  );

  const auto gct_descriptor_set = gct_descriptor_pool->allocate( gct_descriptor_set_layout );

  const auto gct_pipeline_layout = gct_device->get_pipeline_layout(
    gct::pipeline_layout_create_info_t()
      .add_descriptor_set_layout( gct_descriptor_set_layout )
  );
  const auto gct_allocator = gct_device->get_allocator();
  const auto gct_pipeline_cache = gct_device->get_pipeline_cache();
  const auto gct_command_pool = gct_queue->get_command_pool();
  
  const auto instance = **gct_instance;
  const auto physical_device = **gct_physical_device.devices[ 0 ];
  const auto device = **gct_device;
  const auto queue = **gct_queue;
  const auto queue_family_index = gct_queue->get_available_queue_family_index();
  const auto descriptor_pool = **gct_descriptor_pool;
  const auto shader = **gct_shader;
  const auto descriptor_set_layout = **gct_descriptor_set_layout;
  const auto descriptor_set = **gct_descriptor_set;
  const auto pipeline_cache = **gct_pipeline_cache;
  const auto allocator = **gct_allocator;
  const auto pipeline_layout = **gct_pipeline_layout;
  const auto command_pool = **gct_command_pool;

  const auto gct_pipeline = gct_pipeline_cache->get_pipeline(
    gct::compute_pipeline_create_info_t()
      .set_stage(
        gct::pipeline_shader_stage_create_info_t()
          .set_shader_module( gct_shader )
          .set_specialization_info(
            gct::specialization_info_t< spec_t >()
              .set_data(
                spec_t{ 16, 16, 1.0f }
              )
              .add_map< std::uint32_t >( 1, offsetof( spec_t, local_x_size ) )
              .add_map< std::uint32_t >( 2, offsetof( spec_t, local_y_size ) )
          )
      )
      .set_layout( gct_pipeline_layout )
  );
  const auto pipeline = **gct_pipeline;

  const auto gct_src_buffer = gct_allocator->create_buffer(
    gct::buffer_create_info_t()
      .set_basic(
        vk::BufferCreateInfo()
          .setSize( 1024u * 1024u * 4u )
          .setUsage( vk::BufferUsageFlagBits::eTransferSrc )
      ),
    VMA_MEMORY_USAGE_CPU_TO_GPU
  );
  const auto src_buffer = **gct_src_buffer;

  auto gct_src_image = gct_allocator->create_image(
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
      ),
      VMA_MEMORY_USAGE_GPU_ONLY
  );
  const auto src_image = **gct_src_image;

  const auto gct_dest_buffer = gct_allocator->create_buffer(
    gct::buffer_create_info_t()
      .set_basic(
        vk::BufferCreateInfo()
          .setSize( 1024u * 1024u * 4u )
          .setUsage( vk::BufferUsageFlagBits::eTransferDst )
      ),
    VMA_MEMORY_USAGE_GPU_TO_CPU
  );
  const auto dest_buffer = **gct_dest_buffer;

  auto gct_dest_image = gct_allocator->create_image(
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
      ),
      VMA_MEMORY_USAGE_GPU_ONLY
  );
  const auto dest_image = **gct_dest_image;

  // 入力用のバッファに画像ファイルの内容を書く
  {
    using namespace OIIO_NAMESPACE;
#if OIIO_VERSION_MAJOR >= 2 
    auto texture_file = ImageInput::open( CMAKE_CURRENT_SOURCE_DIR "/test.png" );
#else
    std::shared_ptr< ImageInput > texture_file(
      ImageInput::open( filename ),
      []( auto p ) { if( p ) ImageInput::destroy( p ); }
    );
#endif
    if( !texture_file ) throw -1;
    const ImageSpec &spec = texture_file->spec();
    {
      auto mapped = gct_src_buffer->map< std::uint8_t >();
      if( spec.nchannels == 3 ) {
        std::vector< uint8_t > temp( spec.width * spec.height * 4u );
        texture_file->read_image( TypeDesc::UINT8, temp.data() );
        for( size_t i = spec.width * spec.height - 1; i; --i ) {
          temp[ i * 4 ] = temp[ i * spec.nchannels ];
          temp[ i * 4 + 1 ] = temp[ i * spec.nchannels + 1 ];
          temp[ i * 4 + 2 ] = temp[ i * spec.nchannels + 2 ];
          temp[ i * 4 + 3 ] = 255u;
        }
        std::copy( temp.begin(), temp.end(), mapped.begin() );
      }
      else {
        texture_file->read_image( TypeDesc::UINT8, mapped.begin() );
      }
    }
  }

  {
    auto command_buffers = device.allocateCommandBuffersUnique(
      vk::CommandBufferAllocateInfo()
        .setCommandPool( command_pool )
        .setLevel( vk::CommandBufferLevel::ePrimary )
        .setCommandBufferCount( 1u )
    );
    const auto command_buffer = std::move( command_buffers[ 0 ] );
    const auto fence = device.createFenceUnique(
      vk::FenceCreateInfo()
    );
    
    command_buffer->begin(
      vk::CommandBufferBeginInfo()
    );

    // イメージのレイアウトをコピー先に適したものに変更
    {
      command_buffer->pipelineBarrier(
        vk::PipelineStageFlagBits::eTransfer,
        vk::PipelineStageFlagBits::eTransfer,
        vk::DependencyFlagBits( 0 ),
        {},
        {},
	{
          vk::ImageMemoryBarrier()
            .setSrcAccessMask( vk::AccessFlagBits::eTransferRead )
            .setDstAccessMask( vk::AccessFlagBits::eTransferWrite )
            .setOldLayout( vk::ImageLayout::eUndefined )
            .setNewLayout( vk::ImageLayout::eTransferDstOptimal )
            .setImage( src_image )
            .setSubresourceRange(
              vk::ImageSubresourceRange()
	        .setAspectMask( vk::ImageAspectFlagBits::eColor )
                .setLevelCount( 1u )
                .setLayerCount( 1u )
            )
	}
      );
    }
      
    // バッファの内容をイメージに変換しながらコピー
    command_buffer->copyBufferToImage(
      // このバッファから
      src_buffer,
      // このイメージへ
      src_image,
      vk::ImageLayout::eTransferDstOptimal,
      std::vector< vk::BufferImageCopy >{
        vk::BufferImageCopy()
          // バッファの内容をこのサイズの画像と見做す
          .setBufferRowLength( 1024u )
          .setBufferImageHeight( 1024u )
          // ミップレベル0 レイヤー0に書く
          .setImageSubresource(
            vk::ImageSubresourceLayers()
              .setAspectMask( vk::ImageAspectFlagBits::eColor )
              .setLayerCount( 1 )
          )
          // イメージのこの範囲に書く
          .setImageExtent(
            vk::Extent3D()
	      .setWidth( 1024u )
	      .setHeight( 1024u )
	      .setDepth( 1u )
          )
      }
    );

    // イメージのレイアウトを汎用的に使える物に変更する 
    command_buffer->pipelineBarrier(
      vk::PipelineStageFlagBits::eTransfer,
      vk::PipelineStageFlagBits::eComputeShader,
      vk::DependencyFlagBits( 0 ),
      {},
      {},
      {
        vk::ImageMemoryBarrier()
          // このレイアウトから
          .setOldLayout( vk::ImageLayout::eTransferDstOptimal )
          // このレイアウトに
          .setNewLayout( vk::ImageLayout::eGeneral )
          .setSrcAccessMask( vk::AccessFlagBits::eTransferWrite )
          .setDstAccessMask( vk::AccessFlagBits::eShaderRead )
          // 入力用のイメージを
          .setImage( src_image )
           // ミップレベル0 レイヤー0のレイアウトを変更する
          .setSubresourceRange(
            vk::ImageSubresourceRange()
              .setAspectMask( vk::ImageAspectFlagBits::eColor )
              .setLevelCount( 1 )
              .setLayerCount( 1 )
          ),
        // 出力用のイメージも汎用的に使えるレイアウトにする
        vk::ImageMemoryBarrier()
          .setOldLayout( vk::ImageLayout::eUndefined )
          .setNewLayout( vk::ImageLayout::eGeneral )
          .setSrcAccessMask( vk::AccessFlagBits::eTransferWrite )
          .setDstAccessMask( vk::AccessFlagBits::eShaderRead )
          .setImage( dest_image )
          .setSubresourceRange(
            vk::ImageSubresourceRange()
              .setAspectMask( vk::ImageAspectFlagBits::eColor )
              .setLevelCount( 1 )
              .setLayerCount( 1 )
          )
      }
    );
    
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
  }

  // 入力用のイメージビューを作る
  auto src_view =
    device.createImageViewUnique(
      vk::ImageViewCreateInfo()
        // 入力用のイメージの
        .setImage( src_image )
        // ミップレベル0 レイヤー0の部分が見えるビューを作る
        .setSubresourceRange(
          vk::ImageSubresourceRange()
            .setAspectMask( vk::ImageAspectFlagBits::eColor )
            .setLevelCount( 1 )
            .setLayerCount( 1 )
        )
        .setFormat( vk::Format::eR8G8B8A8Srgb )
        .setViewType( vk::ImageViewType::e2D )
        .setComponents(
          vk::ComponentMapping()
            .setR( vk::ComponentSwizzle::eR )
            .setG( vk::ComponentSwizzle::eG )
            .setB( vk::ComponentSwizzle::eB )
            .setA( vk::ComponentSwizzle::eA )
        )
    );
  
  // 出力用のイメージビューを作る
  auto dest_view =
    device.createImageViewUnique(
      vk::ImageViewCreateInfo()
        // 出力用のイメージの
        .setImage( dest_image )
        // ミップレベル0 レイヤー0の部分が見えるビューを作る
        .setSubresourceRange(
          vk::ImageSubresourceRange()
            .setAspectMask( vk::ImageAspectFlagBits::eColor )
            .setLevelCount( 1 )
            .setLayerCount( 1 )
        )
        .setFormat( vk::Format::eR8G8B8A8Srgb )
        .setViewType( vk::ImageViewType::e2D )
        .setComponents(
          vk::ComponentMapping()
            .setR( vk::ComponentSwizzle::eR )
            .setG( vk::ComponentSwizzle::eG )
            .setB( vk::ComponentSwizzle::eB )
            .setA( vk::ComponentSwizzle::eA )
        )
    );

  const auto src_descriptor_image_info =
    vk::DescriptorImageInfo()
      .setImageView( *src_view )
      .setImageLayout( vk::ImageLayout::eGeneral );
  const auto dest_descriptor_image_info =
    vk::DescriptorImageInfo()
      .setImageView( *dest_view )
      .setImageLayout( vk::ImageLayout::eGeneral );

  // デスクリプタの内容を更新
  device.updateDescriptorSets(
    {
      vk::WriteDescriptorSet()
        // このデスクリプタセットの
        .setDstSet( descriptor_set )
        // binding=0の
        .setDstBinding( 0 )
        // 1個の
        .setDescriptorCount( 1u )
        // ストレージバッファのデスクリプタを
        .setDescriptorType( vk::DescriptorType::eStorageImage )
        // この内容にする
        .setPImageInfo( &src_descriptor_image_info ),
      vk::WriteDescriptorSet()
        // このデスクリプタセットの
        .setDstSet( descriptor_set )
        // binding=1の
        .setDstBinding( 1 )
        // 1個の
        .setDescriptorCount( 1u )
        // ストレージバッファのデスクリプタを
        .setDescriptorType( vk::DescriptorType::eStorageImage )
        // この内容にする
        .setPImageInfo( &dest_descriptor_image_info )
    },
    {}
  );

  {
    auto command_buffers = device.allocateCommandBuffersUnique(
      vk::CommandBufferAllocateInfo()
        .setCommandPool( command_pool )
        .setLevel( vk::CommandBufferLevel::ePrimary )
        .setCommandBufferCount( 1u )
    );
    const auto command_buffer = std::move( command_buffers[ 0 ] );
    const auto fence = device.createFenceUnique(
      vk::FenceCreateInfo()
    );
    
    command_buffer->begin(
      vk::CommandBufferBeginInfo()
    );

    command_buffer->bindDescriptorSets(
      vk::PipelineBindPoint::eCompute,
      pipeline_layout,
      0u,
      { descriptor_set },
      {}
    );
 
    command_buffer->bindPipeline(
      vk::PipelineBindPoint::eCompute,
      pipeline
    );
 
    // 16x16のワークグループを64x64個 = 1024x1024要素で実行する
    command_buffer->dispatch( 64, 64, 1 );


    // 出力用のイメージのレイアウトをコピー元に適した物に変更する
    {
      command_buffer->pipelineBarrier(
        vk::PipelineStageFlagBits::eComputeShader,
        vk::PipelineStageFlagBits::eTransfer,
        vk::DependencyFlagBits( 0 ),
        {},
        {},
	{
          vk::ImageMemoryBarrier()
            .setSrcAccessMask( vk::AccessFlagBits::eShaderWrite )
            .setDstAccessMask( vk::AccessFlagBits::eTransferRead )
            .setOldLayout( vk::ImageLayout::eGeneral )
            .setNewLayout( vk::ImageLayout::eTransferSrcOptimal )
            // 出力用のイメージの
            .setImage( dest_image )
            // ミップレベル0 レイヤー0のレイアウトを変更する
            .setSubresourceRange(
              vk::ImageSubresourceRange()
	        .setAspectMask( vk::ImageAspectFlagBits::eColor )
                .setLevelCount( 1u )
                .setLayerCount( 1u )
            )
	}
      );
    }

    // イメージの内容をバッファに変換しながらコピーする
    command_buffer->copyImageToBuffer(
      dest_image,
      vk::ImageLayout::eTransferSrcOptimal,
      dest_buffer,
      std::vector< vk::BufferImageCopy >{
        vk::BufferImageCopy()
          // バッファの内容をこのサイズの画像と見做す
          .setBufferRowLength( 1024u )
          .setBufferImageHeight( 1024u )
          // ミップレベル0 レイヤー0を読む
          .setImageSubresource(
            vk::ImageSubresourceLayers()
              .setAspectMask( vk::ImageAspectFlagBits::eColor )
              .setLayerCount( 1 )
          )
          // イメージのこの範囲を
          .setImageExtent(
            vk::Extent3D()
	      .setWidth( 1024u )
	      .setHeight( 1024u )
	      .setDepth( 1u )
          )
      }
    );

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
  }

  // バッファの内容を画像ファイルに書く
  {
    using namespace OIIO_NAMESPACE;
    ImageSpec spec( 1024, 1024, 4, TypeDesc::UINT8 );
    auto out = ImageOutput::create( "out.png" );
    out->open( "out.png", spec );
    {
      auto mapped = gct_dest_buffer->map< std::uint8_t >();
      out->write_image( TypeDesc::UINT8, mapped.begin() );
    }
    out->close();
  }

}

