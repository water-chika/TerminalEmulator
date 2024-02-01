#include <vulkan/vulkan_raii.hpp>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_shared.hpp>
#include <GLFW/glfw3.h>

#include <iostream>
#include <cassert>
#include <array>
#include <span>
#include <algorithm>
#include <numeric>
#include <ranges>

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
	auto create_instance() {
		vk::ApplicationInfo applicationInfo("Terminal Emulator", 1, nullptr, 0, VK_API_VERSION_1_3);
		std::array<const char*, 0> instanceLayers{};
		std::array<const char*, 2> instanceExtensions{ "VK_KHR_surface", "VK_KHR_win32_surface" };
		vk::InstanceCreateInfo instanceCreateInfo({}, &applicationInfo, instanceLayers, instanceExtensions);
		return vk::SharedInstance{ vk::createInstance(instanceCreateInfo) };
	}
	auto select_physical_device(vk::SharedInstance instance) {
		auto devices = instance->enumeratePhysicalDevices();
		return devices.front();
	}
	auto select_queue_family(vk::PhysicalDevice physical_device) {
		uint32_t graphicsQueueFamilyIndex = 0;
		auto queueFamilyProperties = physical_device.getQueueFamilyProperties();
		for (int i = 0; i < queueFamilyProperties.size(); i++) {
			if (queueFamilyProperties[i].queueFlags & vk::QueueFlagBits::eGraphics) {
				graphicsQueueFamilyIndex = i;
			}
		}
		return graphicsQueueFamilyIndex;
	}
	auto create_window_and_get_surface(vk::Instance instance, uint32_t width, uint32_t height) {
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		assert(true == glfwVulkanSupported());
		GLFWwindow* window = glfwCreateWindow(width, height, "Terminal Emulator", nullptr, nullptr);

		VkSurfaceKHR surface;
		glfwCreateWindowSurface(instance, window, nullptr, &surface);
		return std::pair{ window, surface };
	}
	auto create_device(vk::PhysicalDevice physical_device, uint32_t graphicsQueueFamilyIndex) {
		float queuePriority = 0.0f;
		vk::DeviceQueueCreateInfo deviceQueueCreateInfo({}, graphicsQueueFamilyIndex, 1, &queuePriority);
		std::array<const char*, 0> deviceLayers{};
		std::array<const char*, 1> deviceExtensions{ "VK_KHR_swapchain" };
		vk::DeviceCreateInfo deviceCreateInfo({}, deviceQueueCreateInfo, deviceLayers, deviceExtensions);

		return physical_device.createDevice(deviceCreateInfo);
	}
	auto select_depth_image_tiling(vk::PhysicalDevice physical_device, vk::Format format) {
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
	auto create_image(vk::Device device, vk::ImageType type, vk::Format format, vk::Extent2D extent, vk::ImageTiling tiling, vk::ImageUsageFlags usages) {
		vk::ImageCreateInfo create_info{ {}, type, format, vk::Extent3D{extent, 1}, 1, 1, vk::SampleCountFlagBits::e1, tiling, usages };
		return device.createImage(create_info);
	}
	template<class T, class B>
	bool contain_bit(T bitfield, B bit) {
		return (bitfield & bit) == bit;
	}

	auto select_memory_type(vk::PhysicalDevice physical_device, vk::Device device, vk::MemoryRequirements requirements) {
		auto memory_properties = physical_device.getMemoryProperties();
		auto type_bits = requirements.memoryTypeBits;
		return *std::ranges::find_if(from_0_count_n(memory_properties.memoryTypeCount), [&memory_properties, type_bits](auto i) {
			if ((type_bits & (1 << i)) &&
				contain_bit(memory_properties.memoryTypes[i].propertyFlags, vk::MemoryPropertyFlagBits::eDeviceLocal)) {
				return true;
			}
			return false;
			});
	}
	auto allocate_device_memory(vk::PhysicalDevice physical_device, vk::Device device, vk::Image image) {

		auto memory_requirements = device.getImageMemoryRequirements(image);
		auto type_index = select_memory_type(physical_device, device, memory_requirements);
		return device.allocateMemory(vk::MemoryAllocateInfo{ memory_requirements.size, type_index });
	}
	auto create_depth_buffer(vk::PhysicalDevice physical_device, vk::Device device, uint32_t width, uint32_t height) {
		const vk::Format format = vk::Format::eD16Unorm;
		vk::ImageTiling tiling = select_depth_image_tiling(physical_device, format);
		auto image = create_image(device, vk::ImageType::e2D, format, vk::Extent2D{ width, height }, tiling, vk::ImageUsageFlagBits::eDepthStencilAttachment);
		auto memory = allocate_device_memory(physical_device, device, image);
	}
}

int main() {
	struct glfwContext
	{
		glfwContext()
		{
			glfwInit();
			glfwSetErrorCallback(
				[](int error, const char* msg)
				{
					std::cerr << "glfw: " << "(" << error << ") " << msg << std::endl;
				}
			);
		}
		~glfwContext() {
			glfwTerminate();
		}
	};
	auto glfwCtx = glfwContext{};
	try {
		auto instance{ terminal::create_instance() };

		auto physical_device{ terminal::select_physical_device(instance) };
		uint32_t graphicsQueueFamilyIndex = terminal::select_queue_family(physical_device);

		uint32_t width = 512, height = 512;
		auto [window, vk_surface] = terminal::create_window_and_get_surface(*instance, width, height);
		vk::SharedSurfaceKHR surface(vk_surface, instance);

		vk::SharedDevice device{ terminal::create_device(physical_device, graphicsQueueFamilyIndex) };
		vk::SharedQueue queue{ device->getQueue(graphicsQueueFamilyIndex, 0), device };

		vk::CommandPoolCreateInfo commandPoolCreateInfo({}, graphicsQueueFamilyIndex);
		vk::SharedCommandPool commandPool(device->createCommandPool(commandPoolCreateInfo), device);

		vk::CommandBufferAllocateInfo commandBufferAllocateInfo(*commandPool, vk::CommandBufferLevel::ePrimary, 1);
		auto buffers = device->allocateCommandBuffers(commandBufferAllocateInfo);
		assert(buffers.size() == 1);
		vk::SharedCommandBuffer commandBuffer{ buffers.front(), device, commandPool};

		std::vector<vk::SurfaceFormatKHR> formats = physical_device.getSurfaceFormatsKHR(*surface);
		vk::Format format = (formats[0].format == vk::Format::eUndefined) ? vk::Format::eR8G8B8A8Unorm : formats[0].format;

		vk::SurfaceCapabilitiesKHR surfaceCapabilities = physical_device.getSurfaceCapabilitiesKHR(*surface);
		vk::Extent2D swapchainExtent;
		if (surfaceCapabilities.currentExtent.width == std::numeric_limits<uint32_t>::max()) {
			swapchainExtent.width = std::clamp(width, surfaceCapabilities.minImageExtent.width, surfaceCapabilities.maxImageExtent.width);
			swapchainExtent.height = std::clamp(height, surfaceCapabilities.minImageExtent.height, surfaceCapabilities.maxImageExtent.height);
		}
		else {
			swapchainExtent = surfaceCapabilities.currentExtent;
		}

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
			*surface,
			std::clamp(3u, surfaceCapabilities.minImageCount, surfaceCapabilities.maxImageCount),
			format,
			vk::ColorSpaceKHR::eSrgbNonlinear,
			swapchainExtent,
			1,
			vk::ImageUsageFlagBits::eColorAttachment,
			vk::SharingMode::eExclusive,
			{},
			preTransform,
			compositeAlpha,
			swapchainPresentMode,
			true,
			nullptr);

		vk::SharedSwapchainKHR swapchain{ device->createSwapchainKHR(swapchainCreateInfo), device, surface };
		std::vector<vk::Image> swapchainImages = device->getSwapchainImagesKHR(*swapchain);

		std::vector<vk::SharedImageView> imageViews;
		imageViews.reserve(swapchainImages.size());
		vk::ImageViewCreateInfo imageViewCreateInfo({}, {}, vk::ImageViewType::e2D, format, {}, { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
		for (auto image : swapchainImages) {
			imageViewCreateInfo.image = image;
			imageViews.emplace_back(device->createImageView(imageViewCreateInfo), device);
		}

		auto acquire_image_semaphore{ device->createSemaphoreUnique(vk::SemaphoreCreateInfo{}) };
		auto render_complete_semaphore{ device->createSemaphoreUnique(vk::SemaphoreCreateInfo{}) };

		while (false == glfwWindowShouldClose(window)) {
			auto image = device->acquireNextImageKHR(*swapchain, 1000000000, *acquire_image_semaphore);
			vk::CommandBufferBeginInfo begin_info{};
			commandBuffer->begin(begin_info);
			commandBuffer->end();

			std::array<vk::Semaphore, 1> wait_semaphores{ *acquire_image_semaphore };
			std::array<vk::PipelineStageFlags, 1> stages{ vk::PipelineStageFlagBits::eBottomOfPipe };
			std::array<vk::CommandBuffer, 1> command_buffers{ *commandBuffer };
			std::array<vk::Semaphore, 1> signal_semaphores{ *render_complete_semaphore };
			queue->submit(vk::SubmitInfo{wait_semaphores, stages, command_buffers, signal_semaphores});
			glfwPollEvents();
		}

	}
	catch (vk::SystemError& err) {
		std::cout << "vk::SystemError: " << err.what() << std::endl;
		return -1;
	}
	catch (std::exception& err) {
		std::cout << "std::exception: " << err.what() << std::endl;
	}
	catch (...) {
		std::cout << "unknown error\n";
		return -1;
	}
	return 0;
}