#include <iostream>
#include <fstream>
#include <iterator>
#include <vector>
#include <nlohmann/json.hpp>
#include <gct/instance.hpp>
#include <gct/device.hpp>
#include <gct/allocator.hpp>
#include <gct/device_create_info.hpp>
#include <gct/spirv_reflect.h>
#include <gct/spv2json.hpp>
#include <vulkan/vulkan.hpp>

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
  const auto instance = **gct_instance;
  const auto physical_device = **gct_physical_device.devices[ 0 ];
  const auto device = **gct_device;
  
  // ファイルからSPIR-Vを読む
  std::fstream file( CMAKE_CURRENT_BINARY_DIR "/shader.comp.spv", std::ios::in|std::ios::binary );
  if( !file.good() ) std::abort();
  std::vector< std::uint8_t > code;
  code.assign(
    std::istreambuf_iterator< char >( file ),
    std::istreambuf_iterator< char >()
  );

  // シェーダモジュールを作る
  auto shader = device.createShaderModuleUnique(
    vk::ShaderModuleCreateInfo()
      .setCodeSize( code.size() )
      .setPCode( reinterpret_cast< const uint32_t* >( code.data() ) )
  );
  
  // シェーダの内容をパースする
  SpvReflectShaderModule reflect;
  if( spvReflectCreateShaderModule(
    code.size(),
    code.data(),
    &reflect
  ) != SPV_REFLECT_RESULT_SUCCESS ) std::abort();

  // パース結果をダンプ
  std::cout << nlohmann::json( reflect ).dump( 2 ) << std::endl;

  // パース結果を捨てる
  spvReflectDestroyShaderModule( &reflect ); 

}

