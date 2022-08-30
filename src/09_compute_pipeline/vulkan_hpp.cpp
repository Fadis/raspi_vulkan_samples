#include <iostream>
#include <gct/instance.hpp>
#include <gct/device.hpp>
#include <gct/allocator.hpp>
#include <gct/device_create_info.hpp>
#include <gct/descriptor_pool.hpp>
#include <gct/descriptor_set_layout.hpp>
#include <gct/write_descriptor_set.hpp>
#include <gct/shader_module.hpp>
#include <vulkan/vulkan.hpp>


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
  const auto instance = **gct_instance;
  const auto physical_device = **gct_physical_device.devices[ 0 ];
  const auto device = **gct_device;
  const auto shader = **gct_shader;
  const auto descriptor_set_layout = **gct_descriptor_set_layout;

  // パイプラインレイアウトを作る
  auto pipeline_layout = device.createPipelineLayoutUnique(
    vk::PipelineLayoutCreateInfo()
      .setSetLayoutCount( 1u )
      // このレイアウトのデスクリプタセットとくっつく
      .setPSetLayouts( &descriptor_set_layout )
  );

  // シェーダの特殊化パラメータの配置
  const std::vector< vk::SpecializationMapEntry > specialization_map{
    vk::SpecializationMapEntry()
      .setConstantID( 1 )
      .setOffset( offsetof( spec_t, local_x_size ) )
      .setSize( sizeof( std::uint32_t ) ),
    vk::SpecializationMapEntry()
      .setConstantID( 2 )
      .setOffset( offsetof( spec_t, local_y_size ) )
      .setSize( sizeof( std::uint32_t ) )
  };
  
  // シェーダの特殊化パラメータの値
  const spec_t specialization_values{ 6, 1 };
  
  const auto specialization_info = vk::SpecializationInfo()
    .setMapEntryCount( specialization_map.size() )
    .setPMapEntries( specialization_map.data() )
    .setDataSize( sizeof( spec_t ) )
    .setPData( &specialization_values );

  // パイプラインの設定
  const std::vector< vk::ComputePipelineCreateInfo > pipeline_create_info{
    vk::ComputePipelineCreateInfo()
    // シェーダの設定
    .setStage(
      vk::PipelineShaderStageCreateInfo()
        // コンピュートシェーダに
        .setStage( vk::ShaderStageFlagBits::eCompute )
        // このシェーダを使う
        .setModule( shader )
        // main関数から実行する
        .setPName( "main" )
        // シェーダの特殊化パラメータを設定
        .setPSpecializationInfo(
          &specialization_info
        )
    )
    // このレイアウトのパイプラインを作る
    .setLayout( *pipeline_layout )
  };

  // パイプラインを作る
  auto wrapped = device.createComputePipelinesUnique(
    // このパイプラインキャッシュを使って
    VK_NULL_HANDLE,
    // この設定で
    pipeline_create_info
  );

  // パイプラインは一度に複数作れるのでvectorで返ってくる
  // 1つめの設定のパイプラインは返り値の1要素目に入っている
  if( wrapped.result != vk::Result::eSuccess )
    vk::throwResultException( wrapped.result, "createComputePipeline failed" );
  auto pipeline = std::move( wrapped.value[ 0 ] );

}

