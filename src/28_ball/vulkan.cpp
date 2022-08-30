#include <memory>
#include <gct/get_extensions.hpp>
#include <gct/instance.hpp>
#include <gct/glfw.hpp>
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
#include <gct/pipeline_viewport_state_create_info.hpp>
#include <gct/pipeline_dynamic_state_create_info.hpp>
#include <gct/pipeline_multisample_state_create_info.hpp>
#include <gct/pipeline_tessellation_state_create_info.hpp>
#include <gct/pipeline_rasterization_state_create_info.hpp>
#include <gct/pipeline_depth_stencil_state_create_info.hpp>
#include <gct/pipeline_color_blend_state_create_info.hpp>
#include <gct/graphics_pipeline_create_info.hpp>
#include <gct/graphics_pipeline.hpp>
#include <gct/pipeline_layout.hpp>
#include <gct/submit_info.hpp>
#include <gct/wait_for_sync.hpp>
#include <gct/present_info.hpp>
#include <gct/render_pass_begin_info.hpp>
#include <gct/primitive.hpp>
#include <gct/semaphore.hpp>

struct fb_resources_t {
  std::shared_ptr< gct::image_t > color;
  std::shared_ptr< gct::framebuffer_t > framebuffer;
  std::shared_ptr< gct::semaphore_t > image_acquired;
  std::shared_ptr< gct::semaphore_t > draw_complete;
  std::shared_ptr< gct::bound_command_buffer_t > command_buffer;
  gct::render_pass_begin_info_t render_pass_begin_info;
};


int main( int argc, const char *argv[] ) {

  gct::glfw::get();
  std::uint32_t required_extension_count = 0u;
  const char **required_extensions_begin = glfwGetRequiredInstanceExtensions( &required_extension_count );
  const auto required_extensions_end = std::next( required_extensions_begin, required_extension_count );
  
  const std::shared_ptr< gct::instance_t > instance(
    new gct::instance_t(
      gct::instance_create_info_t()
        .set_application_info(
          vk::ApplicationInfo()
            .setPApplicationName( argc ? argv[ 0 ] : "my_application" )
            .setApplicationVersion(  VK_MAKE_VERSION( 1, 0, 0 ) )
            .setApiVersion( VK_API_VERSION_1_2 )
        )
	.add_extension(
          required_extensions_begin,
	  required_extensions_end
	)
        .add_layer(
          "VK_LAYER_KHRONOS_validation"
        )
    )
  );

  auto groups = instance->get_physical_devices( {} );
  auto selected = groups[ 0 ].with_extensions( {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    VK_KHR_SWAPCHAIN_MUTABLE_FORMAT_EXTENSION_NAME,
  } );

  std::uint32_t width = 1024u;
  std::uint32_t height = 1024u;

  gct::glfw_window window( width, height, argc ? argv[ 0 ] : "my_application", false );
  bool close_app = false;
  bool iconified = false;
  window.set_on_closed( [&]( auto & ) { close_app = true; } );
  window.set_on_key( [&]( auto &, int key, int, int action, int ) {
    if( action == GLFW_PRESS ) {
      if( key == GLFW_KEY_Q ) close_app = true;
    }
  } );
  window.set_on_iconified(
    [&]( auto&, int i ) {
      iconified = i;
    }
  );

  auto surface = window.get_surface( *groups[ 0 ].devices[ 0 ] );
 
  std::vector< gct::queue_requirement_t > queue_requirements{
    gct::queue_requirement_t{
      vk::QueueFlagBits::eGraphics,
      0u,
      vk::Extent3D(),
#ifdef VK_EXT_GLOBAL_PRIORITY_EXTENSION_NAME
      vk::QueueGlobalPriorityEXT(),
#endif
      { **surface },
      vk::CommandPoolCreateFlagBits::eResetCommandBuffer
    }
  };
  const auto device = selected.create_device(
    queue_requirements,
    gct::device_create_info_t()
  );
  const auto queue = device->get_queue( 0u );

  const auto swapchain = device->get_swapchain( surface );
  const auto swapchain_images = swapchain->get_images();

  const auto descriptor_pool = device->get_descriptor_pool(
    gct::descriptor_pool_create_info_t()
      .set_basic(
        vk::DescriptorPoolCreateInfo()
          .setFlags( vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet )
          .setMaxSets( 10 )
      )
      .set_descriptor_pool_size( vk::DescriptorType::eUniformBuffer, 1 )
      .rebuild_chain()
  );

  const auto pipeline_cache = device->get_pipeline_cache();

  VmaAllocatorCreateInfo allocator_create_info{};
  const auto allocator = device->get_allocator(
    allocator_create_info
  );
  
  const auto render_pass = device->get_render_pass(
    gct::select_simple_surface_format( surface->get_caps().get_formats() ).basic.format,
    vk::Format::eD16Unorm
  );

  std::vector< fb_resources_t > framebuffers;
  for( std::size_t i = 0u; i != swapchain_images.size(); ++i ) {
    auto &image = swapchain_images[ i ];
    auto depth = allocator->create_image(
      gct::image_create_info_t()
        .set_basic(
          vk::ImageCreateInfo()
            .setImageType( vk::ImageType::e2D )
            .setFormat( vk::Format::eD16Unorm )
            .setExtent( image->get_props().get_basic().extent )
            .setUsage( vk::ImageUsageFlagBits::eDepthStencilAttachment )
        )
        .rebuild_chain(),
      VMA_MEMORY_USAGE_GPU_ONLY
    );
    auto depth_view = depth->get_view( vk::ImageAspectFlagBits::eDepth );
    auto color_view = image->get_view( vk::ImageAspectFlagBits::eColor );
    auto framebuffer = render_pass->get_framebuffer(
      gct::framebuffer_create_info_t()
        .add_attachment( color_view )
        .add_attachment( depth_view )
    );
    framebuffers.emplace_back(
      fb_resources_t{
        image,
        framebuffer,
        device->get_semaphore(),
        device->get_semaphore(),
        queue->get_command_pool()->allocate(),
        gct::render_pass_begin_info_t()
          .set_basic(
            vk::RenderPassBeginInfo()
              .setRenderPass( **render_pass )
              .setFramebuffer( **framebuffer )
              .setRenderArea( vk::Rect2D( vk::Offset2D(0, 0), vk::Extent2D((uint32_t)width, (uint32_t)height) ) )
          )
          .add_clear_value( vk::ClearColorValue( std::array< float, 4u >{ 0.0f, 0.0f, 0.0f, 1.0f } ) )
          .add_clear_value( vk::ClearDepthStencilValue( 1.f, 0 ) )
          .rebuild_chain()
      }
    );
  }

  const auto vs = device->get_shader_module( CMAKE_CURRENT_BINARY_DIR "/shader.vert.spv" );
  const auto fs = device->get_shader_module( CMAKE_CURRENT_BINARY_DIR "/shader.frag.spv" );
 
  const auto descriptor_set_layout = device->get_descriptor_set_layout(
    gct::descriptor_set_layout_create_info_t()
      .add_binding(
        vs->get_props().get_reflection()
      )
      .add_binding(
        fs->get_props().get_reflection()
      )
  );
  
  const auto gct_descriptor_set = descriptor_pool->allocate( descriptor_set_layout );
  const auto descriptor_set = **gct_descriptor_set;

  const auto gct_pipeline_layout = device->get_pipeline_layout(
    gct::pipeline_layout_create_info_t()
      .add_descriptor_set_layout( descriptor_set_layout )
  );
  const auto pipeline_layout = **gct_pipeline_layout;

  const auto [vistat,vamap,stride] = get_vertex_attributes(
    *device,
    vs->get_props().get_reflection()
  );

  const auto [input_assembly,host_vertex_buffer,vertex_count] = gct::primitive::create_sphere( vamap, stride, 12u, 6u );

  std::shared_ptr< gct::buffer_t > gct_vertex_buffer;
  {
    auto command_buffer = queue->get_command_pool()->allocate();
    {
      auto recorder = command_buffer->begin();
      gct_vertex_buffer = recorder.load_buffer(
        allocator,
        host_vertex_buffer.data(),
        host_vertex_buffer.size(),
        vk::BufferUsageFlagBits::eVertexBuffer
      );
      recorder.barrier(
        vk::AccessFlagBits::eTransferWrite,
        vk::AccessFlagBits::eVertexAttributeRead,
        vk::PipelineStageFlagBits::eTransfer,
        vk::PipelineStageFlagBits::eVertexInput,
        vk::DependencyFlagBits( 0 ),
        { gct_vertex_buffer },
        {}
      );
    }
    command_buffer->execute(
      gct::submit_info_t()
    );
    command_buffer->wait_for_executed();
  }
  const auto vertex_buffer = **gct_vertex_buffer;

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

  const auto stencil_op = vk::StencilOpState()
    .setCompareOp( vk::CompareOp::eAlways )
    .setFailOp( vk::StencilOp::eKeep )
    .setPassOp( vk::StencilOp::eKeep );

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

  const auto gct_pipeline = pipeline_cache->get_pipeline(
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
      .set_dynamic(
        gct::pipeline_dynamic_state_create_info_t()
      )
      .set_layout( gct_pipeline_layout )
      .set_render_pass( render_pass, 0 )
  );
  const auto pipeline = **gct_pipeline;

  uint32_t current_frame = 0u;
  while( !close_app ) {
    const auto begin_time = std::chrono::high_resolution_clock::now();
    if( !iconified ) {
      auto &sync = framebuffers[ current_frame ];
      const auto image_acquired = **sync.image_acquired;
      const auto draw_complete = **sync.draw_complete;
      const auto command_buffer = **sync.command_buffer;
      sync.command_buffer->wait_for_executed();
      auto image_index = swapchain->acquire_next_image( sync.image_acquired );
      auto &fb = framebuffers[ image_index ];
      const auto &render_pass_begin_info = static_cast< const VkRenderPassBeginInfo& >( fb.render_pass_begin_info.rebuild_chain().get_basic() );
      {
        auto recorder = sync.command_buffer->begin();

        // レンダーパスを開始する
        vkCmdBeginRenderPass(
          command_buffer,
          &render_pass_begin_info,
          VkSubpassContents::VK_SUBPASS_CONTENTS_INLINE
        );

        // このパイプラインを使う
        vkCmdBindPipeline(
          command_buffer,
          VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_GRAPHICS,
          pipeline
        );

        // このデスクリプタセットを使う
        VkDescriptorSet raw_descriptor_set = descriptor_set;
        vkCmdBindDescriptorSets(
          command_buffer,
          VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_GRAPHICS,
          pipeline_layout,
          // set=0から
          0,
          // 1個のデスクリプタセットを
          1,
          // これにする
          &raw_descriptor_set,
          0,
          nullptr
        );
       
        // この頂点バッファを使う
        // binding 0番がvertex_bufferの内容になる
        VkBuffer raw_vertex_buffer = vertex_buffer;
        VkDeviceSize vertex_buffer_offset = 0;
        vkCmdBindVertexBuffers(
          command_buffer,
          // binding 0番から
          0,
          // 1個の頂点バッファを
          1,
          // これにする
          &raw_vertex_buffer,
          // バッファの先頭から使う
          &vertex_buffer_offset
        );
       
        // パイプラインを実行する
        //
        vkCmdDraw(
          command_buffer,
          // 頂点バッファには頂点が3個あって
          vertex_count,
          // それを1回繰り返す
          1,
          // 最初の頂点はバッファの0番目の頂点で
          0,
          // 繰り返し回数は0からカウント
          0
        );

	// レンダーパスを終了する
        // サブパスがまだある時はvkCmdEndRenderPassする前にvkCmdNextSubpass
        vkCmdEndRenderPass(
          command_buffer
        );
      }
      sync.command_buffer->execute(
        gct::submit_info_t()
          .add_wait_for( sync.image_acquired, vk::PipelineStageFlagBits::eColorAttachmentOutput )
          .add_signal_to( sync.draw_complete )
      );
      queue->present(
        gct::present_info_t()
          .add_wait_for( sync.draw_complete )
          .add_swapchain( swapchain, image_index )
      );
      ++current_frame;
      current_frame %= framebuffers.size();
    }
    glfwPollEvents();
    gct::wait_for_sync( begin_time );
  }
  (*queue)->waitIdle();
}

