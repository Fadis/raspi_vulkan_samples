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
  
  const char *display_envar = getenv( "DISPLAY" );
  if( display_envar == NULL || display_envar[0] == '\0') {
    std::cout << "環境変数DISPLAYがセットされていない為、Xサーバに接続できません" << std::endl;
    std::abort();
  }

  int scr;
  const auto connection = xcb_connect( NULL, &scr );
  const auto xcb_connection_error = xcb_connection_has_error( connection );
  if( xcb_connection_error > 0 ) {
    if( xcb_connection_error == XCB_CONN_ERROR )
    std::cout << "指定されたXサーバ " << display_envar << " と通信できません" << std::endl;
    std::abort();
  }
  const auto setup = xcb_get_setup( connection );
  auto iter = xcb_setup_roots_iterator( setup );
  while( scr-- > 0 ) xcb_screen_next( &iter );

  const auto screen = iter.data;

  const xcb_window_t xcb_window = xcb_generate_id( connection );

  std::uint32_t value_mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
  std::uint32_t value_list[32];
  value_list[0] = screen->black_pixel;
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

  /* Magic code that will send notification when window is destroyed */
  /*
  const auto cookie = xcb_intern_atom(
    connection,
    1,
    12,
    "WM_PROTOCOLS"
  );

  const auto reply = xcb_intern_atom_reply(
    connection,
    cookie,
    0
  );

  const auto cookie2 = xcb_intern_atom(
    connection,
    0,
    16,
    "WM_DELETE_WINDOW"
  );

  const auto atom_wm_delete_window = xcb_intern_atom_reply(
    connection,
    cookie2,
    0
  );

  xcb_change_property(
    connection,
    XCB_PROP_MODE_REPLACE,
    xcb_window,
    (*reply).atom,
    4,
    32,
    1,
    reinterpret_cast< void* >( (*atom_wm_delete_window).atom )
  );
  free( reply );
  */
  xcb_map_window(
    connection,
    xcb_window
  );

  // Force the x/y coordinates to 100,100 results are identical in consecutive
  // runs
  const uint32_t coords[] = {100, 100};
  xcb_configure_window(
    connection,
    xcb_window,
    XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y,
    coords
  );
  
  xcb_flush( connection );
  
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

  std::cout << "Surface Capabilities" << std::endl;
  std::cout << nlohmann::json( surface_capabilities ).dump( 2 ) << std::endl;

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

  vkDestroySurfaceKHR( instance, surface, nullptr );
  xcb_destroy_window( connection, xcb_window );
  xcb_disconnect( connection );
}

