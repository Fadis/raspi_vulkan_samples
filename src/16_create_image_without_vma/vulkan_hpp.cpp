#include <iostream>
#include <nlohmann/json.hpp>
#include <gct/get_extensions.hpp>
#include <gct/instance.hpp>
#include <gct/queue.hpp>
#include <gct/device.hpp>
#include <gct/allocator.hpp>
#include <gct/device_create_info.hpp>
#include <vulkan/vulkan.hpp>
#include <vulkan2json/MemoryRequirements.hpp>


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
  const auto memory_props = physical_device.getMemoryProperties();

  // イメージを作る
  const auto image = device.createImageUnique(
    vk::ImageCreateInfo()
      // 2次元で
      .setImageType( vk::ImageType::e2D )
      // RGBA各8bitで
      .setFormat( vk::Format::eR8G8B8A8Unorm )
      .setExtent( { 1024, 1024, 1 } )
      // ミップマップは無く
      .setMipLevels( 1 )
      // レイヤーは1枚だけの
      .setArrayLayers( 1 )
      // 1テクセルにつきサンプリング点を1つだけ持つ
      .setSamples( vk::SampleCountFlagBits::e1 )
      // GPUが読みやすいように配置された
      .setTiling( vk::ImageTiling::eOptimal )
      // 転送先とストレージイメージに使う
      .setUsage(
        vk::ImageUsageFlagBits::eTransferDst |
        vk::ImageUsageFlagBits::eStorage
      )
      // 同時に複数のキューから操作しない
      .setSharingMode( vk::SharingMode::eExclusive )
      .setQueueFamilyIndexCount( 0 )
      .setPQueueFamilyIndices( nullptr )
      // 初期状態は不定な
      .setInitialLayout( vk::ImageLayout::eUndefined )
  );
  
  // イメージに必要なメモリの要件を調べる
  const auto image_memory_reqs = device.getImageMemoryRequirements( *image );

  std::cout << nlohmann::json( image_memory_reqs ).dump( 2 ) << std::endl;

  // 利用可能なメモリタイプの中からStorageImageに使えてGPUから触るのに適した物を選ぶ
  // VideoCore VIにはメモリタイプが1つしか無いのでこれは必ず0になる
  std::uint32_t memory_index = 0u;
  for( std::uint32_t i = 0u; i != memory_props.memoryTypeCount; ++i ) {
    if( memory_props.memoryTypes[ i ].propertyFlags & vk::MemoryPropertyFlagBits::eDeviceLocal ) {
      if( ( image_memory_reqs.memoryTypeBits >> i ) & 0x1 ) {
        memory_index = i;
        break;
      }
    }
  }

  std::cout << memory_index << "番目のメモリタイプが選ばれました" << std::endl;
  
  // イメージ用のメモリを確保する
  const auto device_memory = device.allocateMemoryUnique(
    vk::MemoryAllocateInfo()
      .setAllocationSize( image_memory_reqs.size )
      .setMemoryTypeIndex( memory_index )
  );

  // メモリをイメージに結びつける
  device.bindImageMemory(
    *image,
    *device_memory,
    0u
  );
}

