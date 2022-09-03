#include <cmath>
#include <iostream>
#include <chrono>
#include <thread>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <gct/get_extensions.hpp>
#include <gct/instance.hpp>
#include <gct/queue.hpp>
#include <gct/device.hpp>
#include <gct/device_create_info.hpp>
#include <nlohmann/json.hpp>
#include <vulkan2json/SurfaceCapabilitiesKHR.hpp>

int main( int argc, const char *argv[] ) {

  // glfwを初期化する
  if( !glfwInit() ) {
    const char *p = nullptr;
    glfwGetError( &p );
    std::cerr << p << std::endl;
    std::abort();
  }

  // このプラットフォームでサーフェスを使う為に必要な拡張を取得する
  std::uint32_t required_extension_count = 0u;
  const char **required_extensions_begin = glfwGetRequiredInstanceExtensions( &required_extension_count );
  const auto required_extensions_end = std::next( required_extensions_begin, required_extension_count );
  std::for_each(
    required_extensions_begin,
    required_extensions_end,
    []( const char *v ) {
      std::cout << "このプラットフォームでサーフェスを作るには " << v << " 拡張が必要です" << std::endl;
    }
  );

  const std::shared_ptr< gct::instance_t > gct_instance(
    new gct::instance_t(
      gct::instance_create_info_t()
        .set_application_info(
          vk::ApplicationInfo()
            .setPApplicationName( argc ? argv[ 0 ] : "my_application" )
            .setApplicationVersion(  VK_MAKE_VERSION( 1, 0, 0 ) )
            .setApiVersion( VK_API_VERSION_1_2 )
        )
	.add_extension(
          required_extensions_begin,
	  required_extensions_end
	)
        .add_layer(
          "VK_LAYER_KHRONOS_validation"
        )
    )
  );
  auto groups = gct_instance->get_physical_devices( {} );
  auto gct_physical_device = groups[ 0 ].with_extensions( {
    VK_EXT_PIPELINE_CREATION_FEEDBACK_EXTENSION_NAME,
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
  } );
  const auto instance = VkInstance( **gct_instance );
  const auto physical_device = VkPhysicalDevice( **gct_physical_device.devices[ 0 ] );

  std::uint32_t width = 1024; 
  std::uint32_t height = 1024; 

  // OpenGLの用意はしないよう指示する
  glfwWindowHint( GLFW_CLIENT_API, GLFW_NO_API );
  // リサイズできないウィンドウを作るよう指示する
  glfwWindowHint( GLFW_RESIZABLE, GLFW_FALSE );
  //ウィンドウを作る
  auto window = glfwCreateWindow(
    width,
    height,
    argc ? argv[ 0 ] : "my_application",
    nullptr,
    nullptr
  );

  // 指定したサイズでウィンドウを作れない事があるので、実際にできたウィンドウのサイズを取得する
  int width_s;
  int height_s;
  glfwGetWindowSize( window, &width_s, &height_s );
  width = width_s;
  height = height_s;

  // サーフェスを作る
  VkSurfaceKHR surface;
  if( glfwCreateWindowSurface(
    instance,
    window,
    nullptr,
    &surface
  ) != VK_SUCCESS ) std::abort();
 
  // 以降のサーフェスの使い方はlibxcbを使った場合と同じ
  VkBool32 supported = VK_FALSE;
  if( vkGetPhysicalDeviceSurfaceSupportKHR(
    physical_device,
    0,
    surface,
    &supported
  ) != VK_SUCCESS ) std::abort();

  if( supported ) {
    std::cout << "作成したサーフェスはこのGPUで計算した結果を表示できます" << std::endl;
  }
  else {
    std::cout << "作成したサーフェスはこのGPUで計算した結果を表示できません" << std::endl;
    std::abort();
  }

  VkSurfaceCapabilitiesKHR surface_capabilities;
  std::memset( &surface_capabilities, sizeof( surface_capabilities ), 0 );
  if( vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
    physical_device,
    surface,
    &surface_capabilities
  ) != VK_SUCCESS ) std::abort();

  std::cout << nlohmann::json( surface_capabilities ).dump( 2 ) << std::endl;

  bool close_app = false;

  glfwSetWindowUserPointer( window, reinterpret_cast< void* >( &close_app ) );
  glfwSetKeyCallback(
    window,
    []( GLFWwindow *window, int key, int scancode, int action, int mods ) {
      if( action == GLFW_RELEASE ) {
        if( key == GLFW_KEY_Q ) {
          auto &close_app = *reinterpret_cast< bool* >( glfwGetWindowUserPointer( window ) );
          close_app = true;
	}
      }
    }
  );

  while( !close_app ) {
    const auto begin_time = std::chrono::high_resolution_clock::now();
    glfwPollEvents();
    const auto end_time = std::chrono::high_resolution_clock::now();
    const auto elapsed_time = end_time - begin_time;
    if( elapsed_time < std::chrono::microseconds( 16667 ) ) {
      const auto sleep_for = std::chrono::microseconds( 16667 ) - elapsed_time;
      std::this_thread::sleep_for( sleep_for );
    }
  }

  // サーフェスを捨てる
  vkDestroySurfaceKHR( instance, surface, nullptr );

  // ウィンドウを捨てる
  glfwDestroyWindow( window );
}

