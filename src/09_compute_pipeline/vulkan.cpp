#include <iostream>
#include <gct/instance.hpp>
#include <gct/device.hpp>
#include <gct/allocator.hpp>
#include <gct/device_create_info.hpp>
#include <gct/descriptor_pool.hpp>
#include <gct/descriptor_set_layout.hpp>
#include <gct/shader_module.hpp>
#include <vulkan/vulkan.h>

struct spec_t {
  std::uint32_t local_x_size = 0u;
  std::uint32_t local_y_size = 0u;
};

int main( int argc, const char *argv[] ) {
  std::shared_ptr< gct::instance_t > gct_instance(
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
  auto groups = gct_instance->get_physical_devices( {} );
  auto gct_physical_device = groups[ 0 ].with_extensions( {} );
  auto gct_device =
    gct_physical_device
      .create_device(
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
  const auto gct_shader = gct_device->get_shader_module(
    CMAKE_CURRENT_BINARY_DIR "/shader.comp.spv"
  );
  auto gct_descriptor_set_layout = gct_device->get_descriptor_set_layout(
    gct::descriptor_set_layout_create_info_t()
      .add_binding( gct_shader->get_props().get_reflection() )
      .rebuild_chain()
  );
  const auto instance = VkInstance( **gct_instance );
  const auto physical_device = VkPhysicalDevice( **gct_physical_device.devices[ 0 ] );
  const auto device = VkDevice( **gct_device );
  const auto shader = VkShaderModule( **gct_shader );
  const auto descriptor_set_layout = VkDescriptorSetLayout( **gct_descriptor_set_layout );

  // パイプラインレイアウトを作る
  VkDescriptorSetLayout descriptor_set_layout_ = descriptor_set_layout;
  VkPipelineLayoutCreateInfo pipeline_layout_create_info;
  pipeline_layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipeline_layout_create_info.pNext = nullptr;
  pipeline_layout_create_info.flags = 0u;
  // このデスクリプタセットレイアウトのデスクリプタセットと組み合わせる
  pipeline_layout_create_info.setLayoutCount = 1u;
  pipeline_layout_create_info.pSetLayouts = &descriptor_set_layout_;
  // プッシュコンスタントは要らない
  pipeline_layout_create_info.pushConstantRangeCount = 0u;
  pipeline_layout_create_info.pPushConstantRanges = nullptr;
  VkPipelineLayout pipeline_layout;
  if( vkCreatePipelineLayout(
    device,
    &pipeline_layout_create_info,
    nullptr,
    &pipeline_layout
  ) != VK_SUCCESS ) abort();

  // シェーダの特殊化パラメータの配置
  std::vector< VkSpecializationMapEntry > specialization_map( 2u );
  specialization_map[ 0 ].constantID = 1u;
  specialization_map[ 0 ].offset = offsetof( spec_t, local_x_size );
  specialization_map[ 0 ].size = sizeof( std::uint32_t );
  specialization_map[ 1 ].constantID = 2u;
  specialization_map[ 1 ].offset = offsetof( spec_t, local_y_size );
  specialization_map[ 1 ].size = sizeof( std::uint32_t );
  
  // シェーダの特殊化パラメータの値
  const spec_t specialization_values{ 6, 1 };
 
  VkSpecializationInfo specialization_info;
  specialization_info.mapEntryCount = specialization_map.size();
  specialization_info.pMapEntries = specialization_map.data();
  specialization_info.dataSize = sizeof( spec_t );
  specialization_info.pData = &specialization_values;

  // パイプラインの設定
  VkComputePipelineCreateInfo pipeline_create_info;
  pipeline_create_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
  pipeline_create_info.pNext = nullptr;
  pipeline_create_info.flags = 0u;
  // シェーダの設定
  pipeline_create_info.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  pipeline_create_info.stage.pNext = nullptr;
  pipeline_create_info.stage.flags = 0u;
  // コンピュートシェーダに
  pipeline_create_info.stage.stage = VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT;
  // このシェーダを使う
  pipeline_create_info.stage.module = shader;
  // main関数から実行する
  pipeline_create_info.stage.pName = "main";
  // シェーダの特殊化パラメータを設定
  pipeline_create_info.stage.pSpecializationInfo = &specialization_info;
  // このレイアウトのパイプラインを作る
  pipeline_create_info.layout = pipeline_layout;
  pipeline_create_info.basePipelineHandle = VK_NULL_HANDLE;
  pipeline_create_info.basePipelineIndex = 0;

  // パイプラインを作る
  VkPipeline pipeline;
  if( vkCreateComputePipelines(
    device,
    VK_NULL_HANDLE,
    1u,
    &pipeline_create_info,
    nullptr,
    &pipeline
  ) != VK_SUCCESS ) abort();

  // パイプラインを捨てる
  vkDestroyPipeline(
    device,
    pipeline,
    nullptr
  );

  // パイプラインレイアウトを捨てる
  vkDestroyPipelineLayout(
    device,
    pipeline_layout,
    nullptr
  );
}

