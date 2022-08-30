#include <iostream>
#include <gct/instance.hpp>

int main( int argc, const char *argv[] ) {
  // インスタンスを作成
  std::shared_ptr< gct::instance_t > instance(
    new gct::instance_t(
      gct::instance_create_info_t()
        // アプリケーションの情報を指定
        .set_application_info(
          vk::ApplicationInfo()
            // アプリケーションの名前
            .setPApplicationName( argc ? argv[ 0 ] : "my_application" )
            // アプリケーションのバージョン
            .setApplicationVersion(  VK_MAKE_VERSION( 1, 0, 0 ) )
            // 使用するVulkanのバージョンをVulkan 1.2にする
            .setApiVersion( VK_API_VERSION_1_2 )
        )
        // バリデーションレイヤーを使う
        .add_layer(
          "VK_LAYER_KHRONOS_validation"
        )
    )
  );
  auto version = vk::enumerateInstanceVersion();
  std::cout <<
    VK_VERSION_MAJOR( version ) << "." <<
    VK_VERSION_MINOR( version ) << "." <<
    VK_VERSION_PATCH( version ) << std::endl; 
}

