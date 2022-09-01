#include <iostream>
#include <nlohmann/json.hpp>
#include <gct/get_extensions.hpp>
#include <gct/instance.hpp>
#include <gct/queue.hpp>
#include <gct/device.hpp>
#include <gct/allocator.hpp>
#include <gct/device_create_info.hpp>
#include <gct/image_create_info.hpp>
#include <gct/swapchain.hpp>
#include <gct/descriptor_pool.hpp>
#include <gct/descriptor_set_layout.hpp>
#include <gct/pipeline_cache.hpp>
#include <gct/pipeline_layout.hpp>
#include <gct/pipeline_layout_create_info.hpp>
#include <gct/buffer_view_create_info.hpp>
#include <gct/submit_info.hpp>
#include <gct/shader_module_create_info.hpp>
#include <gct/shader_module.hpp>
#include <gct/compute_pipeline_create_info.hpp>
#include <gct/compute_pipeline.hpp>
#include <gct/write_descriptor_set.hpp>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/version.h>

struct spec_t {
  std::uint32_t local_x_size = 0u;
  std::uint32_t local_y_size = 0u;
  float value = 0.f;
};

int main() {
  uint32_t iext_count = 0u;
  std::vector< const char* > iext{};
  const std::shared_ptr< gct::instance_t > gct_instance(
    new gct::instance_t(
      gct::instance_create_info_t()
        .set_application_info(
          vk::ApplicationInfo()
            .setPApplicationName( "my_application" )
            .setApplicationVersion(  VK_MAKE_VERSION( 1, 0, 0 ) )
            .setApiVersion( VK_MAKE_VERSION( 1, 2, 0 ) )
        )
        .add_layer(
          "VK_LAYER_KHRONOS_validation"
        )
        .add_extension(
          iext.begin(), iext.end()
        )
    )
  );
  auto groups = gct_instance->get_physical_devices( {} );
  auto gct_physical_device = groups[ 0 ].with_extensions( {
    VK_EXT_PIPELINE_CREATION_FEEDBACK_EXTENSION_NAME
  } );
 
  const auto gct_device = gct_physical_device.create_device(
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
  const auto gct_queue = gct_device->get_queue( 0u );

  const auto gct_descriptor_pool = gct_device->get_descriptor_pool(
    gct::descriptor_pool_create_info_t()
      .set_basic(
        vk::DescriptorPoolCreateInfo()
          .setFlags( vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet )
          .setMaxSets( 1 )
      )
      .set_descriptor_pool_size( vk::DescriptorType::eStorageImage, 2 )
      .rebuild_chain()
  );
  
  const auto gct_shader = gct_device->get_shader_module(
    CMAKE_CURRENT_BINARY_DIR "/shader.comp.spv"
  );
  
  const auto gct_descriptor_set_layout = gct_device->get_descriptor_set_layout(
    gct::descriptor_set_layout_create_info_t()
      .add_binding(
        gct_shader->get_props().get_reflection()
      )
      .rebuild_chain()
  );

  const auto gct_descriptor_set = gct_descriptor_pool->allocate( gct_descriptor_set_layout );

  const auto gct_pipeline_layout = gct_device->get_pipeline_layout(
    gct::pipeline_layout_create_info_t()
      .add_descriptor_set_layout( gct_descriptor_set_layout )
  );
  const auto gct_allocator = gct_device->get_allocator();
  const auto gct_pipeline_cache = gct_device->get_pipeline_cache();
  const auto gct_command_pool = gct_queue->get_command_pool();
  
  const auto instance = VkInstance( **gct_instance );
  const auto physical_device = VkPhysicalDevice( **gct_physical_device.devices[ 0 ] );
  const auto device = VkDevice( **gct_device );
  const auto queue = VkQueue( **gct_queue );
  const auto queue_family_index = gct_queue->get_available_queue_family_index();
  const auto descriptor_pool = VkDescriptorPool( **gct_descriptor_pool );
  const auto shader = VkShaderModule( **gct_shader );
  const auto descriptor_set_layout = VkDescriptorSetLayout( **gct_descriptor_set_layout );
  const auto descriptor_set = VkDescriptorSet( **gct_descriptor_set );
  const auto pipeline_cache = VkPipelineCache( **gct_pipeline_cache );
  const auto allocator = **gct_allocator;
  const auto pipeline_layout = VkPipelineLayout( **gct_pipeline_layout );
  const auto command_pool = VkCommandPool( **gct_command_pool );

  // パイプラインを作る
  const auto gct_pipeline = gct_pipeline_cache->get_pipeline(
    gct::compute_pipeline_create_info_t()
      .set_stage(
        gct::pipeline_shader_stage_create_info_t()
          .set_shader_module( gct_shader )
          .set_specialization_info(
            gct::specialization_info_t< spec_t >()
              .set_data(
                spec_t{ 16, 16, 1.0f }
              )
              .add_map< std::uint32_t >( 1, offsetof( spec_t, local_x_size ) )
              .add_map< std::uint32_t >( 2, offsetof( spec_t, local_y_size ) )
          )
      )
      .set_layout( gct_pipeline_layout )
  );
  const auto pipeline = VkPipeline( **gct_pipeline );

  // 入力用のバッファを作る
  const auto gct_src_buffer = gct_allocator->create_buffer(
    gct::buffer_create_info_t()
      .set_basic(
        vk::BufferCreateInfo()
	  // 1024x1024ピクセル並べられる大きさの
          .setSize( 1024u * 1024u * 4u )
	  // コピー元に使える
          .setUsage( vk::BufferUsageFlagBits::eTransferSrc )
      ),
    // CPUからGPUに送るのに適した
    VMA_MEMORY_USAGE_CPU_TO_GPU
  );
  const auto src_buffer = VkBuffer( **gct_src_buffer );

  // 入力用のイメージを作る
  auto gct_src_image = gct_allocator->create_image(
    gct::image_create_info_t()
      .set_basic(
        vk::ImageCreateInfo()
          .setImageType( vk::ImageType::e2D )
          .setFormat( vk::Format::eR8G8B8A8Srgb )
	  // 1024x1024の
          .setExtent( { 1024, 1024, 1 } )
          .setMipLevels( 1 )
          .setArrayLayers( 1 )
          .setSamples( vk::SampleCountFlagBits::e1 )
          .setTiling( vk::ImageTiling::eOptimal )
	  // コピー先とStorageImageに使える
          .setUsage(
            vk::ImageUsageFlagBits::eTransferDst |
            vk::ImageUsageFlagBits::eStorage
          )
      ),
      // GPUから触れれば良い
      VMA_MEMORY_USAGE_GPU_ONLY
  );
  const auto src_image = VkImage( **gct_src_image );

  // 出力用のバッファを作る
  const auto gct_dest_buffer = gct_allocator->create_buffer(
    gct::buffer_create_info_t()
      .set_basic(
        vk::BufferCreateInfo()
	  // 1024x1024ピクセル並べられる大きさの
          .setSize( 1024u * 1024u * 4u )
	  // コピー先に使える
          .setUsage( vk::BufferUsageFlagBits::eTransferDst )
      ),
    // GPUからCPUに送るのに適した
    VMA_MEMORY_USAGE_GPU_TO_CPU
  );
  const auto dest_buffer = VkBuffer( **gct_dest_buffer );

  // 出力用のイメージを作る
  auto gct_dest_image = gct_allocator->create_image(
    gct::image_create_info_t()
      .set_basic(
        vk::ImageCreateInfo()
          .setImageType( vk::ImageType::e2D )
          .setFormat( vk::Format::eR8G8B8A8Srgb )
	  // 1024x1024の
          .setExtent( { 1024, 1024, 1 } )
          .setMipLevels( 1 )
          .setArrayLayers( 1 )
          .setSamples( vk::SampleCountFlagBits::e1 )
          .setTiling( vk::ImageTiling::eOptimal )
	  // コピー元とStorageImageに使える
          .setUsage(
            vk::ImageUsageFlagBits::eTransferSrc |
            vk::ImageUsageFlagBits::eStorage
          )
      ),
      // GPUから触れれば良い
      VMA_MEMORY_USAGE_GPU_ONLY
  );
  const auto dest_image = VkImage( **gct_dest_image );

  {
    using namespace OIIO_NAMESPACE;
#if OIIO_VERSION_MAJOR >= 2 
    auto texture_file = ImageInput::open( CMAKE_CURRENT_SOURCE_DIR "/test.png" );
#else
    std::shared_ptr< ImageInput > texture_file(
      ImageInput::open( filename ),
      []( auto p ) { if( p ) ImageInput::destroy( p ); }
    );
#endif
    if( !texture_file ) throw -1;
    const ImageSpec &spec = texture_file->spec();
    {
      auto mapped = gct_src_buffer->map< std::uint8_t >();
      if( spec.nchannels == 3 ) {
        std::vector< uint8_t > temp( spec.width * spec.height * 4u );
        texture_file->read_image( TypeDesc::UINT8, temp.data() );
        for( size_t i = spec.width * spec.height - 1; i; --i ) {
          temp[ i * 4 ] = temp[ i * spec.nchannels ];
          temp[ i * 4 + 1 ] = temp[ i * spec.nchannels + 1 ];
          temp[ i * 4 + 2 ] = temp[ i * spec.nchannels + 2 ];
          temp[ i * 4 + 3 ] = 255u;
        }
        std::copy( temp.begin(), temp.end(), mapped.begin() );
      }
      else {
        texture_file->read_image( TypeDesc::UINT8, mapped.begin() );
      }
    }
  }

  {
    // コマンドバッファを確保する
    VkCommandBufferAllocateInfo command_buffer_allocate_info;
    command_buffer_allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    command_buffer_allocate_info.pNext = nullptr;
    // このコマンドプールから
    command_buffer_allocate_info.commandPool = command_pool;
    // 直接キューにsubmitする用のやつを
    command_buffer_allocate_info.level =
      VkCommandBufferLevel::VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    // 1個
    command_buffer_allocate_info.commandBufferCount = 1u;
    VkCommandBuffer command_buffer;
    if( vkAllocateCommandBuffers(
      device,
      &command_buffer_allocate_info,
      &command_buffer
    ) != VK_SUCCESS ) abort();
    // フェンスを作る
    VkFenceCreateInfo fence_create_info;
    fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_create_info.pNext = nullptr;
    fence_create_info.flags = 0u;
    VkFence fence;
    if( vkCreateFence(
      device,
      &fence_create_info,
      nullptr,
      &fence
    ) != VK_SUCCESS ) abort();
 
    // コマンドバッファにコマンドの記録を開始する
    VkCommandBufferBeginInfo command_buffer_begin_info;
    command_buffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    command_buffer_begin_info.pNext = nullptr;
    command_buffer_begin_info.flags = 0u;
    command_buffer_begin_info.pInheritanceInfo = nullptr;
    if( vkBeginCommandBuffer(
      command_buffer,
      &command_buffer_begin_info
    ) != VK_SUCCESS ) abort();

    {
      VkImageMemoryBarrier image_memory_barrier;
      image_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
      image_memory_barrier.pNext = nullptr;
      image_memory_barrier.srcAccessMask = VkAccessFlagBits::VK_ACCESS_TRANSFER_READ_BIT;
      image_memory_barrier.dstAccessMask = VkAccessFlagBits::VK_ACCESS_SHADER_READ_BIT;
      image_memory_barrier.oldLayout = VkImageLayout::VK_IMAGE_LAYOUT_UNDEFINED;
      image_memory_barrier.newLayout = VkImageLayout::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
      image_memory_barrier.srcQueueFamilyIndex = 0u;
      image_memory_barrier.dstQueueFamilyIndex = 0u;
      image_memory_barrier.image = src_image;
      image_memory_barrier.subresourceRange.aspectMask = VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT;
      image_memory_barrier.subresourceRange.baseMipLevel = 0u;
      image_memory_barrier.subresourceRange.levelCount = 1u;
      image_memory_barrier.subresourceRange.baseArrayLayer = 0u;
      image_memory_barrier.subresourceRange.layerCount = 1u;
      vkCmdPipelineBarrier(
        command_buffer,
        VkPipelineStageFlagBits::VK_PIPELINE_STAGE_TRANSFER_BIT,
        VkPipelineStageFlagBits::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VkDependencyFlagBits::VK_DEPENDENCY_BY_REGION_BIT,
        0u,
        nullptr,
        0u,
        nullptr,
        1u,
        &image_memory_barrier
      );
    }

    VkBufferImageCopy buffer_to_image;
    buffer_to_image.bufferOffset = 0u;
    buffer_to_image.bufferRowLength = 1024u;
    buffer_to_image.bufferImageHeight = 1024u;
    buffer_to_image.imageSubresource.aspectMask = VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT;
    buffer_to_image.imageSubresource.mipLevel = 0u;
    buffer_to_image.imageSubresource.baseArrayLayer = 0u;
    buffer_to_image.imageSubresource.layerCount = 1u;
    buffer_to_image.imageOffset.x = 0u;
    buffer_to_image.imageOffset.y = 0u;
    buffer_to_image.imageOffset.z = 0u;
    buffer_to_image.imageExtent.width = 1024u;
    buffer_to_image.imageExtent.height = 1024u;
    buffer_to_image.imageExtent.depth = 1u;
    vkCmdCopyBufferToImage(
      command_buffer,
      src_buffer,
      src_image,
      VkImageLayout::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      1u,
      &buffer_to_image
    );
 
    {
      std::vector< VkImageMemoryBarrier > image_memory_barrier( 2u );
      image_memory_barrier[ 0 ].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
      image_memory_barrier[ 0 ].pNext = nullptr;
      image_memory_barrier[ 0 ].srcAccessMask = VkAccessFlagBits::VK_ACCESS_TRANSFER_READ_BIT;
      image_memory_barrier[ 0 ].dstAccessMask = VkAccessFlagBits::VK_ACCESS_SHADER_READ_BIT;
      image_memory_barrier[ 0 ].oldLayout = VkImageLayout::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
      image_memory_barrier[ 0 ].newLayout = VkImageLayout::VK_IMAGE_LAYOUT_GENERAL;
      image_memory_barrier[ 0 ].srcQueueFamilyIndex = 0u;
      image_memory_barrier[ 0 ].dstQueueFamilyIndex = 0u;
      image_memory_barrier[ 0 ].image = src_image;
      image_memory_barrier[ 0 ].subresourceRange.aspectMask = VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT;
      image_memory_barrier[ 0 ].subresourceRange.baseMipLevel = 0u;
      image_memory_barrier[ 0 ].subresourceRange.levelCount = 1u;
      image_memory_barrier[ 0 ].subresourceRange.baseArrayLayer = 0u;
      image_memory_barrier[ 0 ].subresourceRange.layerCount = 1u;
      image_memory_barrier[ 1 ].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
      image_memory_barrier[ 1 ].pNext = nullptr;
      image_memory_barrier[ 1 ].srcAccessMask = VkAccessFlagBits::VK_ACCESS_TRANSFER_READ_BIT;
      image_memory_barrier[ 1 ].dstAccessMask = VkAccessFlagBits::VK_ACCESS_SHADER_READ_BIT;
      image_memory_barrier[ 1 ].oldLayout = VkImageLayout::VK_IMAGE_LAYOUT_UNDEFINED;
      image_memory_barrier[ 1 ].newLayout = VkImageLayout::VK_IMAGE_LAYOUT_GENERAL;
      image_memory_barrier[ 1 ].srcQueueFamilyIndex = 0u;
      image_memory_barrier[ 1 ].dstQueueFamilyIndex = 0u;
      image_memory_barrier[ 1 ].image = dest_image;
      image_memory_barrier[ 1 ].subresourceRange.aspectMask = VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT;
      image_memory_barrier[ 1 ].subresourceRange.baseMipLevel = 0u;
      image_memory_barrier[ 1 ].subresourceRange.levelCount = 1u;
      image_memory_barrier[ 1 ].subresourceRange.baseArrayLayer = 0u;
      image_memory_barrier[ 1 ].subresourceRange.layerCount = 1u;
      vkCmdPipelineBarrier(
        command_buffer,
        VkPipelineStageFlagBits::VK_PIPELINE_STAGE_TRANSFER_BIT,
        VkPipelineStageFlagBits::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VkDependencyFlagBits::VK_DEPENDENCY_BY_REGION_BIT,
        0u,
        nullptr,
        0u,
        nullptr,
        image_memory_barrier.size(),
        image_memory_barrier.data()
      );
    }
    
    // コマンドバッファにコマンドの記録を終了する
    if( vkEndCommandBuffer(
      command_buffer
    ) != VK_SUCCESS ) abort();
 
    // コマンドバッファの内容をキューに流す
    VkSubmitInfo submit_info;
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.pNext = nullptr;
    submit_info.waitSemaphoreCount = 0u;
    submit_info.pWaitSemaphores = nullptr;
    submit_info.pWaitDstStageMask = nullptr;
    submit_info.commandBufferCount = 1u;
    // このコマンドバッファの内容を流す
    submit_info.pCommandBuffers = &command_buffer;
    submit_info.signalSemaphoreCount = 0u;
    submit_info.pSignalSemaphores = nullptr;
    if( vkQueueSubmit(
      queue,
      1u,
      &submit_info,
      // 実行し終わったらこのフェンスに通知
      fence
    ) != VK_SUCCESS ) abort();
 
    // フェンスが完了通知を受けるのを待つ
    if( vkWaitForFences(
      device,
      1u,
      // このフェンスを待つ
      &fence,
      // 全部のフェンスに完了通知が来るまで待つ
      true,
      // 1秒でタイムアウト
      1 * 1000 * 1000 * 1000
    ) != VK_SUCCESS ) abort();

    // フェンスを捨てる
    vkDestroyFence(
      device,
      fence,
      nullptr
    );

    // コマンドバッファを捨てる
    vkFreeCommandBuffers(
      device,
      command_pool,
      1u,
      &command_buffer
    );
  }

  VkImageViewCreateInfo src_image_view_create_info;
  src_image_view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  src_image_view_create_info.pNext = nullptr;
  src_image_view_create_info.flags = 0;
  src_image_view_create_info.image = src_image;
  src_image_view_create_info.viewType = VkImageViewType::VK_IMAGE_VIEW_TYPE_2D;
  src_image_view_create_info.format = VK_FORMAT_R8G8B8A8_SRGB;
  src_image_view_create_info.components.r = VkComponentSwizzle::VK_COMPONENT_SWIZZLE_R;
  src_image_view_create_info.components.g = VkComponentSwizzle::VK_COMPONENT_SWIZZLE_G;
  src_image_view_create_info.components.b = VkComponentSwizzle::VK_COMPONENT_SWIZZLE_B;
  src_image_view_create_info.components.a = VkComponentSwizzle::VK_COMPONENT_SWIZZLE_A;
  src_image_view_create_info.subresourceRange.aspectMask = VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT;
  src_image_view_create_info.subresourceRange.baseMipLevel = 0u;
  src_image_view_create_info.subresourceRange.levelCount = 1u;
  src_image_view_create_info.subresourceRange.baseArrayLayer = 0u;
  src_image_view_create_info.subresourceRange.layerCount = 1u;
  VkImageView src_image_view;
  if( vkCreateImageView(
    device,
    &src_image_view_create_info,
    nullptr,
    &src_image_view
  ) != VK_SUCCESS ) std::abort();

  VkImageViewCreateInfo dest_image_view_create_info;
  dest_image_view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  dest_image_view_create_info.pNext = nullptr;
  dest_image_view_create_info.flags = 0;
  dest_image_view_create_info.image = dest_image;
  dest_image_view_create_info.viewType = VkImageViewType::VK_IMAGE_VIEW_TYPE_2D;
  dest_image_view_create_info.format = VK_FORMAT_R8G8B8A8_SRGB;
  dest_image_view_create_info.components.r = VkComponentSwizzle::VK_COMPONENT_SWIZZLE_R;
  dest_image_view_create_info.components.g = VkComponentSwizzle::VK_COMPONENT_SWIZZLE_G;
  dest_image_view_create_info.components.b = VkComponentSwizzle::VK_COMPONENT_SWIZZLE_B;
  dest_image_view_create_info.components.a = VkComponentSwizzle::VK_COMPONENT_SWIZZLE_A;
  dest_image_view_create_info.subresourceRange.aspectMask = VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT;
  dest_image_view_create_info.subresourceRange.baseMipLevel = 0u;
  dest_image_view_create_info.subresourceRange.levelCount = 1u;
  dest_image_view_create_info.subresourceRange.baseArrayLayer = 0u;
  dest_image_view_create_info.subresourceRange.layerCount = 1u;
  VkImageView dest_image_view;
  if( vkCreateImageView(
    device,
    &dest_image_view_create_info,
    nullptr,
    &dest_image_view
  ) != VK_SUCCESS ) std::abort();

  VkDescriptorImageInfo descriptor_src_image_info;
  descriptor_src_image_info.sampler = VK_NULL_HANDLE;
  descriptor_src_image_info.imageView = src_image_view;
  descriptor_src_image_info.imageLayout = VkImageLayout::VK_IMAGE_LAYOUT_GENERAL;
  
  VkDescriptorImageInfo descriptor_dest_image_info;
  descriptor_dest_image_info.sampler = VK_NULL_HANDLE;
  descriptor_dest_image_info.imageView = dest_image_view;
  descriptor_dest_image_info.imageLayout = VkImageLayout::VK_IMAGE_LAYOUT_GENERAL;

  // デスクリプタの内容を更新
  std::vector< VkWriteDescriptorSet > write_descriptor_set( 2u );
  write_descriptor_set[ 0 ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  write_descriptor_set[ 0 ].pNext = nullptr;
  // このデスクリプタセットの
  write_descriptor_set[ 0 ].dstSet = descriptor_set;
  // binding=0の
  write_descriptor_set[ 0 ].dstBinding = 0u;
  // 0要素目から[
  write_descriptor_set[ 0 ].dstArrayElement = 0u;
  // 1個の
  write_descriptor_set[ 0 ].descriptorCount = 1u;
  // ストレージバッファのデスクリプタを
  write_descriptor_set[ 0 ].descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
  // この内容にする
  write_descriptor_set[ 0 ].pImageInfo = &descriptor_src_image_info;
  write_descriptor_set[ 0 ].pBufferInfo = nullptr;
  write_descriptor_set[ 0 ].pTexelBufferView = nullptr;
  write_descriptor_set[ 1 ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  write_descriptor_set[ 1 ].pNext = nullptr;
  // このデスクリプタセットの
  write_descriptor_set[ 1 ].dstSet = descriptor_set;
  // binding=1の
  write_descriptor_set[ 1 ].dstBinding = 1u;
  // 0要素目から
  write_descriptor_set[ 1 ].dstArrayElement = 0u;
  // 1個の
  write_descriptor_set[ 1 ].descriptorCount = 1u;
  // ストレージバッファのデスクリプタを
  write_descriptor_set[ 1 ].descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
  // この内容にする
  write_descriptor_set[ 1 ].pImageInfo = &descriptor_dest_image_info;
  write_descriptor_set[ 1 ].pBufferInfo = nullptr;
  write_descriptor_set[ 1 ].pTexelBufferView = nullptr;
  vkUpdateDescriptorSets(
    device,
    write_descriptor_set.size(),
    write_descriptor_set.data(),
    0u,
    nullptr
  );

  {
    // コマンドバッファを確保する
    VkCommandBufferAllocateInfo command_buffer_allocate_info;
    command_buffer_allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    command_buffer_allocate_info.pNext = nullptr;
    // このコマンドプールから
    command_buffer_allocate_info.commandPool = command_pool;
    // 直接キューにsubmitする用のやつを
    command_buffer_allocate_info.level =
      VkCommandBufferLevel::VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    // 1個
    command_buffer_allocate_info.commandBufferCount = 1u;
    VkCommandBuffer command_buffer;
    if( vkAllocateCommandBuffers(
      device,
      &command_buffer_allocate_info,
      &command_buffer
    ) != VK_SUCCESS ) abort();
    // フェンスを作る
    VkFenceCreateInfo fence_create_info;
    fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_create_info.pNext = nullptr;
    fence_create_info.flags = 0u;
    VkFence fence;
    if( vkCreateFence(
      device,
      &fence_create_info,
      nullptr,
      &fence
    ) != VK_SUCCESS ) abort();

    // コマンドバッファにコマンドの記録を開始する
    VkCommandBufferBeginInfo command_buffer_begin_info;
    command_buffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    command_buffer_begin_info.pNext = nullptr;
    command_buffer_begin_info.flags = 0u;
    command_buffer_begin_info.pInheritanceInfo = nullptr;
    if( vkBeginCommandBuffer(
      command_buffer,
      &command_buffer_begin_info
    ) != VK_SUCCESS ) abort();
 
    // 以降のパイプラインの実行ではこのデスクリプタセットを使う
    VkDescriptorSet raw_descriptor_set = descriptor_set;
    vkCmdBindDescriptorSets(
      command_buffer,
      // コンピュートパイプラインの実行に使うデスクリプタセットを
      VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_COMPUTE,
      pipeline_layout,
      0u,
      1u,
      // これにする
      &raw_descriptor_set,
      0u,
      nullptr
    );
 
    // 以降のパイプラインの実行ではこのパイプラインを使う
    vkCmdBindPipeline(
      command_buffer,
      // コンピュートパイプラインを
      VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_COMPUTE,
      // これにする
      pipeline
    );
 
    // コンピュートパイプラインを実行する
    vkCmdDispatch(
      command_buffer,
      64, 64, 1
    );

    {
      VkImageMemoryBarrier image_memory_barrier;
      image_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
      image_memory_barrier.pNext = nullptr;
      image_memory_barrier.srcAccessMask = VkAccessFlagBits::VK_ACCESS_SHADER_WRITE_BIT;
      image_memory_barrier.dstAccessMask = VkAccessFlagBits::VK_ACCESS_TRANSFER_READ_BIT;
      image_memory_barrier.oldLayout = VkImageLayout::VK_IMAGE_LAYOUT_GENERAL;
      image_memory_barrier.newLayout = VkImageLayout::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
      image_memory_barrier.srcQueueFamilyIndex = 0u;
      image_memory_barrier.dstQueueFamilyIndex = 0u;
      image_memory_barrier.image = dest_image;
      image_memory_barrier.subresourceRange.aspectMask = VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT;
      image_memory_barrier.subresourceRange.baseMipLevel = 0u;
      image_memory_barrier.subresourceRange.levelCount = 1u;
      image_memory_barrier.subresourceRange.baseArrayLayer = 0u;
      image_memory_barrier.subresourceRange.layerCount = 1u;
      vkCmdPipelineBarrier(
        command_buffer,
        VkPipelineStageFlagBits::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VkPipelineStageFlagBits::VK_PIPELINE_STAGE_TRANSFER_BIT,
        VkDependencyFlagBits::VK_DEPENDENCY_BY_REGION_BIT,
        0u,
        nullptr,
        0u,
        nullptr,
        1u,
        &image_memory_barrier
      );
    }


    VkBufferImageCopy image_to_buffer;
    image_to_buffer.bufferOffset = 0u;
    image_to_buffer.bufferRowLength = 1024u;
    image_to_buffer.bufferImageHeight = 1024u;
    image_to_buffer.imageSubresource.aspectMask = VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT;
    image_to_buffer.imageSubresource.mipLevel = 0u;
    image_to_buffer.imageSubresource.baseArrayLayer = 0u;
    image_to_buffer.imageSubresource.layerCount = 1u;
    image_to_buffer.imageOffset.x = 0u;
    image_to_buffer.imageOffset.y = 0u;
    image_to_buffer.imageOffset.z = 0u;
    image_to_buffer.imageExtent.width = 1024u;
    image_to_buffer.imageExtent.height = 1024u;
    image_to_buffer.imageExtent.depth = 1u;
    vkCmdCopyImageToBuffer(
      command_buffer,
      dest_image,
      VkImageLayout::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
      dest_buffer,
      1u,
      &image_to_buffer
    );

    // コマンドバッファにコマンドの記録を終了する
    if( vkEndCommandBuffer(
      command_buffer
    ) != VK_SUCCESS ) abort();

    // コマンドバッファの内容をキューに流す
    VkSubmitInfo submit_info;
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.pNext = nullptr;
    submit_info.waitSemaphoreCount = 0u;
    submit_info.pWaitSemaphores = nullptr;
    submit_info.pWaitDstStageMask = nullptr;
    submit_info.commandBufferCount = 1u;
    // このコマンドバッファの内容を流す
    submit_info.pCommandBuffers = &command_buffer;
    submit_info.signalSemaphoreCount = 0u;
    submit_info.pSignalSemaphores = nullptr;
    if( vkQueueSubmit(
      queue,
      1u,
      &submit_info,
      // 実行し終わったらこのフェンスに通知
      fence
    ) != VK_SUCCESS ) abort();
 
    // フェンスが完了通知を受けるのを待つ
    if( vkWaitForFences(
      device,
      1u,
      // このフェンスを待つ
      &fence,
      // 全部のフェンスに完了通知が来るまで待つ
      true,
      // 1秒でタイムアウト
      1 * 1000 * 1000 * 1000
    ) != VK_SUCCESS ) abort();

    // フェンスを捨てる
    vkDestroyFence(
      device,
      fence,
      nullptr
    );

    // コマンドバッファを捨てる
    vkFreeCommandBuffers(
      device,
      command_pool,
      1u,
      &command_buffer
    );

  }
  
  vkDestroyImageView(
    device,
    src_image_view,
    nullptr
  );
  
  vkDestroyImageView(
    device,
    dest_image_view,
    nullptr
  );


  {
    using namespace OIIO_NAMESPACE;
    ImageSpec spec( 1024, 1024, 4, TypeDesc::UINT8 );
    auto out = ImageOutput::create( "out.png" );
    out->open( "out.png", spec );
    {
      auto mapped = gct_dest_buffer->map< std::uint8_t >();
      out->write_image( TypeDesc::UINT8, mapped.begin() );
    }
    out->close();
  }

}

