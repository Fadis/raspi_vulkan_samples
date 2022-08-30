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

  // 頂点シェーダをロード
  const auto vs = device->get_shader_module(
    CMAKE_CURRENT_BINARY_DIR "/shader.vert.spv"
  );

  // フラグメントシェーダをロード
  const auto fs = device->get_shader_module(
    CMAKE_CURRENT_BINARY_DIR "/shader.frag.spv"
  );
  
  // デスクリプタセットレイアウトを作る
  const auto descriptor_set_layout = device->get_descriptor_set_layout(
    gct::descriptor_set_layout_create_info_t()
      .add_binding(
        // このシェーダにあるデスクリプタを追加
        vs->get_props().get_reflection()
      )
      .add_binding(
        // このシェーダにあるデスクリプタを追加
        fs->get_props().get_reflection()
      )
  );

  // パイプラインレイアウトを作る
  const auto pipeline_layout = device->get_pipeline_layout(
    gct::pipeline_layout_create_info_t()
      // このデスクリプタセットレイアウトを使う
      .add_descriptor_set_layout( descriptor_set_layout )
  );

  // レンダーパスを作る
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

  // 何もしないステンシルの設定
  const auto stencil_op = vk::StencilOpState()
    // 常にtrue
    .setCompareOp( vk::CompareOp::eAlways )
    // 比較結果がtrueなら通過
    .setFailOp( vk::StencilOp::eKeep )
    // 比較結果がfalseなら通過
    .setPassOp( vk::StencilOp::eKeep );

  // シェーダから頂点配列のデータの並びを決める
  auto [vistat,vamap,stride] = gct::get_vertex_attributes(
    *device,
    vs->get_props().get_reflection()
  );
  
  // 三角形が1つだけの頂点配列を作る
  const auto [input_assembly,host_vertex_buffer,vertex_count] = gct::primitive::create_triangle( vamap, stride );
 
  // ビューポートとシザーの設定
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

  // ラスタライズの設定
  const auto rasterization =
    gct::pipeline_rasterization_state_create_info_t()
      .set_basic(
        vk::PipelineRasterizationStateCreateInfo()
          // 範囲外の深度を丸めない
          .setDepthClampEnable( false )
          // ラスタライズを行う
          .setRasterizerDiscardEnable( false )
          // 三角形の中を塗る
          .setPolygonMode( vk::PolygonMode::eFill )
          // 背面カリングを行わない
          .setCullMode( vk::CullModeFlagBits::eNone )
          // 表面は時計回り
          .setFrontFace( vk::FrontFace::eClockwise )
          // 深度バイアスを使わない
          .setDepthBiasEnable( false )
          // 線を描く時は太さ1.0で
          .setLineWidth( 1.0f )
      );

  // マルチサンプルの設定
  const auto multisample =
    gct::pipeline_multisample_state_create_info_t()
      .set_basic(
        // 全部デフォルト(マルチサンプルを使わない)
        vk::PipelineMultisampleStateCreateInfo()
      );

  // 深度とステンシルの設定
  const auto depth_stencil =
    gct::pipeline_depth_stencil_state_create_info_t()
      .set_basic(
        vk::PipelineDepthStencilStateCreateInfo()
          // 深度テストをする
          .setDepthTestEnable( true )
          // 深度値を深度バッファに書く
          .setDepthWriteEnable( true )
          // 深度値がより小さい場合手前と見做す
          .setDepthCompareOp( vk::CompareOp::eLessOrEqual )
          // 深度の範囲を制限しない
          .setDepthBoundsTestEnable( false )
          // ステンシルテストをしない
          .setStencilTestEnable( false )
          // ステンシルテストの演算を指定
          .setFront( stencil_op )
          .setBack( stencil_op )
      );

  // カラーブレンドの設定
  const auto color_blend =
    gct::pipeline_color_blend_state_create_info_t()
      .add_attachment(
        vk::PipelineColorBlendAttachmentState()
          // フレームバッファに既にある色と新しい色を混ぜない
          // (新しい色で上書きする)
          .setBlendEnable( false )
          // RGBA全ての要素を書く
          .setColorWriteMask(
            vk::ColorComponentFlagBits::eR |
            vk::ColorComponentFlagBits::eG |
            vk::ColorComponentFlagBits::eB |
            vk::ColorComponentFlagBits::eA
          )
      );
  
  // グラフィクスパイプラインを作る
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
      // このパイプラインレイアウトで
      .set_layout( pipeline_layout )
      // このレンダーパスの0番目のサブパスとして使う
      .set_render_pass( render_pass, 0 )
  );

  if( pipeline->get_props().has_creation_feedback() )
    std::cout << nlohmann::json( *pipeline->get_props().get_creation_feedback().pPipelineCreationFeedback ).dump( 2 ) << std::endl;

  // パイプラインキャッシュをファイルに保存する
  auto serialized = (*device)->getPipelineCacheData( **pipeline_cache );
  std::filesystem::remove( pipeline_cache_filename );
  std::fstream pipeline_cache_output( pipeline_cache_filename.string(), std::ios_base::out );
  pipeline_cache_output.write( reinterpret_cast< const char* >( serialized.data() ), serialized.size() );
}

