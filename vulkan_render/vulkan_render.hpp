#pragma once
#include <vulkan/vulkan_raii.hpp>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_shared.hpp>

#include <iostream>
#include <cassert>
#include <array>
#include <span>
#include <algorithm>
#include <numeric>
#include <ranges>
#include <filesystem>
#include <set>

#define max max
#include "spirv_reader.hpp"
#include "shader_path.hpp"
#include "multidimention_array.hpp"
#include "font_loader.hpp"
#include "run_result.hpp"

using namespace std::literals;


template<std::integral Int>
class integer_less_equal {
public:
    integer_less_equal(Int v, Int maximum) : m_value{ v }, m_max{ maximum } {
        check_valid();
    }
    integer_less_equal& operator=(int d) {
        m_value = d;
        check_valid();
    }
    operator Int() const {
        return m_value;
    }
    integer_less_equal& operator++() {
        ++m_value;
        check_valid();
        return *this;
    }
    Int operator++(int) {
        auto ret = m_value++;
        check_valid();
        return ret;
    }
private:
    void check_valid() {
        assert(m_value <= m_max);
    }
    Int m_value;
    const Int m_max;
};

template<class T>
class from_0_count_n {
public:
    struct T_ite {
        using difference_type = int64_t;
        using value_type = T;
        using pointer = T*;
        using reference = T&;
        using iterator_category = std::random_access_iterator_tag;
        using iterator_concept = std::contiguous_iterator_tag;
        constexpr T operator*() const noexcept { return value; }
        constexpr T_ite& operator++() noexcept { ++value; return *this; }
        constexpr T_ite operator++(int) noexcept { return T_ite{ value++ }; }
        constexpr T_ite& operator--() noexcept { --value; return *this; }
        constexpr T_ite operator--(int) noexcept { return T_ite{ value-- }; }
        constexpr bool operator==(T_ite rhs) const noexcept{
            return value == rhs.value;
        }
        constexpr int64_t operator-(T_ite rhs) const noexcept {
            return static_cast<int64_t>(value) - rhs.value;
        }
        T value;
    };
    constexpr from_0_count_n(T count) : m_count{ count } {}
    constexpr T size() noexcept { return m_count; }
    constexpr T_ite begin() noexcept { return T_ite{ 0 }; }
    constexpr T_ite end() noexcept { return T_ite{ m_count }; }
private:
    T m_count;
};

template<class T>
inline constexpr bool std::ranges::enable_borrowed_range<from_0_count_n<T>> = true;

namespace vulkan {
    inline auto create_instance() {
        vk::ApplicationInfo applicationInfo("Terminal Emulator", 1, nullptr, 0, VK_API_VERSION_1_3);
        std::array<const char*, 0> instanceLayers{};
        std::array<const char*, 2> instanceExtensions{ "VK_KHR_surface", 
#ifdef WIN32
            "VK_KHR_win32_surface",
#endif
#ifdef __unix__
            "VK_KHR_xcb_surface",
#endif
        };
        vk::InstanceCreateInfo instanceCreateInfo({}, &applicationInfo, instanceLayers, instanceExtensions);
        return vk::SharedInstance{ vk::createInstance(instanceCreateInfo) };
    }
    inline auto select_physical_device(vk::Instance instance) {
        auto devices = instance.enumeratePhysicalDevices();
        return devices.front();
    }
    inline auto select_queue_family(vk::PhysicalDevice physical_device) {
        uint32_t graphicsQueueFamilyIndex = 0;
        auto queueFamilyProperties = physical_device.getQueueFamilyProperties();
        for (int i = 0; i < queueFamilyProperties.size(); i++) {
            if (queueFamilyProperties[i].queueFlags & vk::QueueFlagBits::eGraphics) {
                graphicsQueueFamilyIndex = i;
            }
        }
        return graphicsQueueFamilyIndex;
    }
    inline auto create_device(vk::PhysicalDevice physical_device, uint32_t graphicsQueueFamilyIndex) {
        float queuePriority = 0.0f;
        vk::DeviceQueueCreateInfo deviceQueueCreateInfo({}, graphicsQueueFamilyIndex, 1, &queuePriority);
        std::array<std::string, 2> deviceExtensions{
            "VK_KHR_swapchain",
            "VK_EXT_mesh_shader",
        };
        std::vector<vk::ExtensionProperties> available_device_extensions =
            physical_device.enumerateDeviceExtensionProperties();
        std::vector<std::string> nonavailable_device_extensions{};
        std::vector<std::string> availables(available_device_extensions.size());
        std::ranges::transform(
            available_device_extensions,
            availables.begin(),
            [](auto&& exten) { return std::string{exten.extensionName.data()}; });
        std::ranges::sort(deviceExtensions);
        std::ranges::sort(availables);
        std::ranges::set_difference(
                deviceExtensions, availables, 
                std::back_inserter(nonavailable_device_extensions),
                {},
                [](auto exten) { return std::string{exten}; }
                );
        if (nonavailable_device_extensions.size() > 0) {
            throw std::runtime_error{"not supported device extensions: "s + nonavailable_device_extensions[0]};
        }
        std::vector<const char*> device_extensions(deviceExtensions.size());
        std::ranges::transform(
            deviceExtensions,
            device_extensions.begin(),
            [](auto& ext) { return ext.data();  });
        vk::StructureChain device_create_info{
            vk::DeviceCreateInfo{}
            .setQueueCreateInfos(deviceQueueCreateInfo)
            .setPEnabledExtensionNames(device_extensions),
            vk::PhysicalDeviceFeatures2{},
            vk::PhysicalDeviceMeshShaderFeaturesEXT{}.setMeshShader(true).setTaskShader(true),
            vk::PhysicalDeviceMaintenance4Features{}.setMaintenance4(true),
            vk::PhysicalDeviceSynchronization2Features{}.setSynchronization2(true),
        };
        return physical_device.createDevice(device_create_info.get<vk::DeviceCreateInfo>());
    }
    inline auto select_depth_image_tiling(vk::PhysicalDevice physical_device, vk::Format format) {
        vk::FormatProperties format_properties = physical_device.getFormatProperties(format);
        if (format_properties.linearTilingFeatures & vk::FormatFeatureFlagBits::eDepthStencilAttachment) {
            return vk::ImageTiling::eLinear;
        }
        else if (format_properties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eDepthStencilAttachment) {
            return vk::ImageTiling::eOptimal;
        }
        else {
            throw std::runtime_error{ "DepthStencilAttachment is not supported for this format" };
        }
    }
    inline auto create_image(vk::Device device, vk::ImageType type, vk::Format format, vk::Extent2D extent, vk::ImageTiling tiling, vk::ImageUsageFlags usages) {
        vk::ImageCreateInfo create_info{ {}, type, format, vk::Extent3D{extent, 1}, 1, 1, vk::SampleCountFlagBits::e1, tiling, usages };
        return device.createImage(create_info);
    }
    inline auto create_image(vk::Device device, vk::ImageType type, vk::Format format, vk::Extent2D extent, vk::ImageTiling tiling, vk::ImageUsageFlags usages, vk::ImageLayout layout) {
        auto create_info = vk::ImageCreateInfo{ {}, type, format, vk::Extent3D{extent, 1}, 1, 1, vk::SampleCountFlagBits::e1, tiling, usages }
        .setInitialLayout(layout);
        return device.createImage(create_info);
    }
    template<class T, class B>
    inline bool contain_bit(T bitfield, B bit) {
        return (bitfield & bit) == bit;
    }

    inline auto select_memory_type(vk::PhysicalDevice physical_device, vk::Device device, vk::MemoryRequirements requirements, vk::MemoryPropertyFlags properties) {
        auto memory_properties = physical_device.getMemoryProperties();
        auto type_bits = requirements.memoryTypeBits;
        return *std::ranges::find_if(from_0_count_n(memory_properties.memoryTypeCount), [&memory_properties, type_bits, properties](auto i) {
            if ((type_bits & (1 << i)) &&
                contain_bit(memory_properties.memoryTypes[i].propertyFlags, properties)) {
                return true;
            }
            return false;
            });
    }
    inline auto allocate_device_memory(vk::PhysicalDevice physical_device, vk::Device device, vk::Image image, vk::MemoryPropertyFlags properties) {

        auto memory_requirements = device.getImageMemoryRequirements(image);
        auto type_index = select_memory_type(physical_device, device, memory_requirements, properties);
        return std::tuple{ device.allocateMemory(vk::MemoryAllocateInfo{ memory_requirements.size, type_index }), memory_requirements.size };
    }
    inline auto allocate_device_memory(vk::PhysicalDevice physical_device, vk::Device device, vk::Buffer buffer, vk::MemoryPropertyFlags properties) {

        auto memory_requirements = device.getBufferMemoryRequirements(buffer);
        auto type_index = select_memory_type(physical_device, device, memory_requirements, properties);
        return std::tuple{ device.allocateMemory(vk::MemoryAllocateInfo{ memory_requirements.size, type_index }), memory_requirements.size };
    }
    inline auto create_image_view(vk::Device device, vk::Image image, vk::ImageViewType type, vk::Format format, vk::ImageAspectFlags aspect) {
        return device.createImageView(vk::ImageViewCreateInfo{ {}, image, type, format, {}, {aspect, 0, 1, 0, 1} });
    }
    inline auto create_depth_buffer(vk::PhysicalDevice physical_device, vk::Device device, vk::Format format, vk::Extent2D extent) {
        vk::ImageTiling tiling = select_depth_image_tiling(physical_device, format);
        auto image = create_image(device, vk::ImageType::e2D, format, extent, tiling, vk::ImageUsageFlagBits::eDepthStencilAttachment);
        auto [memory,memory_size] = allocate_device_memory(physical_device, device, image, vk::MemoryPropertyFlagBits::eDeviceLocal);
        device.bindImageMemory(image, memory, 0);
        auto image_view = create_image_view(device, image, vk::ImageViewType::e2D, format, vk::ImageAspectFlagBits::eDepth);
        return std::tuple{ image, memory, image_view };
    }
    template<class T>
    inline auto create_texture(vk::PhysicalDevice physical_device, vk::Device device, vk::Format format, uint32_t width, uint32_t height, T fun) {
        auto image = create_image(device, vk::ImageType::e2D, format, vk::Extent2D{ width, height }, vk::ImageTiling::eLinear, vk::ImageUsageFlagBits::eSampled, vk::ImageLayout::ePreinitialized);
        auto [memory, memory_size] = allocate_device_memory(physical_device, device, image, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
        device.bindImageMemory(image, memory, 0);
        {
            auto const subres = vk::ImageSubresource().setAspectMask(vk::ImageAspectFlagBits::eColor).setMipLevel(0).setArrayLayer(0);
            vk::SubresourceLayout layout;
            device.getImageSubresourceLayout(image, &subres, &layout);
            auto ptr = reinterpret_cast<char*>(device.mapMemory(memory, 0, memory_size));
            fun(ptr + layout.offset, layout.rowPitch);
        }
        device.unmapMemory(memory);
        auto image_view = create_image_view(device, image, vk::ImageViewType::e2D, format, vk::ImageAspectFlagBits::eColor);
        return std::tuple{ image, memory, image_view };
    }
    inline void set_image_layout(vk::CommandBuffer cmd,
        vk::Image image, vk::ImageAspectFlags aspectMask, vk::ImageLayout oldLayout, vk::ImageLayout newLayout,
        vk::AccessFlags srcAccessMask, vk::PipelineStageFlags src_stages, vk::PipelineStageFlags dest_stages) {
        auto DstAccessMask = [](vk::ImageLayout const& layout) {
            vk::AccessFlags flags;

            switch (layout) {
            case vk::ImageLayout::eTransferDstOptimal:
                // Make sure anything that was copying from this image has
                // completed
                flags = vk::AccessFlagBits::eTransferWrite;
                break;
            case vk::ImageLayout::eColorAttachmentOptimal:
                flags = vk::AccessFlagBits::eColorAttachmentWrite;
                break;
            case vk::ImageLayout::eDepthStencilAttachmentOptimal:
                flags = vk::AccessFlagBits::eDepthStencilAttachmentWrite;
                break;
            case vk::ImageLayout::eShaderReadOnlyOptimal:
                // Make sure any Copy or CPU writes to image are flushed
                flags = vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eInputAttachmentRead;
                break;
            case vk::ImageLayout::eTransferSrcOptimal:
                flags = vk::AccessFlagBits::eTransferRead;
                break;
            case vk::ImageLayout::ePresentSrcKHR:
                flags = vk::AccessFlagBits::eMemoryRead;
                break;
            default:
                break;
            }

            return flags;
            };

        cmd.pipelineBarrier(src_stages, dest_stages, vk::DependencyFlagBits(), {}, {},
            vk::ImageMemoryBarrier()
            .setSrcAccessMask(srcAccessMask)
            .setDstAccessMask(DstAccessMask(newLayout))
            .setOldLayout(oldLayout)
            .setNewLayout(newLayout)
            .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
            .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
            .setImage(image)
            .setSubresourceRange(vk::ImageSubresourceRange(aspectMask, 0, 1, 0, 1)));
    }
    inline auto create_buffer(vk::Device device, size_t size, vk::BufferUsageFlags usages) {
        return device.createBuffer(vk::BufferCreateInfo{ {}, size, usages });
    }
    template<class T>
    inline void copy_to_buffer(vk::Device device, vk::Buffer buffer, vk::DeviceMemory memory, T data) {
        auto memory_requirement = device.getBufferMemoryRequirements(buffer);
        using ele_type = std::remove_reference_t<decltype(*data.begin())>;
        auto* ptr = static_cast<ele_type*>(device.mapMemory(memory, 0, memory_requirement.size));
        int i = 0;
        for (auto ite = data.begin(); ite != data.end(); ++ite) {
            ptr[i++] = *ite;
        }
        device.unmapMemory(memory);
    }
    template<class T>
    inline auto create_uniform_buffer(vk::PhysicalDevice physical_device, vk::Device device, T mem) {
        auto buffer = create_buffer(device, mem.size()*sizeof(*std::begin(mem)), vk::BufferUsageFlagBits::eUniformBuffer);
        auto [memory,memory_size] = allocate_device_memory(physical_device, device, buffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
        device.bindBufferMemory(buffer, memory, 0);
        copy_to_buffer(device, buffer, memory, mem);
        return std::tuple{ buffer, memory, memory_size };
    }
    template<class T>
    inline auto create_vertex_buffer(vk::PhysicalDevice physical_device, vk::Device device, T vertices) {
        auto buffer = create_buffer(device, vertices.size()*sizeof(vertices[0]), vk::BufferUsageFlagBits::eVertexBuffer);
        auto [memory, memory_size] = allocate_device_memory(physical_device, device, buffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
        device.bindBufferMemory(buffer, memory, 0);
        copy_to_buffer(device, buffer, memory, vertices);
        return std::tuple{ buffer, memory, memory_size };
    }
    inline auto create_pipeline_layout(vk::Device device, vk::DescriptorSetLayout descriptor_set_layout) {
        return device.createPipelineLayout(vk::PipelineLayoutCreateInfo{}.setSetLayouts(descriptor_set_layout));
    }
    inline auto create_render_pass(vk::Device device, vk::Format colorFormat, vk::Format depthFormat) {
        std::array<vk::AttachmentDescription, 2> attachmentDescriptions;
        attachmentDescriptions[0] = vk::AttachmentDescription(vk::AttachmentDescriptionFlags(),
            colorFormat,
            vk::SampleCountFlagBits::e1,
            vk::AttachmentLoadOp::eClear,
            vk::AttachmentStoreOp::eStore,
            vk::AttachmentLoadOp::eDontCare,
            vk::AttachmentStoreOp::eDontCare,
            vk::ImageLayout::eUndefined,
            vk::ImageLayout::ePresentSrcKHR);
        attachmentDescriptions[1] = vk::AttachmentDescription(vk::AttachmentDescriptionFlags(),
            depthFormat,
            vk::SampleCountFlagBits::e1,
            vk::AttachmentLoadOp::eClear,
            vk::AttachmentStoreOp::eDontCare,
            vk::AttachmentLoadOp::eDontCare,
            vk::AttachmentStoreOp::eDontCare,
            vk::ImageLayout::eUndefined,
            vk::ImageLayout::eDepthStencilAttachmentOptimal);

        vk::AttachmentReference colorReference(0, vk::ImageLayout::eColorAttachmentOptimal);
        vk::AttachmentReference depthReference(1, vk::ImageLayout::eDepthStencilAttachmentOptimal);
        vk::SubpassDescription  subpass(vk::SubpassDescriptionFlags(), vk::PipelineBindPoint::eGraphics, {}, colorReference, {}, &depthReference);
        vk::PipelineStageFlags depth_buffer_stages = vk::PipelineStageFlagBits::eEarlyFragmentTests | vk::PipelineStageFlagBits::eLateFragmentTests;
        std::array<vk::SubpassDependency, 2> dependencies{
            vk::SubpassDependency()
            .setSrcSubpass(VK_SUBPASS_EXTERNAL)
            .setDstSubpass(0)
            .setSrcStageMask(depth_buffer_stages)
            .setDstStageMask(depth_buffer_stages)
            .setSrcAccessMask(vk::AccessFlagBits::eDepthStencilAttachmentWrite)
            .setDstAccessMask(vk::AccessFlagBits::eDepthStencilAttachmentRead | vk::AccessFlagBits::eDepthStencilAttachmentWrite)
            .setDependencyFlags(vk::DependencyFlags{}),
            vk::SubpassDependency()
            .setSrcSubpass(VK_SUBPASS_EXTERNAL)
            .setDstSubpass(0)
            .setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
            .setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
            .setSrcAccessMask(vk::AccessFlagBits{})
            .setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite | vk::AccessFlagBits::eColorAttachmentRead)
            .setDependencyFlags(vk::DependencyFlags{}),
        };

        vk::RenderPass renderPass = device.createRenderPass(vk::RenderPassCreateInfo(vk::RenderPassCreateFlags(), attachmentDescriptions, subpass).setDependencies(dependencies));
        return renderPass;
    }
    inline auto create_shader_module(vk::Device device, std::filesystem::path path) {
        spirv_file file{ path };
        std::span code{ file.data(), file.size() };
        return device.createShaderModuleUnique(vk::ShaderModuleCreateInfo{ {}, code });
    }
    inline auto create_framebuffer(vk::Device device, vk::RenderPass render_pass, std::vector<vk::ImageView> attachments, vk::Extent2D extent) {
        return device.createFramebuffer(vk::FramebufferCreateInfo{ {}, render_pass, attachments, extent.width, extent.height, 1 });
    }
    struct vertex_stage_info {
        std::filesystem::path shader_file_path;
        std::string entry_name;
        vk::VertexInputBindingDescription input_binding;
        std::vector<vk::VertexInputAttributeDescription> input_attributes;
    };
    struct task_stage_info {
        std::filesystem::path shader_file_path;
        std::string entry_name;
    };
    struct mesh_stage_info {
        std::filesystem::path shader_file_path;
        std::string entry_name;
        vk::SpecializationInfo specialization_info;
    };
    struct geometry_stage_info {
        std::filesystem::path shader_file_path;
        std::string entry_name;
    };
    inline auto create_pipeline(vk::Device device,
        task_stage_info task_stage_info,
        mesh_stage_info mesh_stage_info,
        std::filesystem::path fragment_shader,
        vk::RenderPass render_pass,
        vk::PipelineLayout layout) {
        auto task_shader_module = create_shader_module(device, task_stage_info.shader_file_path);
        auto mesh_shader_module = create_shader_module(device, mesh_stage_info.shader_file_path);
        auto fragment_shader_module = create_shader_module(device, fragment_shader);
        auto shader_stage_create_infos = std::array{
            vk::PipelineShaderStageCreateInfo{}
            .setStage(vk::ShaderStageFlagBits::eTaskEXT)
            .setModule(*task_shader_module)
            .setPName(task_stage_info.entry_name.c_str()),
            vk::PipelineShaderStageCreateInfo{{}, vk::ShaderStageFlagBits::eMeshEXT, *mesh_shader_module, mesh_stage_info.entry_name.c_str()}
            .setPSpecializationInfo(&mesh_stage_info.specialization_info),
            vk::PipelineShaderStageCreateInfo{{}, vk::ShaderStageFlagBits::eFragment, *fragment_shader_module, "main"},
        };
        vk::PipelineViewportStateCreateInfo viewport_state_create_info{ {}, 1, nullptr, 1, nullptr };
        vk::PipelineRasterizationStateCreateInfo rasterization_state_create_info{ {}, false, false, vk::PolygonMode::eFill, vk::CullModeFlagBits::eNone, vk::FrontFace::eClockwise, false, 0.0f, 0.0f, 0.0f, 1.0f };
        vk::PipelineMultisampleStateCreateInfo multisample_state_create_info{ {}, vk::SampleCountFlagBits::e1 };
        vk::StencilOpState stencil_op_state{ vk::StencilOp::eKeep, vk::StencilOp::eKeep, vk::StencilOp::eKeep, vk::CompareOp::eAlways };
        vk::PipelineDepthStencilStateCreateInfo depth_stencil_state_create_info{ {}, true, true, vk::CompareOp::eLessOrEqual, false, false, stencil_op_state, stencil_op_state };
        std::array<vk::PipelineColorBlendAttachmentState, 1> const color_blend_attachments = {
        vk::PipelineColorBlendAttachmentState()
        .setColorWriteMask(
            vk::ColorComponentFlagBits::eR |
            vk::ColorComponentFlagBits::eG |
            vk::ColorComponentFlagBits::eB |
            vk::ColorComponentFlagBits::eA)
            .setBlendEnable(true)
            .setColorBlendOp(vk::BlendOp::eAdd)
            .setAlphaBlendOp(vk::BlendOp::eAdd)
            .setSrcColorBlendFactor(vk::BlendFactor::eSrcAlpha)
            .setSrcAlphaBlendFactor(vk::BlendFactor::eOne)
            .setDstColorBlendFactor(vk::BlendFactor::eOneMinusSrcAlpha)
            .setDstAlphaBlendFactor(vk::BlendFactor::eZero)
        };
        vk::PipelineColorBlendStateCreateInfo color_blend_state_create_info{};
        color_blend_state_create_info.setAttachments(color_blend_attachments);
        std::array<vk::DynamicState, 2> dynamic_states = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };
        vk::PipelineDynamicStateCreateInfo dynamic_state_create_info{ vk::PipelineDynamicStateCreateFlags{}, dynamic_states };
        return device.createGraphicsPipeline(nullptr,
            vk::GraphicsPipelineCreateInfo{ {},
                shader_stage_create_infos ,nullptr, nullptr,
                nullptr, &viewport_state_create_info, &rasterization_state_create_info, &multisample_state_create_info,
                &depth_stencil_state_create_info, &color_blend_state_create_info, &dynamic_state_create_info, layout, render_pass });
    }
    inline auto create_pipeline(vk::Device device,
        mesh_stage_info mesh_stage_info, std::filesystem::path fragment_shader,
        vk::RenderPass render_pass,
        vk::PipelineLayout layout) {
        auto mesh_shader_module = create_shader_module(device, mesh_stage_info.shader_file_path);
        auto fragment_shader_module = create_shader_module(device, fragment_shader);
        auto shader_stage_create_infos = std::array{
            vk::PipelineShaderStageCreateInfo{{}, vk::ShaderStageFlagBits::eMeshEXT, *mesh_shader_module, mesh_stage_info.entry_name.c_str()},
            vk::PipelineShaderStageCreateInfo{{}, vk::ShaderStageFlagBits::eFragment, *fragment_shader_module, "main"},
        };
        vk::PipelineViewportStateCreateInfo viewport_state_create_info{ {}, 1, nullptr, 1, nullptr };
        vk::PipelineRasterizationStateCreateInfo rasterization_state_create_info{ {}, false, false, vk::PolygonMode::eFill, vk::CullModeFlagBits::eNone, vk::FrontFace::eClockwise, false, 0.0f, 0.0f, 0.0f, 1.0f };
        vk::PipelineMultisampleStateCreateInfo multisample_state_create_info{ {}, vk::SampleCountFlagBits::e1 };
        vk::StencilOpState stencil_op_state{ vk::StencilOp::eKeep, vk::StencilOp::eKeep, vk::StencilOp::eKeep, vk::CompareOp::eAlways };
        vk::PipelineDepthStencilStateCreateInfo depth_stencil_state_create_info{ {}, true, true, vk::CompareOp::eLessOrEqual, false, false, stencil_op_state, stencil_op_state };
        std::array<vk::PipelineColorBlendAttachmentState, 1> const color_blend_attachments = {
        vk::PipelineColorBlendAttachmentState().setColorWriteMask(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                                                              vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA) };
        vk::PipelineColorBlendStateCreateInfo color_blend_state_create_info{};
        color_blend_state_create_info.setAttachments(color_blend_attachments);
        std::array<vk::DynamicState, 2> dynamic_states = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };
        vk::PipelineDynamicStateCreateInfo dynamic_state_create_info{ vk::PipelineDynamicStateCreateFlags{}, dynamic_states };
        return device.createGraphicsPipeline(nullptr,
            vk::GraphicsPipelineCreateInfo{ {},
                shader_stage_create_infos ,nullptr, nullptr,
                nullptr, &viewport_state_create_info, &rasterization_state_create_info, &multisample_state_create_info,
                &depth_stencil_state_create_info, &color_blend_state_create_info, &dynamic_state_create_info, layout, render_pass });
    }

    inline auto create_pipeline(vk::Device device,
        vertex_stage_info vertex_stage, std::filesystem::path fragment_shader,
        vk::RenderPass render_pass,
        vk::PipelineLayout layout
    ) {
        auto vertex_shader_module = create_shader_module(device, vertex_stage.shader_file_path);
        auto fragment_shader_module = create_shader_module(device, fragment_shader);
        std::array<vk::PipelineShaderStageCreateInfo, 2> shader_stage_create_infos = {
            vk::PipelineShaderStageCreateInfo{{}, vk::ShaderStageFlagBits::eVertex, *vertex_shader_module, vertex_stage.entry_name.c_str()},
            vk::PipelineShaderStageCreateInfo{{}, vk::ShaderStageFlagBits::eFragment, *fragment_shader_module, "main"},
        };
        vk::PipelineVertexInputStateCreateInfo vertex_input_state_create_info{ {}, vertex_stage.input_binding, vertex_stage.input_attributes };
        vk::PipelineInputAssemblyStateCreateInfo input_assembly_state_create_info{ {}, vk::PrimitiveTopology::eTriangleList };
        vk::PipelineViewportStateCreateInfo viewport_state_create_info{ {}, 1, nullptr, 1, nullptr };
        vk::PipelineRasterizationStateCreateInfo rasterization_state_create_info{ {}, false, false, vk::PolygonMode::eFill, vk::CullModeFlagBits::eNone, vk::FrontFace::eClockwise, false, 0.0f, 0.0f, 0.0f, 1.0f };
        vk::PipelineMultisampleStateCreateInfo multisample_state_create_info{ {}, vk::SampleCountFlagBits::e1 };
        vk::StencilOpState stencil_op_state{ vk::StencilOp::eKeep, vk::StencilOp::eKeep, vk::StencilOp::eKeep, vk::CompareOp::eAlways };
        vk::PipelineDepthStencilStateCreateInfo depth_stencil_state_create_info{ {}, true, true, vk::CompareOp::eLessOrEqual, false, false, stencil_op_state, stencil_op_state };
        std::array<vk::PipelineColorBlendAttachmentState, 1> const color_blend_attachments = {
        vk::PipelineColorBlendAttachmentState().setColorWriteMask(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                                                              vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA) };
        vk::PipelineColorBlendStateCreateInfo color_blend_state_create_info{};
        color_blend_state_create_info.setAttachments(color_blend_attachments);
        std::array<vk::DynamicState, 2> dynamic_states = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };
        vk::PipelineDynamicStateCreateInfo dynamic_state_create_info{ vk::PipelineDynamicStateCreateFlags{}, dynamic_states };
        return device.createGraphicsPipeline(nullptr, 
            vk::GraphicsPipelineCreateInfo{ {}, 
                shader_stage_create_infos ,&vertex_input_state_create_info, &input_assembly_state_create_info,
                nullptr, &viewport_state_create_info, &rasterization_state_create_info, &multisample_state_create_info, 
                &depth_stencil_state_create_info, &color_blend_state_create_info, &dynamic_state_create_info, layout, render_pass });
    }

    inline auto create_pipeline(vk::Device device,
        vertex_stage_info vertex_stage,
        geometry_stage_info geometry_stage,
        std::filesystem::path fragment_shader,
        vk::RenderPass render_pass,
        vk::PipelineLayout layout
    ) {
        auto vertex_shader_module = create_shader_module(device, vertex_stage.shader_file_path);
        auto geometry_shader_module = create_shader_module(device, geometry_stage.shader_file_path);
        auto fragment_shader_module = create_shader_module(device, fragment_shader);
        std::array<vk::PipelineShaderStageCreateInfo, 3> shader_stage_create_infos = {
            vk::PipelineShaderStageCreateInfo{{}, vk::ShaderStageFlagBits::eVertex, *vertex_shader_module, vertex_stage.entry_name.c_str()},
            vk::PipelineShaderStageCreateInfo{{}, vk::ShaderStageFlagBits::eGeometry, *geometry_shader_module, geometry_stage.entry_name.c_str()},
            vk::PipelineShaderStageCreateInfo{{}, vk::ShaderStageFlagBits::eFragment, *fragment_shader_module, "main"},
        };
        vk::PipelineVertexInputStateCreateInfo vertex_input_state_create_info{ {}, vertex_stage.input_binding, vertex_stage.input_attributes };
        vk::PipelineInputAssemblyStateCreateInfo input_assembly_state_create_info{ {}, vk::PrimitiveTopology::eTriangleList };
        vk::PipelineViewportStateCreateInfo viewport_state_create_info{ {}, 1, nullptr, 1, nullptr };
        vk::PipelineRasterizationStateCreateInfo rasterization_state_create_info{ {}, false, false, vk::PolygonMode::eFill, vk::CullModeFlagBits::eNone, vk::FrontFace::eClockwise, false, 0.0f, 0.0f, 0.0f, 1.0f };
        vk::PipelineMultisampleStateCreateInfo multisample_state_create_info{ {}, vk::SampleCountFlagBits::e1 };
        vk::StencilOpState stencil_op_state{ vk::StencilOp::eKeep, vk::StencilOp::eKeep, vk::StencilOp::eKeep, vk::CompareOp::eAlways };
        vk::PipelineDepthStencilStateCreateInfo depth_stencil_state_create_info{ {}, true, true, vk::CompareOp::eLessOrEqual, false, false, stencil_op_state, stencil_op_state };
        std::array<vk::PipelineColorBlendAttachmentState, 1> const color_blend_attachments = {
        vk::PipelineColorBlendAttachmentState().setColorWriteMask(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                                                              vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA) };
        vk::PipelineColorBlendStateCreateInfo color_blend_state_create_info{};
        color_blend_state_create_info.setAttachments(color_blend_attachments);
        std::array<vk::DynamicState, 2> dynamic_states = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };
        vk::PipelineDynamicStateCreateInfo dynamic_state_create_info{ vk::PipelineDynamicStateCreateFlags{}, dynamic_states };
        return device.createGraphicsPipeline(nullptr, 
            vk::GraphicsPipelineCreateInfo{ {}, 
                shader_stage_create_infos ,&vertex_input_state_create_info, &input_assembly_state_create_info,
                nullptr, &viewport_state_create_info, &rasterization_state_create_info, &multisample_state_create_info, 
                &depth_stencil_state_create_info, &color_blend_state_create_info, &dynamic_state_create_info, layout, render_pass });
    }
    inline auto create_swapchain(vk::PhysicalDevice physical_device, vk::Device device, vk::SurfaceKHR surface, auto surfaceCapabilities, vk::Format format) {
        vk::Extent2D swapchainExtent = surfaceCapabilities.currentExtent;
        assert(swapchainExtent.width != UINT32_MAX && swapchainExtent.height != UINT32_MAX);
        uint32_t min_image_count = std::max(surfaceCapabilities.maxImageCount,surfaceCapabilities.minImageCount);

        vk::PresentModeKHR swapchainPresentMode = vk::PresentModeKHR::eFifo;

        vk::SurfaceTransformFlagBitsKHR preTransform = (surfaceCapabilities.supportedTransforms & vk::SurfaceTransformFlagBitsKHR::eIdentity)
            ? vk::SurfaceTransformFlagBitsKHR::eIdentity
            : surfaceCapabilities.currentTransform;

        vk::CompositeAlphaFlagBitsKHR compositeAlpha =
            (surfaceCapabilities.supportedCompositeAlpha & vk::CompositeAlphaFlagBitsKHR::ePreMultiplied) ? vk::CompositeAlphaFlagBitsKHR::ePreMultiplied
            : (surfaceCapabilities.supportedCompositeAlpha & vk::CompositeAlphaFlagBitsKHR::ePostMultiplied) ? vk::CompositeAlphaFlagBitsKHR::ePostMultiplied
            : (surfaceCapabilities.supportedCompositeAlpha & vk::CompositeAlphaFlagBitsKHR::eInherit) ? vk::CompositeAlphaFlagBitsKHR::eInherit :
            vk::CompositeAlphaFlagBitsKHR::eOpaque;

        vk::SwapchainCreateInfoKHR swapchainCreateInfo(vk::SwapchainCreateFlagsKHR(),
            surface,
            min_image_count,
            format,
            vk::ColorSpaceKHR::eSrgbNonlinear,
            swapchainExtent,
            1,
            vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc,
            vk::SharingMode::eExclusive,
            {},
            preTransform,
            compositeAlpha,
            swapchainPresentMode,
            true,
            nullptr);
        assert(swapchainCreateInfo.minImageCount >= surfaceCapabilities.minImageCount);
        return device.createSwapchainKHR(swapchainCreateInfo);
    }
    // reuse_semaphore will be used again, so we need a fence to notify cpu that it is not used by gpu.
    struct reuse_semaphore {
        vk::Fence fence;
        vk::Semaphore semaphore;
    };
    class present_manager {
    public:
        present_manager(vk::SharedDevice device, uint32_t count) : m_device{ device }, next_semaphore_index{ 0 } {
            for (int i = 0; i < count; i++) {
                m_semaphores.push_back(reuse_semaphore{ m_device->createFence(vk::FenceCreateInfo{vk::FenceCreateFlagBits::eSignaled}), m_device->createSemaphore(vk::SemaphoreCreateInfo{}) });
            }
        }
        auto get_next() {
            auto [fence, semaphore] = m_semaphores[next_semaphore_index++];
            if (next_semaphore_index >= m_semaphores.size()) {
                next_semaphore_index = 0;
            }
            auto res = m_device->waitForFences(std::array<vk::Fence, 1>{fence}, true, UINT64_MAX);
            assert(res == vk::Result::eSuccess);
            m_device->resetFences(std::array<vk::Fence, 1>{fence});
            return reuse_semaphore{ fence, semaphore };
        }
        void wait_all() {
            std::ranges::for_each(m_semaphores, [&m_device=m_device](auto reuse_semaphore) {
                auto res = m_device->waitForFences(std::array<vk::Fence, 1>{reuse_semaphore.fence}, true, UINT64_MAX);
                assert(res == vk::Result::eSuccess);
                });
        }
        ~present_manager() {
            wait_all();
            std::ranges::for_each(m_semaphores, [&m_device = m_device](auto reuse_semaphore) {
                m_device->destroyFence(reuse_semaphore.fence);
                m_device->destroySemaphore(reuse_semaphore.semaphore);
                });
        }
    private:
        vk::SharedDevice m_device;
        std::vector<reuse_semaphore> m_semaphores;
        uint32_t next_semaphore_index;
    };
    namespace shared {
        inline auto create_instance() {
            return vk::SharedInstance{ vulkan::create_instance() };
        }
        inline auto select_physical_device(vk::SharedInstance instance) {
            return vk::SharedPhysicalDevice{ vulkan::select_physical_device(*instance), instance };
        }
        inline auto select_queue_family(vk::SharedPhysicalDevice physical_device) {
            return vulkan::select_queue_family(*physical_device);
        }
        inline auto create_device(vk::SharedPhysicalDevice physical_device, uint32_t graphics_queue_family_index) {
            return vk::SharedDevice{ vulkan::create_device(*physical_device, graphics_queue_family_index)};
        }
    }
}

class simple_draw_command {
public:
    simple_draw_command(
        vk::CommandBuffer cmd,
        vk::RenderPass render_pass,
        vk::PipelineLayout pipeline_layout,
        vk::Pipeline pipeline,
        vk::DescriptorSet descriptor_set,
        vk::Framebuffer framebuffer,
        vk::Extent2D swapchain_extent,
        vk::DispatchLoaderDynamic dldid)
        : m_cmd{ cmd } {
        vk::CommandBufferBeginInfo begin_info{ vk::CommandBufferUsageFlagBits::eSimultaneousUse };
        cmd.begin(begin_info);
        std::array<vk::ClearValue, 2> clear_values;
        clear_values[0].color = vk::ClearColorValue{ 1.0f, 1.0f,1.0f,1.0f };
        clear_values[1].depthStencil = vk::ClearDepthStencilValue{ 1.0f, 0 };
        vk::RenderPassBeginInfo render_pass_begin_info{
            render_pass, framebuffer,
            vk::Rect2D{vk::Offset2D{0,0},
            swapchain_extent}, clear_values };
        cmd.beginRenderPass(render_pass_begin_info, vk::SubpassContents::eInline);
        cmd.bindPipeline(vk::PipelineBindPoint::eGraphics,
            pipeline);
        cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
            pipeline_layout, 0, descriptor_set, nullptr);
        //cmd.bindVertexBuffers(0, *vertex_buffer, { 0 });
        cmd.setViewport(0, vk::Viewport(0, 0, swapchain_extent.width, swapchain_extent.height, 0, 1));
        cmd.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), swapchain_extent));
        //cmd.draw(3, 1, 0, 0);
        cmd.drawMeshTasksEXT(1, 1, 1, dldid);
        cmd.endRenderPass();
        cmd.end();
    }
    auto get_command_buffer() {
        return m_cmd;
    }
private:
    vk::CommandBuffer m_cmd;
};


class fix_instance_destroy {
public:
    fix_instance_destroy()
        : instance{ vulkan::shared::create_instance() }
    {}
protected:
    vk::SharedInstance instance;
};

class vulkan_render_prepare : fix_instance_destroy {
public:
    auto select_physical_device(auto instance) {
        return vulkan::shared::select_physical_device(instance);
    }
    auto select_queue_family(auto physical_device) {
        return vulkan::shared::select_queue_family(physical_device);
    }
    auto create_device(auto physical_device, auto queue_family_index) {
        return vk::SharedDevice{ vulkan::shared::create_device(physical_device, queue_family_index) };
    }
    auto get_queue(auto device, auto queue_family_index) {
        return vk::SharedQueue{ device->getQueue(queue_family_index, 0), device };
    }
    auto create_command_pool(auto device, auto queue_family_index) {
        vk::CommandPoolCreateInfo commandPoolCreateInfo(vk::CommandPoolCreateFlagBits::eResetCommandBuffer, queue_family_index);
        return vk::SharedCommandPool(device->createCommandPool(commandPoolCreateInfo), device);
    }
    auto get_surface(auto instance, auto&& get_surface_from_extern) {
        vk::SurfaceKHR vk_surface = get_surface_from_extern(*instance);
        return vk::SharedSurfaceKHR{ vk_surface, instance };
    }
    auto select_color_format(auto physical_device, auto surface) {
        std::vector<vk::SurfaceFormatKHR> formats = physical_device->getSurfaceFormatsKHR(*surface);
        return (formats[0].format == vk::Format::eUndefined) ? vk::Format::eR8G8B8A8Unorm : formats[0].format;
    }
    auto select_depth_format() {
        return vk::Format::eD16Unorm;
    }
    auto get_surface_capabilities(auto physical_device, auto surface) {
        return physical_device->getSurfaceCapabilitiesKHR(*surface);
    }
    auto get_surface_extent(auto surface_capabilities) {
        return surface_capabilities.currentExtent;
    }
    auto create_swapchain(
        auto physical_device,
        auto device,
        auto surface,
        auto surface_capabilities,
        auto color_format) {
        return vk::SharedSwapchainKHR(
            vulkan::create_swapchain(
                *physical_device,
                *device,
                *surface,
                surface_capabilities,
                color_format),
            device,
            surface);
    }
    auto create_render_pass(auto device, auto color_format, auto depth_format) {
        return vk::SharedRenderPass{ vulkan::create_render_pass(*device, color_format, depth_format), device };
    }
    auto create_descriptor_set_bindings() {
        return std::array{
            vk::DescriptorSetLayoutBinding{}
            .setBinding(0)
            .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
            .setStageFlags(vk::ShaderStageFlagBits::eFragment)
            .setDescriptorCount(1),
            vk::DescriptorSetLayoutBinding{}
            .setBinding(1)
            .setDescriptorType(vk::DescriptorType::eUniformBuffer)
            .setStageFlags(vk::ShaderStageFlagBits::eMeshEXT)
            .setDescriptorCount(1),
        };
    }
    auto create_descriptor_set_layout(auto device, auto descriptor_set_bindings) {
        return device->createDescriptorSetLayoutUnique(
            vk::DescriptorSetLayoutCreateInfo{}
            .setBindings(descriptor_set_bindings));
    }
    auto create_pipeline_layout(auto device, auto& descriptor_set_layout) {
        return vk::SharedPipelineLayout{
            vulkan::create_pipeline_layout(*device, *descriptor_set_layout),
            device };
    }
    auto create_descriptor_pool(auto device, auto descriptor_pool_size) {
        return device->createDescriptorPoolUnique(vk::DescriptorPoolCreateInfo{}.setPoolSizes(descriptor_pool_size).setMaxSets(1));
    }
    auto get_descriptor_pool_size() {
        return std::array{
            vk::DescriptorPoolSize{}.setType(vk::DescriptorType::eCombinedImageSampler).setDescriptorCount(1),
            vk::DescriptorPoolSize{}.setType(vk::DescriptorType::eUniformBuffer).setDescriptorCount(1),
        };
    }
    auto allocate_descriptor_set(auto device, auto& descriptor_set_layout) {
        return std::move(
            device->allocateDescriptorSets(
                vk::DescriptorSetAllocateInfo{}
                .setDescriptorPool(*descriptor_pool)
                .setSetLayouts(*descriptor_set_layout))
            .front()
            );
    }
    auto create_pipeline(auto device, auto render_pass, auto pipeline_layout, auto& characters) {
        vulkan::task_stage_info task_stage_info{
            task_shader_path, "main",
        };
        class char_count_specialization {
        public:
            char_count_specialization(std::vector<char>& characters)
                :
                m_count{ static_cast<int>(characters.size()) },
                map_entry{ vk::SpecializationMapEntry{}.setConstantID(555).setOffset(0).setSize(sizeof(m_count)) },
                specialization_info{ vk::SpecializationInfo{}
                .setMapEntries(map_entry).setDataSize(sizeof(m_count)).setPData(&this->m_count) }
            {
            }
        public:
            int m_count;
            vk::SpecializationMapEntry map_entry;
            vk::SpecializationInfo specialization_info;
        };
        char_count_specialization specialization{
            characters
        };

        vulkan::mesh_stage_info mesh_stage_info{
            mesh_shader_path, "main", specialization.specialization_info
        };
        vulkan::geometry_stage_info geometry_stage_info{
            geometry_shader_path, "main",
        };
        return vk::SharedPipeline{
            vulkan::create_pipeline(*device,
                    task_stage_info,
                    mesh_stage_info,
                    fragment_shader_path, *render_pass, *pipeline_layout).value, device };
    }
    auto create_font_texture(auto physical_device, auto device, auto characters) {
        font_loader font_loader{};
        uint32_t font_width = 32, font_height = 32;
        uint32_t line_height = font_height * 2;
        font_loader.set_char_size(font_width, font_height);
        auto [vk_texture, vk_texture_memory, vk_texture_view] =
            vulkan::create_texture(*physical_device, *device,
                vk::Format::eR8Unorm,
                font_width * std::size(characters), line_height,
                [&font_loader, font_width, font_height, line_height, &characters](char* ptr, int pitch) {
                    std::ranges::for_each(from_0_count_n(characters.size()),
                    [&font_loader, font_width, font_height, line_height, &characters, ptr, pitch](auto i) {
                            font_loader.render_char(characters[i]);
                            auto glyph = font_loader.get_glyph();
                            uint32_t start_row = font_height - glyph->bitmap_top - 1;
                            assert(start_row + glyph->bitmap.rows < line_height);
                            for (int row = 0; row < glyph->bitmap.rows; row++) {
                                for (int x = 0; x < glyph->bitmap.width; x++) {
                                    ptr[(start_row + row) * pitch + glyph->bitmap_left + i * font_width + x] =
                                        glyph->bitmap.buffer[row * glyph->bitmap.pitch + x];
                                }
                            }
                        });
                });
        auto texture = vk::SharedImage{
            vk_texture, device };
        auto texture_memory = vk::SharedDeviceMemory{ vk_texture_memory, device };
        auto texture_view = vk::SharedImageView{ vk_texture_view, device };
        return std::tuple{ texture, texture_memory, texture_view };
    }
    void update_descriptor_set(auto descriptor_set, auto texture_view, auto& sampler, auto& char_indices_buffer) {
        auto texture_image_info =
            vk::DescriptorImageInfo{}
            .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
            .setImageView(*texture_view)
            .setSampler(*sampler);
        auto char_texture_indices_info =
            vk::DescriptorBufferInfo{}
            .setBuffer(*char_indices_buffer)
            .setOffset(0)
            .setRange(vk::WholeSize);
        auto descriptor_set_write = std::array{
            vk::WriteDescriptorSet{}
            .setDstBinding(0)
            .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
            .setImageInfo(texture_image_info)
            .setDstSet(descriptor_set),
            vk::WriteDescriptorSet{}
            .setDstBinding(1)
            .setDescriptorType(vk::DescriptorType::eUniformBuffer)
            .setBufferInfo(char_texture_indices_info)
            .setDstSet(descriptor_set),
        };
        device->updateDescriptorSets(descriptor_set_write, nullptr);
    }
    void create_and_update_terminal_buffer_relate_data(
        auto descriptor_set, auto& sampler, auto& terminal_buffer,
        auto& imageViews, auto& command_buffers) {
        auto generate_char_set = [](auto& terminal_buffer) {
            std::set<char> char_set{};
            std::for_each(
                terminal_buffer.begin(),
                terminal_buffer.end(),
                [&char_set](auto c) {
                    char_set.emplace(c);
                });
            return char_set;
            };
        auto generate_characters = [generate_char_set](auto& terminal_buffer) {
            auto char_set = generate_char_set(terminal_buffer);
            std::vector<char> characters{};
            std::ranges::copy(char_set, std::back_inserter(characters));
            return characters;
            };
        auto characters = generate_characters(terminal_buffer);
        assert(characters.size() != 0);
        auto generate_char_texture_indices = [](auto& characters) {
            std::map<char, int> char_texture_indices;
            std::ranges::for_each(from_0_count_n(characters.size()), [&characters, &char_texture_indices](auto i) {
                char_texture_indices.emplace(characters[i], static_cast<int>(i));
                });
            return char_texture_indices;
            };
        auto char_texture_indices{
            generate_char_texture_indices(characters)
        };
        auto generate_char_indices_buf{
            [](auto& terminal_buffer, auto& char_texture_indices) {
                multidimention_array<int, 32, 32> char_indices_buf{};
                std::transform(
                    terminal_buffer.begin(),
                    terminal_buffer.end(),
                    char_indices_buf.begin(),
                    [&char_texture_indices](auto c) {
                        return char_texture_indices[c];
                    });
                return char_indices_buf;
            }
        };
        auto char_indices_buf{
            generate_char_indices_buf(terminal_buffer, char_texture_indices)
        };
        auto descriptor_set_bindings{
            create_descriptor_set_bindings()
        };
        vk::UniqueDescriptorSetLayout descriptor_set_layout{
            create_descriptor_set_layout(device, descriptor_set_bindings)
        };
        vk::SharedPipelineLayout pipeline_layout{
            create_pipeline_layout(device, descriptor_set_layout)
        };
        pipeline = create_pipeline(device, render_pass, pipeline_layout, characters);
        std::tie(texture, texture_memory, texture_view) = create_font_texture(physical_device, device, characters);
        {
            auto [vk_char_indices_buffer, vk_char_indices_buffer_memory, vk_char_indices_buffer_size] =
                vulkan::create_uniform_buffer(*physical_device, *device,
                    char_indices_buf);
            char_indices_buffer = vk::SharedBuffer{ vk_char_indices_buffer, device };
            char_indices_buffer_memory = vk::SharedDeviceMemory{ vk_char_indices_buffer_memory, device };
        }
        update_descriptor_set(descriptor_set, texture_view, sampler, char_indices_buffer);
        vk::DispatchLoaderDynamic dldid(*instance, vkGetInstanceProcAddr, *device);
        for (integer_less_equal<decltype(imageViews.size())> i{ 0, imageViews.size() }; i < imageViews.size(); i++) {
            simple_draw_command draw_command{
                command_buffers[i],
                *render_pass,
                *pipeline_layout,
                *pipeline,
                descriptor_set,
                *framebuffers[i],
                swapchain_extent, dldid };
        }
    }
    void init(auto&& get_surface_from_extern, auto& terminal_buffer) {
        p_terminal_buffer = &terminal_buffer;
        physical_device = {
            select_physical_device(instance)
        };
        auto queue_family_index{
            select_queue_family(physical_device)
        };
        device = create_device(physical_device, queue_family_index);
        queue = get_queue(device, queue_family_index);
        command_pool = create_command_pool(device, queue_family_index);
        vk::SharedSurfaceKHR surface{
            get_surface(instance, get_surface_from_extern)
        };
        vk::Format color_format{
            select_color_format(physical_device, surface)
        };
        vk::Format depth_format{
            select_depth_format()
        };
        auto surface_capabilities{
            get_surface_capabilities(physical_device, surface)
        };
        swapchain = {
            create_swapchain(physical_device, device, surface, surface_capabilities, color_format)
        };
        swapchain_extent = {
            get_surface_extent(surface_capabilities)
        };
        std::vector<vk::Image> swapchainImages = device->getSwapchainImagesKHR(*swapchain);
        imageViews.reserve(swapchainImages.size());
        vk::ImageViewCreateInfo imageViewCreateInfo({}, {},
            vk::ImageViewType::e2D, color_format, {}, { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
        render_pass = create_render_pass(device, color_format, depth_format);
        for (auto image : swapchainImages) {
            imageViewCreateInfo.image = image;
            auto image_view = device->createImageView(imageViewCreateInfo);
            imageViews.emplace_back(vk::SharedImageView{ image_view, device });
            render_complete_semaphores.push_back(device->createSemaphoreUnique(vk::SemaphoreCreateInfo{}));

            auto [depth_buffer, depth_buffer_memory, depth_buffer_view] =
                vulkan::create_depth_buffer(*physical_device, *device, depth_format, swapchain_extent);
            depth_buffers.emplace_back(vk::SharedImage{ depth_buffer, device });
            depth_buffer_memories.emplace_back(vk::SharedDeviceMemory{ depth_buffer_memory, device });
            depth_buffer_views.emplace_back(vk::SharedImageView{ depth_buffer_view, device });
            framebuffers.emplace_back(
                vk::SharedFramebuffer{
                vulkan::create_framebuffer(*device, *render_pass,
                        std::vector{image_view, depth_buffer_view}, swapchain_extent), device
                });
        }
        auto descriptor_set_bindings{
            create_descriptor_set_bindings()
        };
        descriptor_set_layout = {
            create_descriptor_set_layout(device, descriptor_set_bindings)
        };
        vk::SharedPipelineLayout pipeline_layout{
            create_pipeline_layout(device, descriptor_set_layout)
        };
        auto descriptor_pool_size{
            get_descriptor_pool_size()
        };
        descriptor_pool = create_descriptor_pool(device, descriptor_pool_size);
        descriptor_set = allocate_descriptor_set(device, descriptor_set_layout);
        sampler = device->createSamplerUnique(vk::SamplerCreateInfo{});

        vk::CommandBufferAllocateInfo commandBufferAllocateInfo(
            *command_pool, vk::CommandBufferLevel::ePrimary, imageViews.size());
        command_buffers = device->allocateCommandBuffers(commandBufferAllocateInfo);
        create_and_update_terminal_buffer_relate_data(
            descriptor_set, sampler, terminal_buffer, imageViews, command_buffers);

        // There should be some commands wait for texture load semaphore.
        // There will be a BUG, fix it!
    }
    void notify_update() {
        create_and_update_terminal_buffer_relate_data(descriptor_set, sampler, *p_terminal_buffer,
            imageViews, command_buffers);
    }

protected:
    multidimention_array<char, 32, 16>* p_terminal_buffer;
    vk::SharedPhysicalDevice physical_device;
    vk::SharedDevice device;
    vk::SharedCommandPool command_pool;
    vk::SharedSwapchainKHR swapchain;
    vk::UniqueDescriptorPool descriptor_pool;
    vk::UniqueDescriptorSetLayout descriptor_set_layout;
    vk::DescriptorSet descriptor_set;
    vk::SharedPipeline pipeline;
    vk::SharedRenderPass render_pass;
    vk::Extent2D swapchain_extent;

    vk::SharedImage texture;
    vk::SharedImageView texture_view;
    vk::SharedDeviceMemory texture_memory;
    vk::SharedBuffer char_indices_buffer;
    vk::SharedDeviceMemory char_indices_buffer_memory;
    vk::UniqueSampler sampler;
    std::vector<vk::SharedImageView> imageViews;
    std::vector<vk::UniqueSemaphore> render_complete_semaphores;
    std::vector<vk::SharedImage> depth_buffers;
    std::vector<vk::SharedImageView> depth_buffer_views;
    std::vector<vk::SharedDeviceMemory> depth_buffer_memories;
    std::vector<vk::SharedFramebuffer> framebuffers;
    std::vector<vk::CommandBuffer> command_buffers;
    vk::SharedQueue queue;
};

class vulkan_render : public vulkan_render_prepare {
public:
    void set_texture_image_layout() {
        auto texture_prepare_semaphore = present_manager->get_next();
        {
            vk::CommandBuffer init_command_buffer{ device->allocateCommandBuffers(vk::CommandBufferAllocateInfo{}.setCommandBufferCount(1).setCommandPool(*command_pool)).front() };
            init_command_buffer.begin(vk::CommandBufferBeginInfo{});
            vulkan::set_image_layout(init_command_buffer, *texture, vk::ImageAspectFlagBits::eColor, vk::ImageLayout::ePreinitialized,
                vk::ImageLayout::eShaderReadOnlyOptimal, vk::AccessFlagBits(), vk::PipelineStageFlagBits::eTopOfPipe,
                vk::PipelineStageFlagBits::eFragmentShader);
            auto cmd = init_command_buffer;
            cmd.end();
            auto signal_semaphore = texture_prepare_semaphore.semaphore;
            auto command_submit_info = vk::CommandBufferSubmitInfo{}.setCommandBuffer(cmd);
            auto signal_semaphore_info = vk::SemaphoreSubmitInfo{}.setSemaphore(signal_semaphore).setStageMask(vk::PipelineStageFlagBits2::eTransfer);
            queue->submit2(vk::SubmitInfo2{}.setCommandBufferInfos(command_submit_info).setSignalSemaphoreInfos(signal_semaphore_info));
            queue->submit2(vk::SubmitInfo2{}.setWaitSemaphoreInfos(signal_semaphore_info), texture_prepare_semaphore.fence);
        }
    }
    void init(auto&& get_surface, auto& terminal_buffer) {
        vulkan_render_prepare::init(std::forward<decltype(get_surface)>(get_surface),
            terminal_buffer);
        present_manager = std::make_shared<vulkan::present_manager>(device, 10);
        set_texture_image_layout();
    }
    run_result run();
    void notify_update() {
        present_manager->wait_all();
        vulkan_render_prepare::notify_update();
        set_texture_image_layout();
    }
private:
    std::shared_ptr<vulkan::present_manager> present_manager;
};
