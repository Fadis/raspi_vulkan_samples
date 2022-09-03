#include <fstream>
#include <iostream>
#include <optional>
#include <filesystem>
#include <gct/instance.hpp>
#include <gct/device.hpp>
#include <gct/allocator.hpp>
#include <gct/device_create_info.hpp>
#include <gct/descriptor_pool.hpp>
#include <gct/descriptor_set_layout.hpp>
#include <gct/write_descriptor_set.hpp>
#include <gct/shader_module.hpp>
#include <vulkan/vulkan.hpp>
#include <gct/mmaped_file.hpp>
#include <nlohmann/json.hpp>
#include <vulkan2json/PipelineCreationFeedbackEXT.hpp>

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
    )
  );
  auto groups = gct_instance->get_physical_devices( {} );
  auto gct_physical_device = groups[ 0 ].with_extensions( {
    VK_EXT_PIPELINE_CREATION_FEEDBACK_EXTENSION_NAME
  } );
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

  auto pipeline_layout = device.createPipelineLayoutUnique(
    vk::PipelineLayoutCreateInfo()
      .setSetLayoutCount( 1u )
      .setPSetLayouts( &descriptor_set_layout )
  );

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
  
  const spec_t specialization_values{ 6, 1 };
  
  const auto specialization_info = vk::SpecializationInfo()
    .setMapEntryCount( specialization_map.size() )
    .setPMapEntries( specialization_map.data() )
    .setDataSize( sizeof( spec_t ) )
    .setPData( &specialization_values );

  // パイプラインのコンパイルにかかった時間を計測する
  std::vector< vk::PipelineCreationFeedbackEXT > feedback_( 2u );
  const auto feedback =
     vk::PipelineCreationFeedbackCreateInfoEXT()
       .setPPipelineCreationFeedback( &feedback_.back() )
       .setPipelineStageCreationFeedbackCount( feedback_.size() - 1u )
       .setPPipelineStageCreationFeedbacks( feedback_.data() );

  const std::vector< vk::ComputePipelineCreateInfo > pipeline_create_info{
    vk::ComputePipelineCreateInfo()
      .setPNext( &feedback )
      .setStage(
        vk::PipelineShaderStageCreateInfo()
          .setStage( vk::ShaderStageFlagBits::eCompute )
          .setModule( shader )
          .setPName( "main" )
          .setPSpecializationInfo(
            &specialization_info
          )
      )
      .setLayout( *pipeline_layout )
  };

  // 既にパイプラインキャッシュを保存したファイルがあったら読む
  std::filesystem::path pipeline_cache_filename(
    CMAKE_CURRENT_BINARY_DIR "/pipeline_cache"
  );
  std::optional< gct::mmaped_file > pipeline_cache_data;
  if( std::filesystem::exists( pipeline_cache_filename ) ) {
    pipeline_cache_data = gct::mmaped_file( pipeline_cache_filename );
  }

  // パイプラインキャッシュを作る
  // 既にファイルがあった場合はその内容を使う
  const auto pipeline_cache = device.createPipelineCacheUnique(
    pipeline_cache_data ?
      vk::PipelineCacheCreateInfo()
        .setInitialDataSize( std::distance( pipeline_cache_data->begin(), pipeline_cache_data->end() ) )
        .setPInitialData( pipeline_cache_data->begin() ) :
      vk::PipelineCacheCreateInfo()
  );

  // パイプラインキャッシュを付けてパイプラインを作る
  auto wrapped = device.createComputePipelinesUnique(
    *pipeline_cache,
    pipeline_create_info
  );

  if( wrapped.result != vk::Result::eSuccess )
    vk::throwResultException( wrapped.result, "createComputePipeline failed" );
  auto pipeline = std::move( wrapped.value[ 0 ] );

  // パイプラインのコンパイルにかかった時間を表示
  std::cout << nlohmann::json( feedback_.back() ).dump( 2 ) << std::endl;

  // パイプラインキャッシュをファイルに保存する
  auto serialized = device.getPipelineCacheData( *pipeline_cache );
  std::filesystem::remove( pipeline_cache_filename );
  std::fstream pipeline_cache_output( pipeline_cache_filename.string(), std::ios_base::out );
  pipeline_cache_output.write( reinterpret_cast< const char* >( serialized.data() ), serialized.size() );
}

