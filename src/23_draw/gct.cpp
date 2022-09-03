#include <cmath>
#include <iostream>
#include <gct/get_extensions.hpp>
#include <gct/instance.hpp>
#include <gct/queue.hpp>
#include <gct/device.hpp>
#include <gct/allocator.hpp>
#include <gct/device_create_info.hpp>
#include <gct/image_create_info.hpp>
#include <gct/descriptor_set_layout.hpp>
#include <gct/pipeline_cache.hpp>
#include <gct/pipeline_layout_create_info.hpp>
#include <gct/submit_info.hpp>
#include <gct/shader_module_create_info.hpp>
#include <gct/shader_module.hpp>
#include <gct/graphics_pipeline_create_info.hpp>
#include <gct/graphics_pipeline.hpp>
#include <gct/pipeline_layout.hpp>
#include <gct/render_pass_begin_info.hpp>

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
  auto selected = groups[ 0 ].with_extensions( {
    VK_EXT_PIPELINE_CREATION_FEEDBACK_EXTENSION_NAME
  } );
 
  const auto device = selected.create_device(
    std::vector< gct::queue_requirement_t >{
      gct::queue_requirement_t{
        vk::QueueFlagBits::eGraphics,
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

  const auto vs = device->get_shader_module(
    CMAKE_CURRENT_BINARY_DIR "/shader.vert.spv"
  );

  const auto fs = device->get_shader_module(
    CMAKE_CURRENT_BINARY_DIR "/shader.frag.spv"
  );
  
  const auto descriptor_set_layout = device->get_descriptor_set_layout(
    gct::descriptor_set_layout_create_info_t()
      .add_binding(
        vs->get_props().get_reflection()
      )
      .add_binding(
        fs->get_props().get_reflection()
      )
  );
  
  const auto descriptor_set = descriptor_pool->allocate( descriptor_set_layout );

  const auto pipeline_layout = device->get_pipeline_layout(
    gct::pipeline_layout_create_info_t()
      .add_descriptor_set_layout( descriptor_set_layout )
  );

  const auto render_pass = device->get_render_pass(
    gct::render_pass_create_info_t()
      .add_attachment(
        vk::AttachmentDescription()
          .setFormat( vk::Format::eR8G8B8A8Unorm )
          .setSamples( vk::SampleCountFlagBits::e1 )
          .setLoadOp( vk::AttachmentLoadOp::eClear )
          .setStoreOp( vk::AttachmentStoreOp::eStore )
          .setStencilLoadOp( vk::AttachmentLoadOp::eDontCare )
          .setStencilStoreOp( vk::AttachmentStoreOp::eDontCare )
          .setInitialLayout( vk::ImageLayout::eUndefined )
          .setFinalLayout( vk::ImageLayout::eColorAttachmentOptimal )
      )
      .add_attachment(
        vk::AttachmentDescription()
          .setFormat( vk::Format::eD16Unorm )
          .setSamples( vk::SampleCountFlagBits::e1 )
          .setLoadOp( vk::AttachmentLoadOp::eClear )
          .setStoreOp( vk::AttachmentStoreOp::eStore )
          .setStencilLoadOp( vk::AttachmentLoadOp::eDontCare )
          .setStencilStoreOp( vk::AttachmentStoreOp::eDontCare )
          .setInitialLayout( vk::ImageLayout::eUndefined )
          .setFinalLayout( vk::ImageLayout::eDepthStencilAttachmentOptimal )
      )
      .add_subpass(
        gct::subpass_description_t()
          .add_color_attachment( 0, vk::ImageLayout::eColorAttachmentOptimal )
          .set_depth_stencil_attachment( 1, vk::ImageLayout::eDepthStencilAttachmentOptimal )
          .rebuild_chain()
      )
    );

  const auto pipeline_cache = device->get_pipeline_cache();

  const auto stencil_op = vk::StencilOpState()
    .setCompareOp( vk::CompareOp::eAlways )
    .setFailOp( vk::StencilOp::eKeep )
    .setPassOp( vk::StencilOp::eKeep );

  auto vistat = gct::pipeline_vertex_input_state_create_info_t()
    .add_vertex_input_binding_description(
      vk::VertexInputBindingDescription()
        .setBinding( 0 )
        .setInputRate( vk::VertexInputRate::eVertex )
        .setStride( sizeof( float ) * 3 )
    )
    .add_vertex_input_attribute_description(
      vk::VertexInputAttributeDescription()
        .setLocation( 0 )
        .setFormat( vk::Format::eR32G32B32Sfloat )
        .setBinding( 0 )
        .setOffset( 0 )
    );

  const auto input_assembly =
    gct::pipeline_input_assembly_state_create_info_t()
      .set_basic(
        vk::PipelineInputAssemblyStateCreateInfo()
          .setTopology( vk::PrimitiveTopology::eTriangleList )
      );

  const std::uint32_t width = 1024u;
  const std::uint32_t height = 1024u;

  const auto viewport =
    gct::pipeline_viewport_state_create_info_t()
      .add_viewport(
        vk::Viewport()
          .setWidth( width )
          .setHeight( height )
          .setMinDepth( 0.0f )
          .setMaxDepth( 1.0f )
      )
      .add_scissor(
        vk::Rect2D()
          .setOffset( { 0, 0 } )
          .setExtent( { width, height } )
      )
      .rebuild_chain();

  const auto rasterization =
    gct::pipeline_rasterization_state_create_info_t()
      .set_basic(
        vk::PipelineRasterizationStateCreateInfo()
          .setDepthClampEnable( false )
          .setRasterizerDiscardEnable( false )
          .setPolygonMode( vk::PolygonMode::eFill )
          .setCullMode( vk::CullModeFlagBits::eNone )
          .setFrontFace( vk::FrontFace::eClockwise )
          .setDepthBiasEnable( false )
          .setLineWidth( 1.0f )
      );

  const auto multisample =
    gct::pipeline_multisample_state_create_info_t()
      .set_basic(
        vk::PipelineMultisampleStateCreateInfo()
      );

  const auto depth_stencil =
    gct::pipeline_depth_stencil_state_create_info_t()
      .set_basic(
        vk::PipelineDepthStencilStateCreateInfo()
          .setDepthTestEnable( true )
          .setDepthWriteEnable( true )
          .setDepthCompareOp( vk::CompareOp::eLessOrEqual )
          .setDepthBoundsTestEnable( false )
          .setStencilTestEnable( false )
          .setFront( stencil_op )
          .setBack( stencil_op )
      );

  const auto color_blend =
    gct::pipeline_color_blend_state_create_info_t()
      .add_attachment(
        vk::PipelineColorBlendAttachmentState()
          .setBlendEnable( false )
          .setColorWriteMask(
            vk::ColorComponentFlagBits::eR |
            vk::ColorComponentFlagBits::eG |
            vk::ColorComponentFlagBits::eB |
            vk::ColorComponentFlagBits::eA
          )
      );

  const auto dynamic =
    gct::pipeline_dynamic_state_create_info_t();

  auto pipeline = pipeline_cache->get_pipeline(
    gct::graphics_pipeline_create_info_t()
      .add_stage( vs )
      .add_stage( fs )
      .set_vertex_input( vistat )
      .set_input_assembly( input_assembly )
      .set_viewport( viewport )
      .set_rasterization( rasterization )
      .set_multisample( multisample )
      .set_depth_stencil( depth_stencil )
      .set_color_blend( color_blend )
      .set_dynamic( dynamic )
      .set_layout( pipeline_layout )
      .set_render_pass( render_pass, 0 )
  );
  
  const auto allocator = device->get_allocator();

  auto dest_image = allocator->create_image(
    gct::image_create_info_t()
      .set_basic(
        vk::ImageCreateInfo()
          .setImageType( vk::ImageType::e2D )
          .setFormat( vk::Format::eR8G8B8A8Unorm )
          .setExtent( { 1024, 1024, 1 } )
          .setUsage(
            vk::ImageUsageFlagBits::eTransferSrc |
            vk::ImageUsageFlagBits::eColorAttachment
          )
          .setMipLevels( 1 )
          .setArrayLayers( 1 )
          .setSamples( vk::SampleCountFlagBits::e1 )
          .setTiling( vk::ImageTiling::eOptimal )
          .setInitialLayout( vk::ImageLayout::eUndefined )
      ),
    VMA_MEMORY_USAGE_GPU_ONLY
  );

  const auto dest_buffer = allocator->create_pixel_buffer(
    vk::BufferUsageFlagBits::eTransferDst,
    VMA_MEMORY_USAGE_GPU_TO_CPU,
    dest_image->get_props().get_basic().extent,
    vk::Format::eR8G8B8A8Unorm
  );

  auto depth = allocator->create_image(
    gct::image_create_info_t()
      .set_basic(
        vk::ImageCreateInfo()
          .setImageType( vk::ImageType::e2D )
          .setFormat( vk::Format::eD16Unorm )
          .setExtent( dest_image->get_props().get_basic().extent )
          .setUsage( vk::ImageUsageFlagBits::eDepthStencilAttachment )
          .setMipLevels( 1 )
          .setArrayLayers( 1 )
          .setSamples( vk::SampleCountFlagBits::e1 )
          .setTiling( vk::ImageTiling::eOptimal )
          .setInitialLayout( vk::ImageLayout::eUndefined )
      ),
    VMA_MEMORY_USAGE_GPU_ONLY
  );

  auto depth_view = depth->get_view( vk::ImageAspectFlagBits::eDepth );
  auto color_view = dest_image->get_view( vk::ImageAspectFlagBits::eColor );

  auto framebuffer = render_pass->get_framebuffer(
    gct::framebuffer_create_info_t()
      .add_attachment( color_view )
      .add_attachment( depth_view )
  );

  const auto command_buffer = queue->get_command_pool()->allocate();
  std::shared_ptr< gct::buffer_t > vertex_buffer;
  {
    auto rec = command_buffer->begin();
    rec.convert_image(
      dest_image,
      vk::ImageLayout::eColorAttachmentOptimal
    );

    // 三角形の頂点の座標
    const std::vector< float > vertex{
      0.f, 0.f, 0.f,
      1.f, 0.f, 0.f,
      0.f, 1.f, 0.f
    };
    // 三角形の頂点の座標をバッファに乗せる
    vertex_buffer = rec.load_buffer(
      allocator,
      vertex.data(),
      sizeof( float ) * vertex.size(),
      // 用途は頂点配列
      vk::BufferUsageFlagBits::eVertexBuffer
    );

    rec.barrier(
      vk::AccessFlagBits::eTransferWrite,
      vk::AccessFlagBits::eVertexAttributeRead,
      vk::PipelineStageFlagBits::eTransfer,
      vk::PipelineStageFlagBits::eVertexInput,
      vk::DependencyFlagBits( 0 ),
      { vertex_buffer },
      {}
    );

    {
      // レンダーパスを開始する
      auto render_pass_token = rec.begin_render_pass(
        gct::render_pass_begin_info_t()
          .set_basic(
            vk::RenderPassBeginInfo()
              .setRenderPass( **render_pass )
              .setFramebuffer( **framebuffer )
              .setRenderArea( vk::Rect2D( vk::Offset2D(0, 0), vk::Extent2D((uint32_t)width, (uint32_t)height) ) )
          )
          // loadOpがVK_ATTACHMENT_LOAD_OP_CLEARのアタッチメントは
          // レンダーパス開始時にこの色で塗り潰す
          // 色は真っ白で
          .add_clear_value( vk::ClearColorValue( std::array< float, 4u >{ 1.0f, 1.0f, 1.0f, 1.0f } ) )
          // 深度は最も遠く
          .add_clear_value( vk::ClearDepthStencilValue( 1.f, 0 ) )
          .rebuild_chain(),
        vk::SubpassContents::eInline
      );
     
      rec->bindPipeline(
        vk::PipelineBindPoint::eGraphics,
        **pipeline
      );
     
      rec->bindDescriptorSets(
        vk::PipelineBindPoint::eGraphics,
        **pipeline_layout,
        0,
        **descriptor_set,
        {}
      );
     
      // この頂点バッファを使う
      // binding 0番がvertex_bufferの内容になる
      rec->bindVertexBuffers( 0, { **vertex_buffer }, { 0 } );
     
      // パイプラインを実行する
      rec->draw( 3, 1, 0, 0 );
    }
    
    rec.barrier(
      vk::AccessFlagBits::eColorAttachmentWrite,
      vk::AccessFlagBits::eTransferRead,
      vk::PipelineStageFlagBits::eColorAttachmentOutput,
      vk::PipelineStageFlagBits::eTransfer,
      vk::DependencyFlagBits( 0 ),
      {},
      { dest_image }
    );

    rec.convert_image(
      dest_image,
      vk::ImageLayout::eTransferSrcOptimal
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

  
  dest_buffer->dump_image( "out.png" );

}

