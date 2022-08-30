#include <iostream>
#include <unordered_set>
#include <utility>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/gtx/string_cast.hpp>
#include <gct/get_extensions.hpp>
#include <gct/setter.hpp>
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
#include <gct/pipeline_input_assembly_state_create_info.hpp>
#include <gct/pipeline_vertex_input_state_create_info.hpp>
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
#include <gct/vertex_attributes.hpp>
#include <gct/render_pass_begin_info.hpp>
#include <gct/primitive.hpp>

struct fb_resources_t {
  std::shared_ptr< gct::image_t > color;
  std::shared_ptr< gct::framebuffer_t > framebuffer;
  std::shared_ptr< gct::semaphore_t > image_acquired;
  std::shared_ptr< gct::semaphore_t > draw_complete;
  std::shared_ptr< gct::bound_command_buffer_t > command_buffer;
  gct::render_pass_begin_info_t render_pass_begin_info;
  std::shared_ptr< gct::descriptor_set_t > descriptor_set;
  std::shared_ptr< gct::buffer_t > uniform_staging;
  std::shared_ptr< gct::buffer_t > uniform;
};

struct uniform_t {
  LIBGCT_SETTER( projection_matrix )
  LIBGCT_SETTER( camera_matrix )
  LIBGCT_SETTER( world_matrix )
  glm::mat4 projection_matrix;
  glm::mat4 camera_matrix;
  glm::mat4 world_matrix;
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
    VK_KHR_SWAPCHAIN_MUTABLE_FORMAT_EXTENSION_NAME
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
  auto device = selected.create_device(
    queue_requirements,
    gct::device_create_info_t()
  );
  auto queue = device->get_queue( 0u );

  auto swapchain = device->get_swapchain( surface );
  auto swapchain_images = swapchain->get_images();

  auto descriptor_pool = device->get_descriptor_pool(
    gct::descriptor_pool_create_info_t()
      .set_basic(
        vk::DescriptorPoolCreateInfo()
          .setFlags( vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet )
          .setMaxSets( 10 )
      )
      .set_descriptor_pool_size( vk::DescriptorType::eUniformBuffer, 1 )
      .rebuild_chain()
  );

  auto pipeline_cache = device->get_pipeline_cache();

  VmaAllocatorCreateInfo allocator_create_info{};
  auto allocator = device->get_allocator(
    allocator_create_info
  );
  
  const auto render_pass = device->get_render_pass(
    gct::select_simple_surface_format( surface->get_caps().get_formats() ).basic.format,
    vk::Format::eD16Unorm
  );

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
    auto uniform_staging = allocator->create_buffer(
      gct::buffer_create_info_t()
        .set_basic(
          vk::BufferCreateInfo()
            .setSize( sizeof( uniform_t ) )
            .setUsage( vk::BufferUsageFlagBits::eTransferSrc )
        ),
      VMA_MEMORY_USAGE_CPU_TO_GPU
    );
    auto uniform = allocator->create_buffer(
      gct::buffer_create_info_t()
        .set_basic(
          vk::BufferCreateInfo()
            .setSize( sizeof( uniform_t ) )
            .setUsage( vk::BufferUsageFlagBits::eTransferDst|vk::BufferUsageFlagBits::eUniformBuffer )
        ),
      VMA_MEMORY_USAGE_GPU_ONLY
    );
    auto descriptor_set = descriptor_pool->allocate( descriptor_set_layout );
    std::vector< gct::write_descriptor_set_t > updates;
    updates.push_back(
      gct::write_descriptor_set_t()
        .set_basic(
          (*descriptor_set)[ "uniforms" ]
        )
        .add_buffer(
          gct::descriptor_buffer_info_t()
            .set_buffer( uniform )
            .set_basic(
              vk::DescriptorBufferInfo()
                .setOffset( 0 )
                .setRange( sizeof( uniform_t ) )
            )
        )
    );
    descriptor_set->update( updates );
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
          .rebuild_chain(),
        descriptor_set,
        uniform_staging,
        uniform
      }
    );
  }

  const auto pipeline_layout = device->get_pipeline_layout(
    gct::pipeline_layout_create_info_t()
      .add_descriptor_set_layout( descriptor_set_layout )
  );

  auto [vistat,vamap,stride] = get_vertex_attributes(
    *device,
    vs->get_props().get_reflection()
  );

  const auto [input_assembly,host_vertex_buffer,vertex_count] = gct::primitive::create_sphere( vamap, stride, 12u, 6u );
  //const auto [input_assembly,host_vertex_buffer,vertex_count] = gct::primitive::create_cube( vamap, stride );

  std::shared_ptr< gct::buffer_t > vertex_buffer;
  {
    auto command_buffer = queue->get_command_pool()->allocate();
    {
      auto recorder = command_buffer->begin();
      vertex_buffer = recorder.load_buffer(
        allocator,
        host_vertex_buffer.data(),
        sizeof( float ) * host_vertex_buffer.size(),
        vk::BufferUsageFlagBits::eVertexBuffer
      );
      recorder.barrier(
        vk::AccessFlagBits::eTransferWrite,
        vk::AccessFlagBits::eVertexAttributeRead,
        vk::PipelineStageFlagBits::eTransfer,
        vk::PipelineStageFlagBits::eVertexInput,
        vk::DependencyFlagBits( 0 ),
        { vertex_buffer },
        {}
      );
    }
    command_buffer->execute(
      gct::submit_info_t()
    );
    command_buffer->wait_for_executed();
  }


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
          .setCullMode( vk::CullModeFlagBits::eBack )
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
      .set_dynamic(
        gct::pipeline_dynamic_state_create_info_t()
      )
      .set_layout( pipeline_layout )
      .set_render_pass( render_pass, 0 )
  );

  auto camera_pos = glm::vec3{ 0.f, -3.f, 6.0f };
  float camera_angle = 0;//M_PI;
  auto uniforms = uniform_t()
    .set_projection_matrix(
      glm::perspective( 0.39959648408210363f, (float(width)/float(height)), 0.1f, 150.f )
    )
    .set_camera_matrix(
      glm::lookAt(
        camera_pos,
        glm::vec3( 0.f, 0.f, 0.f ),
        glm::vec3{ 0.f, camera_pos[ 1 ] + 100.f, 0.f }
      )
    )
    .set_world_matrix(
      glm::mat4( 1.0 )
    );

  uint32_t current_frame = 0u;
  float angle = 0.f;
  while( !close_app ) {
    const auto begin_time = std::chrono::high_resolution_clock::now();
    angle += 1.f / 60.f;
    uniforms
      .set_world_matrix(
        glm::mat4(
          std::cos( angle ), 0.f, -std::sin( angle ), 0.f,
          0.f, 1.f, 0.f, 0.f,
          std::sin( angle ), 0.f, std::cos( angle ), 0.f,
          0.f, 0.f, 0.f, 1.f
        )
      );

    if( !iconified ) {
      auto &sync = framebuffers[ current_frame ];
      sync.command_buffer->wait_for_executed();
      auto image_index = swapchain->acquire_next_image( sync.image_acquired );
      auto &fb = framebuffers[ image_index ];
      {
        auto recorder = sync.command_buffer->begin();
        recorder.copy(
          uniforms,
          fb.uniform_staging,
          fb.uniform
        );
        recorder.barrier(
          vk::AccessFlagBits::eTransferRead,
          vk::AccessFlagBits::eShaderRead,
          vk::PipelineStageFlagBits::eTransfer,
          vk::PipelineStageFlagBits::eVertexShader,
          vk::DependencyFlagBits( 0 ),
          { fb.uniform },
          {}
        );
        auto render_pass_token = recorder.begin_render_pass(
          fb.render_pass_begin_info,
          vk::SubpassContents::eInline
        );
        recorder.bind_pipeline( pipeline );
        recorder.bind_descriptor_set(
          vk::PipelineBindPoint::eGraphics,
          pipeline_layout,
          fb.descriptor_set
        );
        recorder.bind_vertex_buffer( vertex_buffer );
        recorder->draw( vertex_count, 1, 0, 0 );
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

