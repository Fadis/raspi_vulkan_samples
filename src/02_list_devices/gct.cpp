#include <iostream>
#include <nlohmann/json.hpp>
#include <gct/instance.hpp>
#include <gct/physical_device.hpp>
#include <vulkan2json/PhysicalDeviceProperties.hpp>
#include <vulkan2json/PhysicalDeviceFeatures.hpp>

int main( int argc, const char *argv[] ) {
  std::shared_ptr< gct::instance_t > instance(
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
  // デバイスの情報をダンプ
  for( auto &group: instance->get_physical_devices( {} ) ) {
    if( group.devices.size() == 1 )
      std::cout << nlohmann::json( group.devices[ 0 ]->get_props().get_basic() ).dump( 2 ) << std::endl;
      std::cout << nlohmann::json( group.devices[ 0 ]->get_features().get_basic() ).dump( 2 ) << std::endl;
  }
}


