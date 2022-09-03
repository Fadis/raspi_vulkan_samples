#include <cmath>
#include <iostream>
#include <chrono>
#include <thread>
#define VK_USE_PLATFORM_XCB_KHR
#include <vulkan/vulkan.h>
#include <gct/get_extensions.hpp>
#include <gct/instance.hpp>
#include <gct/queue.hpp>
#include <gct/device.hpp>
#include <gct/device_create_info.hpp>
#include <X11/Xutil.h>
#include <nlohmann/json.hpp>
#include <vulkan2json/SurfaceCapabilitiesKHR.hpp>
#include <vulkan2json/SurfaceFormatKHR.hpp>

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
	.add_extension(
          VK_KHR_SURFACE_EXTENSION_NAME
	)
	.add_extension(
          VK_KHR_XCB_SURFACE_EXTENSION_NAME
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

  const std::uint32_t width = 1024; 
  const std::uint32_t height = 1024; 
  Display *display;

  // Xサーバのアドレスが環境変数にセットされていなかったら諦める  
  const char *display_envar = getenv( "DISPLAY" );
  if( display_envar == NULL || display_envar[0] == '\0') {
    std::cout << "環境変数DISPLAYがセットされていない為、Xサーバに接続できません" << std::endl;
    std::abort();
  }

  // Xサーバに接続する
  int scr;
  const auto connection = xcb_connect( NULL, &scr );
  const auto xcb_connection_error = xcb_connection_has_error( connection );
  if( xcb_connection_error > 0 ) {
    if( xcb_connection_error == XCB_CONN_ERROR )
    std::cout << "指定されたXサーバ " << display_envar << " と通信できません" << std::endl;
    std::abort();
  }

  // 画面を選ぶ(デュアルディスプレイだと複薄あるかもしれない)
  const auto setup = xcb_get_setup( connection );
  auto iter = xcb_setup_roots_iterator( setup );
  while( scr-- > 0 ) xcb_screen_next( &iter );

  const auto screen = iter.data;

  // ウィンドウのIDを作る
  const xcb_window_t xcb_window = xcb_generate_id( connection );

  // ウィンドウを作る
  std::uint32_t value_mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
  std::uint32_t value_list[32];
  value_list[0] = screen->black_pixel;
  // キー入力のイベントを受け取るようにする
  value_list[1] = XCB_EVENT_MASK_KEY_RELEASE | XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_STRUCTURE_NOTIFY;
  xcb_create_window(
    connection,
    XCB_COPY_FROM_PARENT,
    xcb_window,
    screen->root,
    0,
    0,
    width,
    height,
    0,
    XCB_WINDOW_CLASS_INPUT_OUTPUT,
    screen->root_visual,
    value_mask,
    value_list
  );
  xcb_map_window(
    connection,
    xcb_window
  );

  // ウィンドウを適当な位置に移動させる
  const uint32_t coords[] = {100, 100};
  xcb_configure_window(
    connection,
    xcb_window,
    XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y,
    coords
  );
  
  // ここまでの内容をXサーバに反映させる
  xcb_flush( connection );
  
  // libxcbのウィンドウからサーフェスを作る
  VkXcbSurfaceCreateInfoKHR surface_create_info;
  surface_create_info.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
  surface_create_info.pNext = NULL;
  surface_create_info.flags = 0;
  surface_create_info.connection = connection;
  surface_create_info.window = xcb_window;
  VkSurfaceKHR surface;
  if( vkCreateXcbSurfaceKHR(
    instance,
    &surface_create_info,
    nullptr,
    &surface
  ) != VK_SUCCESS ) std::abort();

  // この物理デバイスからサーフェスに描けるかどうか調べる
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

  // この物理デバイスからサーフェスに描く為にサーフェスに送るイメージの形式を調べる
  VkSurfaceCapabilitiesKHR surface_capabilities;
  std::memset( &surface_capabilities, sizeof( surface_capabilities ), 0 );
  if( vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
    physical_device,
    surface,
    &surface_capabilities
  ) != VK_SUCCESS ) std::abort();

  std::cout << "Surface Capabilities" << std::endl;
  std::cout << nlohmann::json( surface_capabilities ).dump( 2 ) << std::endl;

  // この物理デバイスからサーフェスに描く場合に使えるピクセルのフォーマットを調べる
  std::uint32_t available_format_count = 0u;
  if( vkGetPhysicalDeviceSurfaceFormatsKHR(
    physical_device,
    surface,
    &available_format_count,
    nullptr
  ) != VK_SUCCESS ) std::abort();
  std::vector< VkSurfaceFormatKHR > available_formats( available_format_count );
  if( vkGetPhysicalDeviceSurfaceFormatsKHR(
    physical_device,
    surface,
    &available_format_count,
    available_formats.data()
  ) != VK_SUCCESS ) std::abort();
  
  std::cout << "Surface Formats" << std::endl;
  std::cout << nlohmann::json( available_formats ).dump( 2 ) << std::endl;

  // 1/60秒毎にXサーバからイベントを受け取り、キーボードのqが押されていたら終了させる
  bool close_app = false;
  while( !close_app ) {
    const auto begin_time = std::chrono::high_resolution_clock::now();
    xcb_generic_event_t *event;
    event = xcb_poll_for_event( connection );
    while( event ) {
      const std::uint8_t event_code = event->response_type & 0x7f;
      if( event_code == XCB_KEY_RELEASE ) {
        const xcb_key_release_event_t *key = reinterpret_cast<const xcb_key_release_event_t *>( event );
	if( key->detail == 24 ) close_app = true;
      }
      free( event );
      event = xcb_poll_for_event( connection );
    }
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
  xcb_destroy_window( connection, xcb_window );

  // Xサーバとの接続を切る
  xcb_disconnect( connection );
}

