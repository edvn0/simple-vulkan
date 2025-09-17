#include "sv/context.hpp"

#include "sv/bindless_access.hpp"
#include "sv/object_handle.hpp"
#include "sv/texture.hpp"
#include "sv/transitions.hpp"

#include "sv/app.hpp"
#include "sv/common.hpp"
#include "sv/renderer.hpp"
#include "sv/scope_exit.hpp"

#include <GLFW/glfw3.h>
#include <cstdlib>
#include <iostream>
#include <span>

namespace sv {

namespace {

static constexpr auto get_pipeline_specialisation_info =
  [](const SpecialisationConstantDescription& d, auto& spec_entries) {
    const auto num_entries = d.get_specialisation_constants_count();
    for (auto i = 0U; i < num_entries; ++i) {
      const auto& [constant_id, offset, size] = d.entries.at(i);
      spec_entries[i] = VkSpecializationMapEntry{
        .constantID = constant_id,
        .offset = offset,
        .size = size,
      };
    }

    return VkSpecializationInfo{
      .mapEntryCount = num_entries,
      .pMapEntries = spec_entries.data(),
      .dataSize = d.data.size_bytes(),
      .pData = d.data.data(),
    };
  };

auto
blend_factor_to_vk_blend_factor(BlendFactor blend_factor) -> VkBlendFactor
{
  switch (blend_factor) {
    case BlendFactor::Zero:
      return VK_BLEND_FACTOR_ZERO;
    case BlendFactor::One:
      return VK_BLEND_FACTOR_ONE;
    case BlendFactor::SrcColor:
      return VK_BLEND_FACTOR_SRC_COLOR;
    case BlendFactor::OneMinusSrcColor:
      return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
    case BlendFactor::SrcAlpha:
      return VK_BLEND_FACTOR_SRC_ALPHA;
    case BlendFactor::OneMinusSrcAlpha:
      return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    case BlendFactor::DstColor:
      return VK_BLEND_FACTOR_DST_COLOR;
    case BlendFactor::OneMinusDstColor:
      return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
    case BlendFactor::DstAlpha:
      return VK_BLEND_FACTOR_DST_ALPHA;
    case BlendFactor::OneMinusDstAlpha:
      return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
    case BlendFactor::SrcAlphaSaturated:
      return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
    case BlendFactor::BlendColor:
      return VK_BLEND_FACTOR_CONSTANT_COLOR;
    case BlendFactor::OneMinusBlendColor:
      return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
    case BlendFactor::BlendAlpha:
      return VK_BLEND_FACTOR_CONSTANT_ALPHA;
    case BlendFactor::OneMinusBlendAlpha:
      return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;
    case BlendFactor::Src1Color:
      return VK_BLEND_FACTOR_SRC1_COLOR;
    case BlendFactor::OneMinusSrc1Color:
      return VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR;
    case BlendFactor::Src1Alpha:
      return VK_BLEND_FACTOR_SRC1_ALPHA;
    case BlendFactor::OneMinusSrc1Alpha:
      return VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA;
  }
  return VK_BLEND_FACTOR_ZERO;
}

auto
blend_op_to_vk_blend_op(BlendOp blend_op) -> VkBlendOp
{
  switch (blend_op) {
    case BlendOp::Add:
      return VK_BLEND_OP_ADD;
    case BlendOp::Subtract:
      return VK_BLEND_OP_SUBTRACT;
    case BlendOp::ReverseSubtract:
      return VK_BLEND_OP_REVERSE_SUBTRACT;
    case BlendOp::Min:
      return VK_BLEND_OP_MIN;
    case BlendOp::Max:
      return VK_BLEND_OP_MAX;
  }
  return VK_BLEND_OP_ADD;
}
auto
topology_to_vk_topology(Topology topology) -> VkPrimitiveTopology
{
  switch (topology) {
    case Topology::Point:
      return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    case Topology::Line:
      return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    case Topology::LineStrip:
      return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
    case Topology::Triangle:
      return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    case Topology::TriangleStrip:
      return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    case Topology::Patch:
      return VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
  }
  return VK_PRIMITIVE_TOPOLOGY_MAX_ENUM;
}

auto
polygon_mode_to_vk_polygon_mode(PolygonMode polygon_mode) -> VkPolygonMode
{
  switch (polygon_mode) {
    case PolygonMode::Fill:
      return VK_POLYGON_MODE_FILL;
    case PolygonMode::Line:
      return VK_POLYGON_MODE_LINE;
  }
  return VK_POLYGON_MODE_FILL;
}

auto
cull_mode_to_vk_cull_mode(CullMode cull_mode) -> VkCullModeFlags
{
  switch (cull_mode) {
    case CullMode::None:
      return VK_CULL_MODE_NONE;
    case CullMode::Front:
      return VK_CULL_MODE_FRONT_BIT;
    case CullMode::Back:
      return VK_CULL_MODE_BACK_BIT;
  }
  return VK_CULL_MODE_NONE;
}

auto
winding_to_vk_winding(WindingMode winding) -> VkFrontFace
{
  switch (winding) {
    case WindingMode::CCW:
      return VK_FRONT_FACE_COUNTER_CLOCKWISE;
    case WindingMode::CW:
      return VK_FRONT_FACE_CLOCKWISE;
  }
  return VK_FRONT_FACE_COUNTER_CLOCKWISE;
}

auto
stencil_op_to_vk_stencil_op(StencilOp stencil_op) -> VkStencilOp
{
  switch (stencil_op) {
    case StencilOp::Keep:
      return VK_STENCIL_OP_KEEP;
    case StencilOp::Zero:
      return VK_STENCIL_OP_ZERO;
    case StencilOp::Replace:
      return VK_STENCIL_OP_REPLACE;
    case StencilOp::IncrementClamp:
      return VK_STENCIL_OP_INCREMENT_AND_CLAMP;
    case StencilOp::DecrementClamp:
      return VK_STENCIL_OP_DECREMENT_AND_CLAMP;
    case StencilOp::Invert:
      return VK_STENCIL_OP_INVERT;
    case StencilOp::IncrementWrap:
      return VK_STENCIL_OP_INCREMENT_AND_WRAP;
    case StencilOp::DecrementWrap:
      return VK_STENCIL_OP_DECREMENT_AND_WRAP;
  }
  return VK_STENCIL_OP_KEEP;
}

auto
compare_op_to_vk_compare_op(CompareOp compare_op) -> VkCompareOp
{
  switch (compare_op) {
    case CompareOp::Never:
      return VK_COMPARE_OP_NEVER;
    case CompareOp::Less:
      return VK_COMPARE_OP_LESS;
    case CompareOp::Equal:
      return VK_COMPARE_OP_EQUAL;
    case CompareOp::LessEqual:
      return VK_COMPARE_OP_LESS_OR_EQUAL;
    case CompareOp::Greater:
      return VK_COMPARE_OP_GREATER;
    case CompareOp::NotEqual:
      return VK_COMPARE_OP_NOT_EQUAL;
    case CompareOp::GreaterEqual:
      return VK_COMPARE_OP_GREATER_OR_EQUAL;
    case CompareOp::AlwaysPass:
      return VK_COMPARE_OP_ALWAYS;
  }
  return VK_COMPARE_OP_ALWAYS;
}

constexpr VkShaderStageFlags all_stages_flags =
  VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT |
  VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT |
  VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT | VK_SHADER_STAGE_COMPUTE_BIT;

VKAPI_ATTR VkBool32 VKAPI_CALL
vk_debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                  VkDebugUtilsMessageTypeFlagsEXT messageType,
                  const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                  void* user_data)
{
  auto ms = vkb::to_string_message_severity(messageSeverity);
  auto mt = vkb::to_string_message_type(messageType);

  static auto& out = std::cerr;

  if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT) {
    out << std::format("[{}: {}] - {}\n{}\n",
                       ms,
                       mt,
                       pCallbackData->pMessageIdName,
                       pCallbackData->pMessage);
  } else {
    out << std::format("[{}: {}]\n{}\n", ms, mt, pCallbackData->pMessage);
  }
  std::flush(out);

  if (const auto user = static_cast<ContextConfiguration*>(user_data)) {
    if (user->abort_on_validation_error &&
        ((messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) !=
         0)) {
      _CrtDbgBreak();
    }
  }

  return VK_FALSE;
}
}

VulkanContext::~VulkanContext()
{
  destroy(*dummy_texture);

  staging_allocator.reset();
  immediate_commands.reset();

  while (!delete_queue.empty()) {
    auto back = std::move(delete_queue.back());
    delete_queue.pop_back();
    back(*this);
  }
  while (!pre_frame_queue.empty()) {
    auto back = std::move(pre_frame_queue.back());
    pre_frame_queue.pop_back();
    back(*this);
  }

  assert(textures.size() == 0);

  DeviceAllocator::deinitialise();

  vkDestroySurfaceKHR(instance, surface, nullptr);
  vkb::destroy_device(device);
  vkb::destroy_instance(instance);
}

auto
VulkanContext::create(const Window& window, const ContextConfiguration& conf)
  -> std::expected<std::unique_ptr<IContext>, ContextError>
{
  using enum ContextError::Code;
  auto ptr = static_cast<GLFWwindow*>(window.opaque_handle);
  if (!ptr)
    return make_error(ContextError{ InvalidWindow, "Window not initialised" });

  VulkanContext::config = conf;

  vkb::InstanceBuilder instance_builder;
  auto instance_ret =
    instance_builder.require_api_version(1, 4)
      .set_debug_callback(vk_debug_callback)
      .set_debug_callback_user_data_pointer(&VulkanContext::config)
      .request_validation_layers()
      .build();
  if (!instance_ret) {
    std::cout << instance_ret.error().message() << "\n";
    const auto& detailed_reasons = instance_ret.detailed_failure_reasons();
    if (!detailed_reasons.empty()) {
      std::cerr << "Instance Selection failure reasons:\n";
      for (const std::string& reason : detailed_reasons) {
        std::cerr << reason << "\n";
      }
    }
    return make_error(ContextError{ InvalidWindow, "blah" });
  }
  auto instance = instance_ret.value();

  auto inst_disp = instance.make_table();

  vkb::PhysicalDeviceSelector phys_device_selector(instance);

  VkSurfaceKHR surface = VK_NULL_HANDLE;
  VkResult err = glfwCreateWindowSurface(instance, ptr, nullptr, &surface);
  if (err) {
    const char* error_msg;
    int ret = glfwGetError(&error_msg);
    if (ret != 0) {
      std::cout << ret << " ";
      if (error_msg != nullptr)
        std::cout << error_msg;
      std::cout << "\n";
    }
    surface = VK_NULL_HANDLE;
  }

  VkPhysicalDeviceFeatures required_features{};
  required_features.multiViewport = true;
  required_features.multiDrawIndirect = true;
  required_features.inheritedQueries = true;
  required_features.sampleRateShading = true;
  required_features.geometryShader = true;

  VkPhysicalDeviceVulkan11Features required_11_features{};
  required_11_features.shaderDrawParameters = true;

  VkPhysicalDeviceVulkan12Features required_12_features{};
  required_12_features.bufferDeviceAddress = true;
  required_12_features.descriptorIndexing = true;
  required_12_features.timelineSemaphore = true;
  required_12_features.hostQueryReset = true;
  required_12_features.runtimeDescriptorArray = true;
  required_12_features.descriptorBindingSampledImageUpdateAfterBind = true;
  required_12_features.descriptorBindingUniformBufferUpdateAfterBind = true;
  required_12_features.descriptorBindingSampledImageUpdateAfterBind = true;
  required_12_features.descriptorBindingStorageImageUpdateAfterBind = true;
  required_12_features.descriptorBindingStorageBufferUpdateAfterBind = true;
  required_12_features.descriptorBindingUniformTexelBufferUpdateAfterBind =
    true;
  required_12_features.descriptorBindingStorageTexelBufferUpdateAfterBind =
    true;
  required_12_features.descriptorBindingUpdateUnusedWhilePending = true;
  required_12_features.descriptorBindingPartiallyBound = true;
  required_12_features.descriptorBindingVariableDescriptorCount = true;
  required_12_features.shaderInputAttachmentArrayDynamicIndexing = true;
  required_12_features.shaderUniformTexelBufferArrayDynamicIndexing = true;
  required_12_features.shaderStorageTexelBufferArrayDynamicIndexing = true;
  required_12_features.shaderUniformBufferArrayNonUniformIndexing = true;
  required_12_features.shaderSampledImageArrayNonUniformIndexing = true;
  required_12_features.shaderStorageBufferArrayNonUniformIndexing = true;
  required_12_features.shaderStorageImageArrayNonUniformIndexing = true;

  required_12_features.shaderUniformTexelBufferArrayNonUniformIndexing = false;
  required_12_features.shaderStorageTexelBufferArrayNonUniformIndexing = false;
  required_12_features.shaderInputAttachmentArrayNonUniformIndexing = false;

  VkPhysicalDeviceVulkan13Features required_13_features{};
  required_13_features.dynamicRendering = true;
  required_13_features.synchronization2 = true;

  auto phys_device_builder =
    phys_device_selector.set_minimum_version(1, 3)
      .set_surface(surface)
      .add_required_extension(VK_KHR_SWAPCHAIN_EXTENSION_NAME)
      .set_required_features(required_features)
      .set_required_features_11(required_11_features)
      .set_required_features_12(required_12_features)
      .set_required_features_13(required_13_features);

  auto phys_device_ret = phys_device_builder.select();
  if (!phys_device_ret) {
    std::cout << phys_device_ret.error().message() << "\n";
    if (phys_device_ret.error() ==
        vkb::PhysicalDeviceError::no_suitable_device) {
      const auto& detailed_reasons = phys_device_ret.detailed_failure_reasons();
      if (!detailed_reasons.empty()) {
        std::cerr << "GPU Selection failure reasons:\n";
        for (const std::string& reason : detailed_reasons) {
          std::cerr << reason << "\n";
        }
      }
    }
    return make_error<ContextError>(InvalidWindow, "blah");
  }
  vkb::PhysicalDevice physical_device = phys_device_ret.value();

  vkb::DeviceBuilder device_builder{ physical_device };

  auto device_ret = device_builder.build();
  if (!device_ret) {
    std::cout << device_ret.error().message() << "\n";
    return make_error<ContextError>(InvalidWindow, "blah");
  }
  auto device = device_ret.value();

  auto disp = device.make_table();

  auto gfam = device.get_queue_index(vkb::QueueType::graphics).value();
  auto pfam = device.get_queue_index(vkb::QueueType::present).value();
  VkQueue gq{}, pq{};
  vkGetDeviceQueue(device, gfam, 0, &gq);
  vkGetDeviceQueue(device, pfam, 0, &pq);

  using P = std::unique_ptr<IContext>;
  auto vk_context = new VulkanContext(std::move(instance),
                                      std::move(device),
                                      std::move(disp),
                                      std::move(inst_disp),
                                      surface,
                                      gq,
                                      gfam,
                                      pq,
                                      pfam);

  DeviceAllocator::initialise(*vk_context);
  return std::expected<P, ContextError>{ std::in_place, vk_context };
}

auto
DeviceAllocator::the() -> VmaAllocator&
{
#ifdef _DEBUG
  static std::mutex allocator_mutex{};
  std::unique_lock lock{ allocator_mutex };
  return allocator;
#else
  return allocator;
#endif
}
auto
DeviceAllocator::initialise(IContext& ctx) -> void
{
  VmaAllocatorCreateInfo create_info{};
  create_info.instance = ctx.get_instance();
  create_info.physicalDevice = ctx.get_physical_device();
  create_info.device = ctx.get_device();
  create_info.vulkanApiVersion = VK_API_VERSION_1_4;
  create_info.flags = VMA_ALLOCATOR_CREATE_KHR_DEDICATED_ALLOCATION_BIT |
                      VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT |
                      VMA_ALLOCATOR_CREATE_EXT_MEMORY_PRIORITY_BIT;

  auto& current = the();
  if (vmaCreateAllocator(&create_info, &current) != VK_SUCCESS) {
    std::abort();
  }
}

auto
query_vulkan_properties(VkPhysicalDevice physical_device, auto& props) -> void
{
  vkGetPhysicalDeviceProperties(physical_device, &props.base);

  props.fourteen.sType =
    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_PROPERTIES;
  props.fourteen.pNext = nullptr;

  props.thirteen.sType =
    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_PROPERTIES;
  props.thirteen.pNext = &props.fourteen;

  props.twelve.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES;
  props.twelve.pNext = &props.thirteen;

  props.eleven.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES;
  props.eleven.pNext = &props.twelve;

  VkPhysicalDeviceProperties2 device_props2{};
  device_props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
  device_props2.pNext = &props.eleven;

  vkGetPhysicalDeviceProperties2(physical_device, &device_props2);

  props.base = device_props2.properties;
}

VulkanContext::VulkanContext(vkb::Instance&& i,
                             vkb::Device&& d,
                             vkb::DispatchTable&& dt,
                             vkb::InstanceDispatchTable&& idt,
                             VkSurfaceKHR surf,
                             VkQueue gq,
                             std::uint32_t gfam,
                             VkQueue pq,
                             std::uint32_t pfam)
  : instance(std::move(i))
  , device(std::move(d))
  , dispatch_table(std::move(dt))
  , instance_dispatch_table(std::move(idt))
  , surface(surf)
  , graphics_queue(gq)
  , present_queue(pq)
  , graphics_family(gfam)
  , present_family(pfam)
{
  query_vulkan_properties(device.physical_device, vulkan_properties);
}

auto
VulkanContext::create_placeholder_resources() -> void
{
  const uint32_t pixel = 0xFFFFFFFF;
  std::span white_texture_span(&pixel, 1);
  dummy_texture = VulkanTextureND::create(
    *this,
    {
      .format = Format::RGBA_UN8,
      .usage_bits = TextureUsageBits::Sampled | TextureUsageBits::Storage,
      .pixel_data = std::as_bytes(white_texture_span),
      .debug_name = "White Texture (reserved)",
    });
}

auto
VulkanContext::destroy(TextureHandle handle) -> void
{
  SCOPE_EXIT
  {
    textures.erase(handle);
    needs_descriptor_update = true;
  };
  auto* tex = textures.get(handle);
  if (!tex)
    return;
  defer_task([view = tex->image_view](IContext& ctx) {
    vkDestroyImageView(ctx.get_device(), view, nullptr);
  });
  if (tex->storage_image_view) {
    defer_task([view = tex->storage_image_view](IContext& ctx) {
      vkDestroyImageView(ctx.get_device(), view, nullptr);
    });
  }
  for (size_t i = 0; i != max_mip_levels_framebuffer; i++) {
    for (size_t j = 0; j != max_layers_framebuffer; j++) {
      VkImageView v = tex->framebuffer_image_views.at(i).at(j);
      if (v) {
        defer_task([imageView = v](IContext& ctx) {
          vkDestroyImageView(ctx.get_device(), imageView, nullptr);
        });
      }
    }
  }

  if (!tex->is_owning_image)
    return;
  if (tex->allocation_info.pMappedData)
    vmaUnmapMemory(DeviceAllocator::the(), tex->allocation);
  defer_task(([image = tex->image, allocation = tex->allocation](IContext&) {
    vmaDestroyImage(DeviceAllocator::the(), image, allocation);
  }));
};

auto
VulkanContext::destroy(const ShaderModuleHandle handle) -> void
{
  if (!handle.valid()) {
    return;
  }

  const auto maybe_shader = get_shader_module_pool().get(handle);
  if (!maybe_shader) {
    return;
  }

  for (const auto shader = *maybe_shader;
       const auto& module : shader.get_modules()) {
    defer_task([m = module.module](auto& ctx) {
      vkDestroyShaderModule(ctx.get_device(), m, nullptr);
    });
  }
}

auto
VulkanContext::destroy(const ComputePipelineHandle handle) -> void
{
  if (!handle.valid()) {
    return;
  }

  const auto maybe_pipeline = get_compute_pipeline_pool().get(handle);
  if (!maybe_pipeline) {
    return;
  }

  auto* pipeline = maybe_pipeline;
  if (pipeline == nullptr) {
    return;
  }

  defer_task([ptr = pipeline->get_pipeline(),
              layout = pipeline->get_layout()](auto& ctx) {
    auto device = ctx.get_device();
    auto allocation_callbacks = nullptr;
    vkDestroyPipeline(device, ptr, allocation_callbacks);
    vkDestroyPipelineLayout(device, layout, allocation_callbacks);
  });
}

auto
VulkanContext::destroy(const GraphicsPipelineHandle handle) -> void
{
  if (!handle.valid()) {
    return;
  }

  const auto maybe_pipeline = get_graphics_pipeline_pool().get(handle);
  if (!maybe_pipeline) {
    return;
  }

  auto* pipeline = maybe_pipeline;
  if (pipeline == nullptr) {
    return;
  }

  defer_task([ptr = pipeline->get_pipeline(),
              layout = pipeline->get_layout()](auto& ctx) {
    auto device = ctx.get_device();
    auto allocation_callbacks = nullptr;
    vkDestroyPipeline(device, ptr, allocation_callbacks);
    vkDestroyPipelineLayout(device, layout, allocation_callbacks);
  });
}

auto
VulkanContext::destroy(BufferHandle handle) -> void
{
  SCOPE_EXIT
  {
    buffers.erase(handle);
  };
  auto* buf = buffers.get(handle);
  if (!buf)
    return;
  defer_task([buffer = buf->buffer, allocation = buf->allocation](IContext&) {
    vmaDestroyBuffer(DeviceAllocator::the(), buffer, allocation);
  });
}

auto
VulkanContext::flush_mapped_memory(BufferHandle handle, OffsetSize os) const
  -> void
{
  auto* buf = get_buffer_pool().get(handle);
  vmaFlushAllocation(
    DeviceAllocator::the(), buf->allocation, os.offset, os.size);
}

auto
VulkanContext::invalidate_mapped_memory(BufferHandle handle,
                                        OffsetSize os) const -> void
{
  auto* buf = get_buffer_pool().get(handle);
  vmaInvalidateAllocation(
    DeviceAllocator::the(), buf->allocation, os.offset, os.size);
}

auto
VulkanContext::submit(ICommandBuffer& cmd, TextureHandle present) -> void
{
  const auto& vk_buffer = static_cast<CommandBuffer&>(cmd);

#if defined(LVK_WITH_TRACY_GPU)
  TracyVkCollect(pimpl_->tracyVkCtx_, vk_buffer.get_command_buffer());
#endif // LVK_WITH_TRACY_GPU

  if (present) {
    const auto* tex = textures.get(present);

    assert(tex->is_swapchain_image);

    Transition::swapchain_image(vk_buffer.get_command_buffer(),
                                tex->image,
                                VK_IMAGE_LAYOUT_UNDEFINED,
                                VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
  }

  command_buffer.last_submit_handle =
    immediate_commands->submit(*command_buffer.wrapper);

  BindlessAccess<VulkanContext>::process_pre_frame_work(*this);

  command_buffer = {};
}

auto
VulkanContext::get_pipeline(ComputePipelineHandle handle) -> VkPipeline
{
  auto* cps = get_compute_pipeline_pool().get(handle);

  if (!cps) {
    return VK_NULL_HANDLE;
  }

  BindlessAccess<VulkanContext>::process_pre_frame_work(*this);

  if (cps->new_shader ||
      cps->last_descriptor_set_layout != descriptors.layout) {
    defer_task([l = cps->get_layout()](auto& ctx) {
      vkDestroyPipelineLayout(ctx.get_device(), l, nullptr);
    });
    defer_task([p = cps->get_pipeline()](auto& ctx) {
      vkDestroyPipeline(ctx.get_device(), p, nullptr);
    });
    cps->pipeline = VK_NULL_HANDLE;
    cps->layout = VK_NULL_HANDLE;
    cps->last_descriptor_set_layout = descriptors.layout;
    cps->new_shader = false;
  }

  if (cps->pipeline == VK_NULL_HANDLE) {
    const auto* sm = get_shader_module_pool().get(cps->description.shader);

    std::array<VkSpecializationMapEntry,
               SpecialisationConstantDescription::max_specialization_constants>
      entries = {};

    const VkSpecializationInfo siComp = get_pipeline_specialisation_info(
      cps->description.specialisation_constants, entries);

    // create pipeline layout
    {
      // duplicate for MoltenVK
      const std::array dsls = {
        descriptors.layout,
      };
      assert(sm->get_push_constant_info().first <=
             vulkan_properties.base.limits.maxPushConstantsSize);
      const VkPushConstantRange range = {
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        .offset = 0,
        .size = static_cast<uint32_t>(
          get_aligned_size(sm->get_push_constant_info().first, 4)),
      };
      const VkPipelineLayoutCreateInfo ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .setLayoutCount = static_cast<uint32_t>(dsls.size()),
        .pSetLayouts = dsls.data(),
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &range,
      };
      vkCreatePipelineLayout(get_device(), &ci, nullptr, &cps->layout);
      set_name(*this,
               cps->get_layout(),
               VK_OBJECT_TYPE_PIPELINE_LAYOUT,
               "Compute Pipeline Layout {}",
               cps->description.debug_name);
    }

    auto maybe_module = std::ranges::find_if(
      sm->get_modules(), [entry = cps->description.entry_point](auto m) {
        return m.entry_name == entry;
      });
    assert(maybe_module != sm->get_modules().end());
    auto module = *maybe_module;

    VkPipelineShaderStageCreateInfo psci{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .stage = VK_SHADER_STAGE_COMPUTE_BIT,
      .module = module.module,
      .pName = module.entry_name.c_str(),
      .pSpecializationInfo = &siComp,
    };
    const VkComputePipelineCreateInfo ci = {
      .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .stage = psci,
      .layout = cps->layout,
      .basePipelineHandle = VK_NULL_HANDLE,
      .basePipelineIndex = -1,
    };
    vkCreateComputePipelines(
      get_device(), nullptr, 1, &ci, nullptr, &cps->pipeline);
    set_name(*this,
             cps->get_pipeline(),
             VK_OBJECT_TYPE_PIPELINE,
             "Compute Pipeline {}",
             cps->description.debug_name);
  }

  return cps->pipeline;
}

auto
VulkanContext::get_pipeline(GraphicsPipelineHandle handle) -> VkPipeline
{
  auto* rps = get_graphics_pipeline_pool().get(handle);

  if (!rps) {
    return VK_NULL_HANDLE;
  }

  if (rps->new_shader ||
      rps->last_descriptor_set_layout != descriptors.layout) {
    defer_task([l = rps->get_layout()](auto& ctx) {
      vkDestroyPipelineLayout(ctx.get_device(), l, nullptr);
    });
    defer_task([p = rps->get_pipeline()](auto& ctx) {
      vkDestroyPipeline(ctx.get_device(), p, nullptr);
    });

    rps->pipeline = VK_NULL_HANDLE;
    rps->last_descriptor_set_layout = descriptors.layout;
    static constexpr auto viewMask = 0U;
    rps->view_mask = viewMask;
    rps->new_shader = false;
  }

  if (rps->pipeline != VK_NULL_HANDLE) {
    return rps->pipeline;
  }

  VkPipelineLayout layout = VK_NULL_HANDLE;
  VkPipeline pipeline = VK_NULL_HANDLE;

  const auto& desc = rps->description;

  const auto colour_attachments_count =
    rps->description.get_colour_attachments_count();

  std::array<VkPipelineColorBlendAttachmentState, max_colour_attachments>
    color_blend_attachment_states{};
  std::array<VkFormat, max_colour_attachments> color_attachment_formats{};

  for (auto i = 0U; i != colour_attachments_count; i++) {
    const auto& [format,
                 blend_enabled,
                 rgb_blend_op,
                 alpha_blend_op,
                 src_rgb_blend_factor,
                 src_alpha_blend_factor,
                 dst_rgb_blend_factor,
                 dst_alpha_blend_factor] = desc.color[i];
    assert(format != Format::Invalid);
    color_attachment_formats[i] = format_to_vk_format(format);
    if (!blend_enabled) {
      color_blend_attachment_states[i] = VkPipelineColorBlendAttachmentState{
        .blendEnable = VK_FALSE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp = VK_BLEND_OP_ADD,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
      };
    } else {
      color_blend_attachment_states[i] = VkPipelineColorBlendAttachmentState{
        .blendEnable = VK_TRUE,
        .srcColorBlendFactor =
          blend_factor_to_vk_blend_factor(src_rgb_blend_factor),
        .dstColorBlendFactor =
          blend_factor_to_vk_blend_factor(dst_rgb_blend_factor),
        .colorBlendOp = blend_op_to_vk_blend_op(rgb_blend_op),
        .srcAlphaBlendFactor =
          blend_factor_to_vk_blend_factor(src_alpha_blend_factor),
        .dstAlphaBlendFactor =
          blend_factor_to_vk_blend_factor(dst_alpha_blend_factor),
        .alphaBlendOp = blend_op_to_vk_blend_op(alpha_blend_op),
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
      };
    }
  }

  const auto* shader = get_shader_module_pool().get(desc.shader);

  assert(shader);

  /*  if (tescModule || teseModule || desc.patchControlPoints) {
      LVK_ASSERT_MSG(tescModule && teseModule, "Both tessellation control and
    evaluation shaders should be provided"); LVK_ASSERT(desc.patchControlPoints
    > 0 && desc.patchControlPoints <=
    vkPhysicalDeviceProperties2_.properties.limits.maxTessellationPatchSize);
    }
  */
  const VkPipelineVertexInputStateCreateInfo ciVertexInputState = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .vertexBindingDescriptionCount = rps->binding_count,
    .pVertexBindingDescriptions =
      rps->binding_count > 0 ? rps->bindings.data() : nullptr,
    .vertexAttributeDescriptionCount = rps->attribute_count,
    .pVertexAttributeDescriptions =
      rps->attribute_count > 0 ? rps->attributes.data() : nullptr,
  };

  std::array<VkSpecializationMapEntry,
             SpecialisationConstantDescription::max_specialization_constants>
    entries{};

  const VkSpecializationInfo si =
    get_pipeline_specialisation_info(desc.specialisation_constants, entries);

  // create pipeline layout
  {
    auto&& [size, flags] = shader->get_push_constant_info();

    // duplicate for MoltenVK
    const std::array<VkDescriptorSetLayout, 1> dsls = {

      descriptors.layout
    };
    assert(size <= vulkan_properties.base.limits.maxPushConstantsSize);
    const VkPushConstantRange range = {
      .stageFlags = rps->stage_flags,
      .offset = 0,
      .size = static_cast<uint32_t>(get_aligned_size(size, 4)),
    };
    const VkPipelineLayoutCreateInfo ci = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .setLayoutCount = static_cast<std::uint32_t>(std::size(dsls)),
      .pSetLayouts = dsls.data(),
      .pushConstantRangeCount = size ? 1u : 0u,
      .pPushConstantRanges = size ? &range : nullptr,
    };
    assert(!desc.debug_name.empty());
    vkCreatePipelineLayout(get_device(), &ci, nullptr, &layout);
    set_name(*this,
             layout,
             VK_OBJECT_TYPE_PIPELINE_LAYOUT,
             "Pipeline_Layout_{}",
             desc.debug_name);
  }

  std::array dynamic_states = {
    VK_DYNAMIC_STATE_VIEWPORT,          VK_DYNAMIC_STATE_SCISSOR,
    VK_DYNAMIC_STATE_DEPTH_BIAS,        VK_DYNAMIC_STATE_BLEND_CONSTANTS,
    VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE, VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE,
    VK_DYNAMIC_STATE_DEPTH_COMPARE_OP,  VK_DYNAMIC_STATE_DEPTH_BIAS_ENABLE
  };

  VkPipelineDynamicStateCreateInfo ci_dynamic{};
  ci_dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  ci_dynamic.dynamicStateCount = static_cast<uint32_t>(dynamic_states.size());
  ci_dynamic.pDynamicStates = dynamic_states.data();

  VkPipelineInputAssemblyStateCreateInfo ci_ia{};
  ci_ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  ci_ia.topology = topology_to_vk_topology(desc.topology);

  VkPipelineRasterizationStateCreateInfo ci_rs{};
  ci_rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  ci_rs.polygonMode = polygon_mode_to_vk_polygon_mode(desc.polygon_mode);
  ci_rs.cullMode = cull_mode_to_vk_cull_mode(desc.cull_mode);
  ci_rs.frontFace = winding_to_vk_winding(desc.winding);
  ci_rs.depthBiasEnable = VK_FALSE;
  ci_rs.lineWidth = 1.0f;

  auto getVulkanSampleCountFlags = [](const uint32_t sample_count,
                                      VkSampleCountFlags max_samples_mask) {
    if (sample_count <= 1 || VK_SAMPLE_COUNT_2_BIT > max_samples_mask) {
      return VK_SAMPLE_COUNT_1_BIT;
    }
    if (sample_count <= 2 || VK_SAMPLE_COUNT_4_BIT > max_samples_mask) {
      return VK_SAMPLE_COUNT_2_BIT;
    }
    if (sample_count <= 4 || VK_SAMPLE_COUNT_8_BIT > max_samples_mask) {
      return VK_SAMPLE_COUNT_4_BIT;
    }
    if (sample_count <= 8 || VK_SAMPLE_COUNT_16_BIT > max_samples_mask) {
      return VK_SAMPLE_COUNT_8_BIT;
    }
    if (sample_count <= 16 || VK_SAMPLE_COUNT_32_BIT > max_samples_mask) {
      return VK_SAMPLE_COUNT_16_BIT;
    }
    if (sample_count <= 32 || VK_SAMPLE_COUNT_64_BIT > max_samples_mask) {
      return VK_SAMPLE_COUNT_32_BIT;
    }
    return VK_SAMPLE_COUNT_64_BIT;
  };

  auto limits = vulkan_properties.base.limits.framebufferColorSampleCounts &
                vulkan_properties.base.limits.framebufferDepthSampleCounts;

  VkSampleCountFlagBits samples =
    getVulkanSampleCountFlags(desc.sample_count, limits);
  VkPipelineMultisampleStateCreateInfo ci_ms{};
  ci_ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  ci_ms.rasterizationSamples = samples;
  ci_ms.sampleShadingEnable =
    desc.min_sample_shading > 0.0f ? VK_TRUE : VK_FALSE;
  ci_ms.minSampleShading = desc.min_sample_shading;

  VkStencilOpState front{};
  front.failOp = stencil_op_to_vk_stencil_op(
    desc.front_face_stencil.stencil_failure_operation);
  front.passOp = stencil_op_to_vk_stencil_op(
    desc.front_face_stencil.depth_stencil_pass_operation);
  front.depthFailOp = stencil_op_to_vk_stencil_op(
    desc.front_face_stencil.depth_failure_operation);
  front.compareOp =
    compare_op_to_vk_compare_op(desc.front_face_stencil.stencil_compare_op);
  front.compareMask = desc.front_face_stencil.read_mask;
  front.writeMask = desc.front_face_stencil.write_mask;
  front.reference = 0xFF;

  VkStencilOpState back{};
  back.failOp = stencil_op_to_vk_stencil_op(
    desc.back_face_stencil.stencil_failure_operation);
  back.passOp = stencil_op_to_vk_stencil_op(
    desc.back_face_stencil.depth_stencil_pass_operation);
  back.depthFailOp =
    stencil_op_to_vk_stencil_op(desc.back_face_stencil.depth_failure_operation);
  back.compareOp =
    compare_op_to_vk_compare_op(desc.back_face_stencil.stencil_compare_op);
  back.compareMask = desc.back_face_stencil.read_mask;
  back.writeMask = desc.back_face_stencil.write_mask;
  back.reference = 0xFF;

  VkPipelineDepthStencilStateCreateInfo ci_ds{};
  ci_ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  ci_ds.depthTestEnable = VK_TRUE;                    // dynamic
  ci_ds.depthWriteEnable = VK_TRUE;                   // dynamic
  ci_ds.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL; // dynamic
  ci_ds.depthBoundsTestEnable = VK_FALSE;
  ci_ds.stencilTestEnable =
    (desc.front_face_stencil.enabled || desc.back_face_stencil.enabled)
      ? VK_TRUE
      : VK_FALSE;
  ci_ds.front = front;
  ci_ds.back = back;

  VkPipelineViewportStateCreateInfo ci_vs{};
  ci_vs.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  ci_vs.viewportCount = 1;
  ci_vs.scissorCount = 1;

  VkPipelineColorBlendStateCreateInfo ci_cb{};
  ci_cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  ci_cb.attachmentCount = colour_attachments_count;
  ci_cb.pAttachments = color_blend_attachment_states.data();

  VkPipelineTessellationStateCreateInfo ci_ts{};
  bool has_tess = (shader->has_stage(ShaderStage::tessellation_control) &&
                   shader->has_stage(ShaderStage::tessellation_evaluation)) &&
                  desc.patch_control_points > 0;
  if (has_tess) {
    ci_ts.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
    ci_ts.patchControlPoints = desc.patch_control_points;
  }

  std::vector<VkPipelineShaderStageCreateInfo> stages;
  shader->populate_stages(stages, si);

  VkPipelineRenderingCreateInfo ci_rendering{};
  ci_rendering.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
  static constexpr auto view_mask = 0U;
  ci_rendering.viewMask = view_mask;
  ci_rendering.colorAttachmentCount = colour_attachments_count;
  ci_rendering.pColorAttachmentFormats = color_attachment_formats.data();
  ci_rendering.depthAttachmentFormat = format_to_vk_format(desc.depth_format);
  ci_rendering.stencilAttachmentFormat =
    format_to_vk_format(desc.stencil_format);

  VkGraphicsPipelineCreateInfo ci_gp{};
  ci_gp.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  ci_gp.pNext = &ci_rendering;
  ci_gp.stageCount = static_cast<uint32_t>(stages.size());
  ci_gp.pStages = stages.data();
  ci_gp.pVertexInputState = &ciVertexInputState;
  ci_gp.pInputAssemblyState = &ci_ia;
  ci_gp.pViewportState = &ci_vs;
  ci_gp.pRasterizationState = &ci_rs;
  ci_gp.pMultisampleState = &ci_ms;
  ci_gp.pDepthStencilState = &ci_ds;
  ci_gp.pColorBlendState = &ci_cb;
  ci_gp.pDynamicState = &ci_dynamic;
  ci_gp.pTessellationState = has_tess ? &ci_ts : nullptr;
  ci_gp.layout = layout;

  if (const auto res = vkCreateGraphicsPipelines(
        get_device(), nullptr, 1, &ci_gp, nullptr, &pipeline);
      res != VK_SUCCESS) {
    return VK_NULL_HANDLE;
  }

  rps->pipeline = pipeline;
  rps->layout = layout;

  return pipeline;
}
}