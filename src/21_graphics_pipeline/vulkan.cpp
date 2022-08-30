#include <cmath>
#include <iostream>
#include <fstream>
#include <nlohmann/json.hpp>
#include <gct/get_extensions.hpp>
#include <gct/instance.hpp>
#include <gct/queue.hpp>
#include <gct/device.hpp>
#include <gct/allocator.hpp>
#include <gct/device_create_info.hpp>
#include <gct/image_create_info.hpp>
#include <gct/swapchain.hpp>
#include <gct/descriptor_set_layout.hpp>
#include <gct/pipeline_cache.hpp>
#include <gct/pipeline_layout_create_info.hpp>
#include <gct/buffer_view_create_info.hpp>
#include <gct/submit_info.hpp>
#include <gct/shader_module_create_info.hpp>
#include <gct/shader_module.hpp>
#include <gct/graphics_pipeline_create_info.hpp>
#include <gct/graphics_pipeline.hpp>
#include <gct/sampler_create_info.hpp>
#include <gct/pipeline_layout.hpp>
#include <gct/mmaped_file.hpp>
#include <nlohmann/json.hpp>
#include <vulkan2json/PipelineCreationFeedbackEXT.hpp>

int main( int argc, const char *argv[] ) {
  const std::shared_ptr< gct::instance_t > gct_instance(
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
  auto groups = gct_instance->get_physical_devices( {} );
  auto gct_physical_device = groups[ 0 ].with_extensions( {
    VK_EXT_PIPELINE_CREATION_FEEDBACK_EXTENSION_NAME
  } );
  
  std::uint32_t width = 1024u;
  std::uint32_t height = 1024u;
 
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

  const auto gct_vs = gct_device->get_shader_module(
    CMAKE_CURRENT_BINARY_DIR "/shader.vert.spv"
  );

  const auto gct_fs = gct_device->get_shader_module(
    CMAKE_CURRENT_BINARY_DIR "/shader.frag.spv"
  );
  
  const auto gct_descriptor_set_layout = gct_device->get_descriptor_set_layout(
    gct::descriptor_set_layout_create_info_t()
      .add_binding(
        gct_vs->get_props().get_reflection()
      )
      .add_binding(
        gct_fs->get_props().get_reflection()
      )
  );

  const auto gct_pipeline_layout = gct_device->get_pipeline_layout(
    gct::pipeline_layout_create_info_t()
      .add_descriptor_set_layout( gct_descriptor_set_layout )
  );
  
  std::filesystem::path pipeline_cache_filename(
    CMAKE_CURRENT_BINARY_DIR "/pipeline_cache"
  );

  const auto gct_pipeline_cache = gct_device->get_pipeline_cache(
    std::filesystem::exists( pipeline_cache_filename )  ? 
      gct::pipeline_cache_create_info_t()
        .load( pipeline_cache_filename.string() )
	.rebuild_chain():
      gct::pipeline_cache_create_info_t()
        .rebuild_chain()
  );

  const auto instance = **gct_instance;
  const auto physical_device = **gct_physical_device.devices[ 0 ];
  const auto device = **gct_device;
  const auto vs = **gct_vs;
  const auto fs = **gct_fs;
  const auto descriptor_set_layout = **gct_descriptor_set_layout;
  const auto pipeline_cache = **gct_pipeline_cache;
  const auto pipeline_layout = **gct_pipeline_layout;

  std::vector< VkAttachmentDescription > attachments( 2 );
  // 色を出力するアタッチメントを追加
  attachments[ 0 ].flags = 0;
  // RGBA各8bitの
  attachments[ 0 ].format = VkFormat::VK_FORMAT_R8G8B8A8_UNORM;
  // サンプリング点が1テクセルあたり1つで
  attachments[ 0 ].samples = VkSampleCountFlagBits::VK_SAMPLE_COUNT_1_BIT;
  // レンダーパスに入る前の値は読めなくてもよくて
  attachments[ 0 ].loadOp = VkAttachmentLoadOp::VK_ATTACHMENT_LOAD_OP_CLEAR;
  // 書いた内容はレンダーパスの後で読める必要があり
  attachments[ 0 ].storeOp = VkAttachmentStoreOp::VK_ATTACHMENT_STORE_OP_STORE;
  // ステンシルとして読める必要がなくて
  attachments[ 0 ].stencilLoadOp = VkAttachmentLoadOp::VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  // ステンシルとして書ける必要もない
  attachments[ 0 ].stencilStoreOp = VkAttachmentStoreOp::VK_ATTACHMENT_STORE_OP_DONT_CARE;
  // 初期レイアウトは何でも良くて
  attachments[ 0 ].initialLayout = VkImageLayout::VK_IMAGE_LAYOUT_UNDEFINED;
  // このレンダーパスが終わったら表示に適したレイアウトになっている
  attachments[ 0 ].finalLayout = VkImageLayout::VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
  
  // 深度を出力するアタッチメントを追加
  attachments[ 1 ].flags = 0;
  // D16bitの
  attachments[ 1 ].format = VkFormat::VK_FORMAT_D16_UNORM;
  // サンプリング点が1テクセルあたり1つで
  attachments[ 1 ].samples = VkSampleCountFlagBits::VK_SAMPLE_COUNT_1_BIT;
  // レンダーパスに入る前の値は読めなくてもよくて
  attachments[ 1 ].loadOp = VkAttachmentLoadOp::VK_ATTACHMENT_LOAD_OP_CLEAR;
  // 書いた内容はレンダーパスの後で読める必要があり
  attachments[ 1 ].storeOp = VkAttachmentStoreOp::VK_ATTACHMENT_STORE_OP_STORE;
  // ステンシルとして読める必要がなくて
  attachments[ 1 ].stencilLoadOp = VkAttachmentLoadOp::VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  // ステンシルとして書ける必要もない
  attachments[ 1 ].stencilStoreOp = VkAttachmentStoreOp::VK_ATTACHMENT_STORE_OP_DONT_CARE;
  // 初期レイアウトは何でも良くて
  attachments[ 1 ].initialLayout = VkImageLayout::VK_IMAGE_LAYOUT_UNDEFINED;
  // このレンダーパスが終わったら深度とステンシルを保持するのに適したレイアウトになっている
  attachments[ 1 ].finalLayout = VkImageLayout::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkAttachmentReference color_attachment;
  // 0番目のアタッチメントを
  color_attachment.attachment = 0;
  // 色を吐くのに丁度良いレイアウトにして使う
  color_attachment.layout = VkImageLayout::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkAttachmentReference depth_attachment;
  // 1番目のアタッチメントを
  depth_attachment.attachment = 1;
  // 深度とステンシルを吐くのに丁度良いレイアウトにして使う
  depth_attachment.layout = VkImageLayout::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  // サブパスを定義する
  std::vector< VkSubpassDescription > subpass( 1 );
  subpass[ 0 ].flags = 0;
  // グラフィクスパイプラインに対応するサブパス
  subpass[ 0 ].pipelineBindPoint = VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_GRAPHICS;
  // Input Attachmentは使わない
  subpass[ 0 ].inputAttachmentCount = 0;
  subpass[ 0 ].pInputAttachments = nullptr;
  // Color Attachmentを1つ使う
  subpass[ 0 ].colorAttachmentCount = 1;
  subpass[ 0 ].pColorAttachments = &color_attachment;
  // Depth Stencil Attachmentを使う
  subpass[ 0 ].pDepthStencilAttachment = &depth_attachment;
  // Resolve Attachmentを使わない
  subpass[ 0 ].pResolveAttachments = nullptr;
  // 他に依存するアタッチメントは無い
  subpass[ 0 ].preserveAttachmentCount = 0;
  subpass[ 0 ].pPreserveAttachments = nullptr;


  // レンダーパスを作る
  VkRenderPassCreateInfo render_pass_create_info;
  render_pass_create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  render_pass_create_info.pNext = nullptr;
  render_pass_create_info.flags = 0;
  // このアタッチメントがあり
  render_pass_create_info.attachmentCount = attachments.size();
  render_pass_create_info.pAttachments = attachments.data();
  // このサブパスがある
  render_pass_create_info.subpassCount = subpass.size();
  render_pass_create_info.pSubpasses = subpass.data();
  // サブパス間に依存関係はない
  render_pass_create_info.dependencyCount = 0;
  render_pass_create_info.pDependencies = nullptr;
  VkRenderPass render_pass;
  if( vkCreateRenderPass(
    device,
    &render_pass_create_info,
    nullptr,
    &render_pass
  ) != VK_SUCCESS ) abort();
  

  // 使用するシェーダモジュール
  std::vector< VkPipelineShaderStageCreateInfo > shader( 2 );
  shader[ 0 ].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  shader[ 0 ].pNext = nullptr;
  shader[ 0 ].flags = 0;
  shader[ 0 ].stage = VkShaderStageFlagBits::VK_SHADER_STAGE_VERTEX_BIT;
  shader[ 0 ].module = vs;
  shader[ 0 ].pName = "main";
  shader[ 0 ].pSpecializationInfo = nullptr;
  shader[ 1 ].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  shader[ 1 ].pNext = nullptr;
  shader[ 1 ].flags = 0;
  shader[ 1 ].stage = VkShaderStageFlagBits::VK_SHADER_STAGE_FRAGMENT_BIT;
  shader[ 1 ].module = fs;
  shader[ 1 ].pName = "main";
  shader[ 1 ].pSpecializationInfo = nullptr;

  // 頂点配列の読み方
  std::vector< VkVertexInputBindingDescription > vib( 1 );
  // 頂点配列binding 0番は
  vib[ 0 ].binding = 0;
  // 頂点1個毎に
  vib[ 0 ].inputRate = VkVertexInputRate::VK_VERTEX_INPUT_RATE_VERTEX;
  // 12バイト移動しながら読む
  vib[ 0 ].stride = sizeof( float ) * 3;

  std::vector< VkVertexInputAttributeDescription > via( 1 );
  // 頂点シェーダ上のlocation = 0の入力値は
  via[ 0 ].location = 0;
  // 頂点配列の先頭から
  via[ 0 ].offset = 0;
  // 頂点配列binding 0番の読み方で
  via[ 0 ].binding = 0;
  // 置かれたデータをfloatが3個のベクタと見做して読んだもの
  via[ 0 ].format = VkFormat::VK_FORMAT_R32G32B32_SFLOAT;

  // 頂点配列の読み方
  VkPipelineVertexInputStateCreateInfo vistat;
  vistat.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vistat.pNext = nullptr;
  vistat.flags = 0;
  // このinput binding descriptionと
  vistat.vertexBindingDescriptionCount = vib.size();
  vistat.pVertexBindingDescriptions = vib.data();
  // このinput attribute descriptionを使う
  vistat.vertexAttributeDescriptionCount = via.size();
  vistat.pVertexAttributeDescriptions = via.data();

  // プリミティブの組み立て方
  VkPipelineInputAssemblyStateCreateInfo input_assembly;
  input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  input_assembly.pNext = nullptr;
  input_assembly.flags = 0;
  // 頂点配列の要素3個毎に1つの三角形
  input_assembly.topology = VkPrimitiveTopology::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  input_assembly.primitiveRestartEnable = false;

  // 画面全体を覆うビューポート
  VkViewport viewport_;
  viewport_.x = 0;
  viewport_.y = 0;
  viewport_.width = width;
  viewport_.height = height;
  viewport_.minDepth = 0.f;
  viewport_.maxDepth = 1.f;

  // 画面全体を覆うシザー
  VkRect2D scissor;
  scissor.offset.x = 0;
  scissor.offset.y = 0;
  scissor.extent.width = width;
  scissor.extent.height = height;

  // ビューポートとシザーの設定
  VkPipelineViewportStateCreateInfo viewport;
  viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewport.pNext = nullptr;
  viewport.flags = 0;
  // このビューポートと
  viewport.viewportCount = 1;
  viewport.pViewports = &viewport_;
  // このシザーを使う
  viewport.scissorCount = 1;
  viewport.pScissors = &scissor;

  // ラスタライズの設定
  VkPipelineRasterizationStateCreateInfo rasterization;
  rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterization.pNext = nullptr;
  rasterization.flags = 0;
  // 範囲外の深度を丸めない
  rasterization.depthClampEnable = false;
  // ラスタライズを行う
  rasterization.rasterizerDiscardEnable = false;
  // 三角形の中を塗る
  rasterization.polygonMode = VkPolygonMode::VK_POLYGON_MODE_FILL;
  // 背面カリングを行わない
  rasterization.cullMode = VkCullModeFlagBits::VK_CULL_MODE_NONE;
  // 表面は時計回り
  rasterization.frontFace = VkFrontFace::VK_FRONT_FACE_CLOCKWISE;
  // 深度バイアスを使わない
  rasterization.depthBiasEnable = false;
  rasterization.depthBiasConstantFactor = 0.f;
  rasterization.depthBiasClamp = 0.f;
  rasterization.depthBiasSlopeFactor = 0.f;
  // 線を描く時は太さ1.0で
  rasterization.lineWidth = 1.0f;

  // マルチサンプルの設定
  VkPipelineMultisampleStateCreateInfo multisample;
  multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisample.pNext = nullptr;
  multisample.flags = 0;
  // 1テクセルあたりのサンプリング点の数は1つ
  multisample.rasterizationSamples = VkSampleCountFlagBits::VK_SAMPLE_COUNT_1_BIT;
  // サンプルシェーディングを使わない
  multisample.sampleShadingEnable = false;
  multisample.minSampleShading = 0.f;
  multisample.pSampleMask = nullptr;
  multisample.alphaToCoverageEnable = false;
  multisample.alphaToOneEnable = false;

  VkPipelineDepthStencilStateCreateInfo depth_stencil;
  depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depth_stencil.pNext = nullptr;
  depth_stencil.flags = 0;
  // 深度テストをする
  depth_stencil.depthTestEnable = true;
  // 深度値を深度バッファに書く
  depth_stencil.depthWriteEnable = true;
  // 深度値がより小さい場合手前と見做す
  depth_stencil.depthCompareOp = VkCompareOp::VK_COMPARE_OP_LESS_OR_EQUAL;
  // 深度の範囲を制限しない
  depth_stencil.depthBoundsTestEnable = false;
  depth_stencil.minDepthBounds = 0.f;
  depth_stencil.maxDepthBounds = 0.f;
  // ステンシルテストをしない
  depth_stencil.stencilTestEnable = false;
  depth_stencil.front.failOp = VkStencilOp::VK_STENCIL_OP_KEEP; 
  depth_stencil.front.passOp = VkStencilOp::VK_STENCIL_OP_KEEP; 
  depth_stencil.front.depthFailOp = VkStencilOp::VK_STENCIL_OP_KEEP; 
  depth_stencil.front.compareOp = VkCompareOp::VK_COMPARE_OP_NEVER;
  depth_stencil.front.compareMask = 0;
  depth_stencil.front.writeMask = 0;
  depth_stencil.front.reference = 0;
  depth_stencil.back.failOp = VkStencilOp::VK_STENCIL_OP_KEEP; 
  depth_stencil.back.passOp = VkStencilOp::VK_STENCIL_OP_KEEP; 
  depth_stencil.back.depthFailOp = VkStencilOp::VK_STENCIL_OP_KEEP; 
  depth_stencil.back.compareOp = VkCompareOp::VK_COMPARE_OP_NEVER;
  depth_stencil.back.compareMask = 0;
  depth_stencil.back.writeMask = 0;
  depth_stencil.back.reference = 0;

  // カラーブレンドの設定
  VkPipelineColorBlendAttachmentState color_blend_attachment;
  // フレームバッファに既にある色と新しい色を混ぜない
  // (新しい色で上書きする)
  color_blend_attachment.blendEnable = false;
  color_blend_attachment.srcColorBlendFactor = VkBlendFactor::VK_BLEND_FACTOR_ZERO;
  color_blend_attachment.dstColorBlendFactor = VkBlendFactor::VK_BLEND_FACTOR_ZERO;
  color_blend_attachment.colorBlendOp = VkBlendOp::VK_BLEND_OP_ADD;
  color_blend_attachment.srcAlphaBlendFactor = VkBlendFactor::VK_BLEND_FACTOR_ZERO;
  color_blend_attachment.dstAlphaBlendFactor = VkBlendFactor::VK_BLEND_FACTOR_ZERO;
  color_blend_attachment.alphaBlendOp = VkBlendOp::VK_BLEND_OP_ADD;
  // RGBA全ての要素を書く
  color_blend_attachment.colorWriteMask =
    VkColorComponentFlagBits::VK_COLOR_COMPONENT_R_BIT |
    VkColorComponentFlagBits::VK_COLOR_COMPONENT_G_BIT |
    VkColorComponentFlagBits::VK_COLOR_COMPONENT_B_BIT |
    VkColorComponentFlagBits::VK_COLOR_COMPONENT_A_BIT;

  VkPipelineColorBlendStateCreateInfo color_blend;
  color_blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  color_blend.pNext = nullptr;
  color_blend.flags = 0;
  // 論理演算をしない
  color_blend.logicOpEnable = false;
  // 論理演算をする場合clearを使う
  color_blend.logicOp = VkLogicOp::VK_LOGIC_OP_CLEAR;
  // カラーアタッチメント1つ分の設定がある
  color_blend.attachmentCount = 1;
  // このブレンドの設定を0番目のカラーアタッチメントで使う
  color_blend.pAttachments = &color_blend_attachment;
  // カラーブレンドに使う定数
  color_blend.blendConstants[ 0 ] = 0.f;
  color_blend.blendConstants[ 1 ] = 0.f;
  color_blend.blendConstants[ 2 ] = 0.f;
  color_blend.blendConstants[ 3 ] = 0.f;

  // 後から変更できるパラメータの設定
  const std::vector< VkDynamicState > dynamic_state{};
  VkPipelineDynamicStateCreateInfo dynamic;
  dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamic.pNext = nullptr;
  dynamic.flags = 0;
  dynamic.dynamicStateCount = 0;
  dynamic.pDynamicStates = nullptr;

  // テッセレーションの設定
  VkPipelineTessellationStateCreateInfo tessellation;
  tessellation.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
  tessellation.pNext = nullptr;
  tessellation.flags = 0;
  tessellation.patchControlPoints = 1;

  // パイプラインのコンパイルにかかった時間を測れるようにする
  std::vector< VkPipelineCreationFeedbackEXT > feedback_( 3u );
  feedback_[ 0 ].flags = 0u;
  feedback_[ 0 ].duration = 0u;
  feedback_[ 1 ].flags = 0u;
  feedback_[ 1 ].duration = 0u;
  VkPipelineCreationFeedbackCreateInfoEXT feedback;
  feedback.sType = VK_STRUCTURE_TYPE_PIPELINE_CREATION_FEEDBACK_CREATE_INFO_EXT;
  feedback.pNext = nullptr;
  feedback.pPipelineCreationFeedback = &feedback_.back();
  feedback.pipelineStageCreationFeedbackCount = feedback_.size() - 1u;
  feedback.pPipelineStageCreationFeedbacks = feedback_.data();

  // グラフィクスパイプラインを作る
  VkGraphicsPipelineCreateInfo create_info;
  create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  create_info.pNext = &feedback;
  create_info.flags = 0;
  create_info.stageCount = shader.size();
  create_info.pStages = shader.data();
  create_info.pVertexInputState = &vistat;
  create_info.pInputAssemblyState = &input_assembly;
  create_info.pTessellationState = &tessellation;
  create_info.pViewportState = &viewport;
  create_info.pRasterizationState = &rasterization;
  create_info.pMultisampleState = &multisample;
  create_info.pDepthStencilState = &depth_stencil;
  create_info.pColorBlendState = &color_blend;
  create_info.pDynamicState = &dynamic;
  create_info.layout = pipeline_layout;
  create_info.renderPass = render_pass;
  create_info.subpass = 0;
  create_info.basePipelineHandle = VK_NULL_HANDLE;
  create_info.basePipelineIndex = 0;
  VkPipeline pipeline;
  if( vkCreateGraphicsPipelines(
    device,
    pipeline_cache,
    1,
    &create_info,
    nullptr,
    &pipeline
  ) != VK_SUCCESS ) abort();
  
  vkDestroyPipeline(
    device,
    pipeline,
    nullptr
  );

  vkDestroyRenderPass(
    device,
    render_pass,
    nullptr
  );
  
  // パイプラインのコンパイルにかかった時間を表示
  std::cout << nlohmann::json( feedback_.back() ).dump( 2 ) << std::endl;
  
  // パイプラインキャッシュをファイルに保存する
  std::size_t serialized_size = 0u;
  if( vkGetPipelineCacheData(
    device,
    pipeline_cache,
    &serialized_size,
    nullptr
  ) != VK_SUCCESS ) std::abort();
  std::vector< std::uint8_t > serialized( serialized_size );
  if( vkGetPipelineCacheData(
    device,
    pipeline_cache,
    &serialized_size,
    serialized.data()
  ) != VK_SUCCESS ) std::abort();
  std::filesystem::remove( pipeline_cache_filename );
  std::fstream pipeline_cache_output( pipeline_cache_filename.string(), std::ios_base::out );
  pipeline_cache_output.write( reinterpret_cast< const char* >( serialized.data() ), serialized.size() );
 
}

