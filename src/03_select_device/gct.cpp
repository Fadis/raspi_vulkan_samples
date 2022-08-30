#include <iostream>
#include <gct/instance.hpp>
#include <gct/physical_device.hpp>
#include <gct/device.hpp>
#include <gct/device_create_info.hpp>

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
        .add_layer(
          "VK_LAYER_KHRONOS_validation"
        )
    )
  );
  auto groups = instance->get_physical_devices( {} );
  auto physical_device = groups[ 0 ].with_extensions( {} );

  const auto device =
    // 最初に見つかった物理デバイスを使う
    physical_device
      // 論理デバイスを作る
      .create_device(
        std::vector< gct::queue_requirement_t >{
          // 計算要求を受け付けるキューを1つください
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

  // デバイスからキューを取得
  const auto queue = device->get_queue( 0u );
}


