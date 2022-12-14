#include <iostream>
#include <unordered_set>
#include <utility>
#include <fstream>
#include <gct/get_extensions.hpp>
#include <gct/instance.hpp>
#include <gct/device.hpp>
#include <gct/device_create_info.hpp>
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
#include <gct/buffer_view_create_info.hpp>
#include <gct/vertex_attributes.hpp>
#include <gct/primitive.hpp>
#include <gct/mmaped_file.hpp>
#include <nlohmann/json.hpp>
#include <vulkan2json/PipelineCreationFeedbackEXT.hpp>

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
    )
  );
  auto groups = instance->get_physical_devices( {} );
  auto physical_device = groups[ 0 ].with_extensions( {
    VK_EXT_PIPELINE_CREATION_FEEDBACK_EXTENSION_NAME
  } );
  
  std::uint32_t width = 1024u;
  std::uint32_t height = 1024u;
 
  const auto device = physical_device.create_device(
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

  // ??????????????????????????????
  const auto vs = device->get_shader_module(
    CMAKE_CURRENT_BINARY_DIR "/shader.vert.spv"
  );

  // ??????????????????????????????????????????
  const auto fs = device->get_shader_module(
    CMAKE_CURRENT_BINARY_DIR "/shader.frag.spv"
  );
  
  // ???????????????????????????????????????????????????
  const auto descriptor_set_layout = device->get_descriptor_set_layout(
    gct::descriptor_set_layout_create_info_t()
      .add_binding(
        // ??????????????????????????????????????????????????????
        vs->get_props().get_reflection()
      )
      .add_binding(
        // ??????????????????????????????????????????????????????
        fs->get_props().get_reflection()
      )
  );

  // ??????????????????????????????????????????
  const auto pipeline_layout = device->get_pipeline_layout(
    gct::pipeline_layout_create_info_t()
      // ?????????????????????????????????????????????????????????
      .add_descriptor_set_layout( descriptor_set_layout )
  );

  // ???????????????????????????
  const auto render_pass = device->get_render_pass(
    vk::Format::eR8G8B8A8Unorm,
    vk::Format::eD16Unorm
  );

  std::filesystem::path pipeline_cache_filename(
    CMAKE_CURRENT_BINARY_DIR "/pipeline_cache"
  );
  
  const auto pipeline_cache = device->get_pipeline_cache(
    std::filesystem::exists( pipeline_cache_filename )  ? 
      gct::pipeline_cache_create_info_t()
        .load( pipeline_cache_filename.string() )
	.rebuild_chain():
      gct::pipeline_cache_create_info_t()
        .rebuild_chain()
  );

  // ???????????????????????????????????????
  const auto stencil_op = vk::StencilOpState()
    // ??????true
    .setCompareOp( vk::CompareOp::eAlways )
    // ???????????????true????????????
    .setFailOp( vk::StencilOp::eKeep )
    // ???????????????false????????????
    .setPassOp( vk::StencilOp::eKeep );

  // ???????????????????????????????????????????????????????????????
  auto [vistat,vamap,stride] = gct::get_vertex_attributes(
    *device,
    vs->get_props().get_reflection()
  );
  
  // ????????????1?????????????????????????????????
  const auto [input_assembly,host_vertex_buffer,vertex_count] = gct::primitive::create_triangle( vamap, stride );
 
  // ???????????????????????????????????????
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

  // ???????????????????????????
  const auto rasterization =
    gct::pipeline_rasterization_state_create_info_t()
      .set_basic(
        vk::PipelineRasterizationStateCreateInfo()
          // ?????????????????????????????????
          .setDepthClampEnable( false )
          // ???????????????????????????
          .setRasterizerDiscardEnable( false )
          // ????????????????????????
          .setPolygonMode( vk::PolygonMode::eFill )
          // ?????????????????????????????????
          .setCullMode( vk::CullModeFlagBits::eNone )
          // ?????????????????????
          .setFrontFace( vk::FrontFace::eClockwise )
          // ?????????????????????????????????
          .setDepthBiasEnable( false )
          // ????????????????????????1.0???
          .setLineWidth( 1.0f )
      );

  // ??????????????????????????????
  const auto multisample =
    gct::pipeline_multisample_state_create_info_t()
      .set_basic(
        // ?????????????????????(????????????????????????????????????)
        vk::PipelineMultisampleStateCreateInfo()
      );

  // ?????????????????????????????????
  const auto depth_stencil =
    gct::pipeline_depth_stencil_state_create_info_t()
      .set_basic(
        vk::PipelineDepthStencilStateCreateInfo()
          // ????????????????????????
          .setDepthTestEnable( true )
          // ???????????????????????????????????????
          .setDepthWriteEnable( true )
          // ???????????????????????????????????????????????????
          .setDepthCompareOp( vk::CompareOp::eLessOrEqual )
          // ?????????????????????????????????
          .setDepthBoundsTestEnable( false )
          // ????????????????????????????????????
          .setStencilTestEnable( false )
          // ??????????????????????????????????????????
          .setFront( stencil_op )
          .setBack( stencil_op )
      );

  // ??????????????????????????????
  const auto color_blend =
    gct::pipeline_color_blend_state_create_info_t()
      .add_attachment(
        vk::PipelineColorBlendAttachmentState()
          // ????????????????????????????????????????????????????????????????????????
          // (??????????????????????????????)
          .setBlendEnable( false )
          // RGBA????????????????????????
          .setColorWriteMask(
            vk::ColorComponentFlagBits::eR |
            vk::ColorComponentFlagBits::eG |
            vk::ColorComponentFlagBits::eB |
            vk::ColorComponentFlagBits::eA
          )
      );
  
  // ?????????????????????????????????????????????
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
      // ??????????????????????????????????????????
      .set_layout( pipeline_layout )
      // ???????????????????????????0????????????????????????????????????
      .set_render_pass( render_pass, 0 )
  );

  // ??????????????????????????????????????????????????????????????????
  if( pipeline->get_props().has_creation_feedback() )
    std::cout << nlohmann::json( *pipeline->get_props().get_creation_feedback().pPipelineCreationFeedback ).dump( 2 ) << std::endl;

  // ???????????????????????????????????????????????????????????????
  auto serialized = (*device)->getPipelineCacheData( **pipeline_cache );
  std::filesystem::remove( pipeline_cache_filename );
  std::fstream pipeline_cache_output( pipeline_cache_filename.string(), std::ios_base::out );
  pipeline_cache_output.write( reinterpret_cast< const char* >( serialized.data() ), serialized.size() );
}

