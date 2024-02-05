#pragma once
#include <vulkan/vulkan_raii.hpp>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_shared.hpp>
#include <GLFW/glfw3.h>
#include <ft2build.h>
#include FT_FREETYPE_H

#include <iostream>
#include <cassert>
#include <array>
#include <span>
#include <algorithm>
#include <numeric>
#include <ranges>
#include <filesystem>

#define max max
#include "spirv_reader.hpp"

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

namespace terminal {
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
	inline auto create_window_and_get_surface(vk::Instance instance, uint32_t width, uint32_t height) {
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		assert(0 != glfwVulkanSupported());
		GLFWwindow* window = glfwCreateWindow(width, height, "Terminal Emulator", nullptr, nullptr);

		VkSurfaceKHR surface;
		glfwCreateWindowSurface(instance, window, nullptr, &surface);
		return std::pair{ window, surface };
	}
	inline auto create_device(vk::PhysicalDevice physical_device, uint32_t graphicsQueueFamilyIndex) {
		float queuePriority = 0.0f;
		vk::DeviceQueueCreateInfo deviceQueueCreateInfo({}, graphicsQueueFamilyIndex, 1, &queuePriority);
		std::array<const char*, 2> deviceExtensions{
			"VK_KHR_swapchain",
			"VK_EXT_mesh_shader",
		};
		vk::StructureChain<vk::DeviceCreateInfo, vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceMeshShaderFeaturesEXT, vk::PhysicalDeviceMaintenance4Features> device_create_info{
			vk::DeviceCreateInfo{}.setQueueCreateInfos(deviceQueueCreateInfo).setPEnabledExtensionNames(deviceExtensions),
			vk::PhysicalDeviceFeatures2{},
			vk::PhysicalDeviceMeshShaderFeaturesEXT{}.setMeshShader(true),
			vk::PhysicalDeviceMaintenance4Features{}.setMaintenance4(true),
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
	inline auto create_depth_buffer(vk::PhysicalDevice physical_device, vk::Device device, vk::Format format, uint32_t width, uint32_t height) {
		vk::ImageTiling tiling = select_depth_image_tiling(physical_device, format);
		auto image = create_image(device, vk::ImageType::e2D, format, vk::Extent2D{ width, height }, tiling, vk::ImageUsageFlagBits::eDepthStencilAttachment);
		auto [memory,memory_size] = allocate_device_memory(physical_device, device, image, vk::MemoryPropertyFlagBits::eDeviceLocal);
		device.bindImageMemory(image, memory, 0);
		auto image_view = create_image_view(device, image, vk::ImageViewType::e2D, format, vk::ImageAspectFlagBits::eDepth);
		return std::tuple{ image, memory, image_view };
	}
	template<class T>
	inline auto create_texture(vk::PhysicalDevice physical_device, vk::Device device, vk::Format format, uint32_t width, uint32_t height, std::span<T> data) {
		auto image = create_image(device, vk::ImageType::e2D, format, vk::Extent2D{ width, height }, vk::ImageTiling::eLinear, vk::ImageUsageFlagBits::eSampled, vk::ImageLayout::ePreinitialized);
		auto [memory, memory_size] = allocate_device_memory(physical_device, device, image, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
		device.bindImageMemory(image, memory, 0);
		{
			auto const subres = vk::ImageSubresource().setAspectMask(vk::ImageAspectFlagBits::eColor).setMipLevel(0).setArrayLayer(0);
			vk::SubresourceLayout layout;
			device.getImageSubresourceLayout(image, &subres, &layout);
			auto ptr = reinterpret_cast<char*>(device.mapMemory(memory, 0, memory_size));
			for (int i = 0; i < height; i++) {
				memcpy(&ptr[layout.offset + i * layout.rowPitch], data.data() + i * width, width);
			}
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
		auto* ptr = static_cast<uint8_t*>(device.mapMemory(memory, 0, memory_requirement.size));
		std::memcpy(ptr, data.data(), data.size()*sizeof(data[0]));
		device.unmapMemory(memory);
	}
	inline auto create_uniform_buffer(vk::PhysicalDevice physical_device, vk::Device device, std::span<char> mem) {
		auto buffer = create_buffer(device, mem.size(), vk::BufferUsageFlagBits::eUniformBuffer);
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
	struct mesh_stage_info {
		std::filesystem::path shader_file_path;
		std::string entry_name;
	};
	inline auto create_pipeline(vk::Device device,
		mesh_stage_info mesh_stage_info, std::filesystem::path fragment_shader,
		vk::RenderPass render_pass,
		vk::PipelineLayout layout) {
		auto mesh_shader_module = create_shader_module(device, mesh_stage_info.shader_file_path);
		auto fragment_shader_module = create_shader_module(device, fragment_shader);
		std::array<vk::PipelineShaderStageCreateInfo, 2> shader_stage_create_infos = {
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
	struct vertex_stage_info {
		std::filesystem::path shader_file_path;
		std::string entry_name;
		vk::VertexInputBindingDescription input_binding;
		std::vector<vk::VertexInputAttributeDescription> input_attributes;
	};
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
    struct geometry_stage_info {
		std::filesystem::path shader_file_path;
		std::string entry_name;
    };
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
	inline auto create_swapchain(vk::PhysicalDevice physical_device, vk::Device device, vk::SurfaceKHR surface, uint32_t width, uint32_t height, vk::Format format) {
		vk::SurfaceCapabilitiesKHR surfaceCapabilities = physical_device.getSurfaceCapabilitiesKHR(surface);
		vk::Extent2D swapchainExtent;
		if (surfaceCapabilities.currentExtent.width == std::numeric_limits<uint32_t>::max()) {
			swapchainExtent.width = std::clamp(width, surfaceCapabilities.minImageExtent.width, surfaceCapabilities.maxImageExtent.width);
			swapchainExtent.height = std::clamp(height, surfaceCapabilities.minImageExtent.height, surfaceCapabilities.maxImageExtent.height);
		}
		else {
			swapchainExtent = surfaceCapabilities.currentExtent;
		}
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
	class present_manager {
	public:
		present_manager(vk::SharedDevice device, uint32_t count) : m_device{ device }, next_semaphore_index{ 0 } {
			for (int i = 0; i < count; i++) {
				m_semaphores.push_back(std::pair{ m_device->createFence(vk::FenceCreateInfo{vk::FenceCreateFlagBits::eSignaled}), m_device->createSemaphore(vk::SemaphoreCreateInfo{}) });
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
			return std::pair{ fence, semaphore };
		}
		void wait_all() {
			std::ranges::for_each(m_semaphores, [&m_device=m_device](auto fence_semaphore) {
				auto [fence, semaphore] = fence_semaphore;
				auto res = m_device->waitForFences(std::array<vk::Fence, 1>{fence}, true, UINT64_MAX);
				assert(res == vk::Result::eSuccess);
				});
		}
		~present_manager() {
			wait_all();
			std::ranges::for_each(m_semaphores, [&m_device = m_device](auto fence_semaphore) {
				auto [fence, semaphore] = fence_semaphore;
				m_device->destroyFence(fence);
				m_device->destroySemaphore(semaphore);
				});
		}
	private:
		vk::SharedDevice m_device;
		std::vector<std::pair<vk::Fence, vk::Semaphore>> m_semaphores;
		uint32_t next_semaphore_index;
	};
	namespace shared {
		auto create_instance() {
			return vk::SharedInstance{ terminal::create_instance() };
		}
		auto select_physical_device(vk::SharedInstance instance) {
			return vk::SharedPhysicalDevice{ terminal::select_physical_device(*instance), instance };
		}
		auto select_queue_family(vk::SharedPhysicalDevice physical_device) {
			return terminal::select_queue_family(*physical_device);
		}
		auto create_window_and_get_surface(vk::SharedInstance instance, uint32_t width, uint32_t height) {
			auto [window, surface] = terminal::create_window_and_get_surface(*instance, width, height);
			return std::pair{ window, vk::SharedSurfaceKHR{surface, instance} };
		}
		auto create_device(vk::SharedPhysicalDevice physical_device, uint32_t graphics_queue_family_index) {
			return vk::SharedDevice{ terminal::create_device(*physical_device, graphics_queue_family_index)};
		}
	}
}
