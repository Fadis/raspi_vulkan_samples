#include <cmath>
#include <iostream>
#include <nlohmann/json.hpp>
#include <glm/mat2x2.hpp>
#include <gct/get_extensions.hpp>
#include <gct/instance.hpp>
#include <gct/queue.hpp>
#include <gct/device.hpp>
#include <gct/allocator.hpp>
#include <gct/device_create_info.hpp>
#include <gct/image_create_info.hpp>
#include <gct/physical_device.hpp>
#include <gct/descriptor_set_layout.hpp>
#include <gct/shader_module_create_info.hpp>
#include <gct/shader_module.hpp>
#include <gct/render_pass_create_info.hpp>
#include <gct/render_pass.hpp>
#include <gct/framebuffer.hpp>

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
        .add_layer(
          "VK_LAYER_KHRONOS_validation"
        )
    )
  );
  auto groups = gct_instance->get_physical_devices( {} );
  auto gct_physical_device = groups[ 0 ].with_extensions( {
    VK_EXT_PIPELINE_CREATION_FEEDBACK_EXTENSION_NAME
  } );
  
  std::uint32_t width = 1024u;
  std::uint32_t height = 1024u;
 
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
  
  const auto gct_render_pass = gct_device->get_render_pass(
    gct::render_pass_create_info_t()
      .add_attachment(
        vk::AttachmentDescription()
          .setFormat( vk::Format::eR8G8B8A8Unorm )
          .setSamples( vk::SampleCountFlagBits::e1 )
          .setLoadOp( vk::AttachmentLoadOp::eClear )
          .setStoreOp( vk::AttachmentStoreOp::eStore )
          .setStencilLoadOp( vk::AttachmentLoadOp::eDontCare )
          .setStencilStoreOp( vk::AttachmentStoreOp::eDontCare )
          .setInitialLayout( vk::ImageLayout::eUndefined )
          .setFinalLayout( vk::ImageLayout::ePresentSrcKHR )
      )
      .add_attachment(
        vk::AttachmentDescription()
          .setFormat( vk::Format::eD16Unorm )
          .setSamples( vk::SampleCountFlagBits::e1 )
          .setLoadOp( vk::AttachmentLoadOp::eClear )
          .setStoreOp( vk::AttachmentStoreOp::eStore )
          .setStencilLoadOp( vk::AttachmentLoadOp::eDontCare )
          .setStencilStoreOp( vk::AttachmentStoreOp::eDontCare )
          .setInitialLayout( vk::ImageLayout::eUndefined )
          .setFinalLayout( vk::ImageLayout::eDepthStencilAttachmentOptimal )
      )
      .add_subpass(
        gct::subpass_description_t()
          .add_color_attachment( 0, vk::ImageLayout::eColorAttachmentOptimal )
          .set_depth_stencil_attachment( 1, vk::ImageLayout::eDepthStencilAttachmentOptimal )
      )
    );
  
  const auto gct_allocator = gct_device->get_allocator();

  auto gct_dest_image = gct_allocator->create_image(
    gct::image_create_info_t()
      .set_basic(
        vk::ImageCreateInfo()
          .setImageType( vk::ImageType::e2D )
          .setFormat( vk::Format::eR8G8B8A8Unorm )
          .setExtent( { 1024, 1024, 1 } )
          .setUsage(
            vk::ImageUsageFlagBits::eTransferSrc |
            vk::ImageUsageFlagBits::eColorAttachment
          )
      ),
    VMA_MEMORY_USAGE_GPU_ONLY
  );
  auto gct_depth = gct_allocator->create_image(
    gct::image_create_info_t()
      .set_basic(
        vk::ImageCreateInfo()
          .setImageType( vk::ImageType::e2D )
          .setFormat( vk::Format::eD16Unorm )
          .setExtent( { 1024, 1024, 1 } )
          .setUsage( vk::ImageUsageFlagBits::eDepthStencilAttachment )
      ),
    VMA_MEMORY_USAGE_GPU_ONLY
  );

  auto gct_depth_view = gct_depth->get_view( vk::ImageAspectFlagBits::eDepth );
  auto gct_color_view = gct_dest_image->get_view( vk::ImageAspectFlagBits::eColor );

  const auto instance = **gct_instance;
  const auto physical_device = **gct_physical_device.devices[ 0 ];
  const auto device = **gct_device;
  const auto render_pass = **gct_render_pass;
  const auto allocator = **gct_allocator;
  const auto dest_image = **gct_dest_image;
  const auto depth = **gct_depth;
  const auto depth_view = **gct_depth_view;
  const auto color_view = **gct_color_view;

  // フレームバッファで使うイメージビュー
  std::vector< VkImageView > attachments{
    color_view, // 0番目のアタッチメントがこれになる
    depth_view  // 1番目のアタッチメントがこれになる
  };

  // フレームバッファを作る
  VkFramebufferCreateInfo create_info;
  create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  create_info.pNext = nullptr;
  create_info.flags = 0;
  // このレンダーパスのアタッチメントとして
  create_info.renderPass = render_pass;
  // ここに列挙したイメージビューを使う
  create_info.attachmentCount = attachments.size();
  create_info.pAttachments = attachments.data();
  // フレームバッファのサイズ
  // (全てのイメージビューのサイズと一致していなければならない)
  create_info.width = 1024;
  create_info.height = 1024;
  create_info.layers = 1;

  VkFramebuffer framebuffer;
  if( vkCreateFramebuffer(
    device,
    &create_info,
    nullptr,
    &framebuffer
  ) != VK_SUCCESS ) abort();

  vkDestroyFramebuffer(
    device,
    framebuffer,
    nullptr
  );
}

