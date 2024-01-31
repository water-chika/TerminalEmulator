#include <vulkan/vulkan_raii.hpp>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_shared.hpp>
#include <GLFW/glfw3.h>

#include <iostream>
#include <cassert>
#include <array>

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