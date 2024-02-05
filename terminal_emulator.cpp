#include "vulkan_render.hpp"
#include "font_loader.hpp"
#include "shader_path.hpp"

class simple_draw_command{
public:
    simple_draw_command(
		vk::CommandBuffer cmd,
		vk::RenderPass render_pass,
		vk::PipelineLayout pipeline_layout,
		vk::Pipeline pipeline,
		vk::DescriptorSet descriptor_set,
		vk::Framebuffer framebuffer,
		uint32_t width, uint32_t height,
		vk::DispatchLoaderDynamic dldid)
		: m_cmd{ cmd } {
			vk::CommandBufferBeginInfo begin_info{vk::CommandBufferUsageFlagBits::eSimultaneousUse};
			cmd.begin(begin_info);
			std::array<vk::ClearValue, 2> clear_values;
			clear_values[0].color = vk::ClearColorValue{ 1.0f, 0.0f,0.0f,1.0f };
			clear_values[1].depthStencil = vk::ClearDepthStencilValue{ 1.0f, 0 };
			vk::RenderPassBeginInfo render_pass_begin_info{
				render_pass, framebuffer,
				vk::Rect2D{vk::Offset2D{0,0},
				vk::Extent2D{width,height}}, clear_values };
			cmd.beginRenderPass(render_pass_begin_info, vk::SubpassContents::eInline);
			cmd.bindPipeline(vk::PipelineBindPoint::eGraphics,
				pipeline);
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
				pipeline_layout, 0, descriptor_set, nullptr);
			//cmd.bindVertexBuffers(0, *vertex_buffer, { 0 });
			cmd.setViewport(0, vk::Viewport(0, 0, width, height, 0, 1));
			cmd.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(width, height)));
			//cmd.draw(3, 1, 0, 0);
			cmd.drawMeshTasksEXT(2, 1, 1, dldid);
			cmd.endRenderPass();
			cmd.end();
	}
	auto get_command_buffer() {
		return m_cmd;
	}
private:
    vk::CommandBuffer m_cmd;
};

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

class vulkan_render {
public:
	vulkan_render() {
	}
	void run() {
		auto instance{ vulkan::shared::create_instance() };

		auto physical_device{ vulkan::shared::select_physical_device(instance) };
		uint32_t graphicsQueueFamilyIndex = vulkan::shared::select_queue_family(physical_device);

		uint32_t width = 512, height = 512;
		auto [window, surface] = vulkan::shared::create_window_and_get_surface(instance, width, height);

		vk::SharedDevice device{ vulkan::shared::create_device(physical_device, graphicsQueueFamilyIndex) };
		vk::SharedQueue queue{ device->getQueue(graphicsQueueFamilyIndex, 0), device };

		vk::CommandPoolCreateInfo commandPoolCreateInfo({}, graphicsQueueFamilyIndex);
		vk::SharedCommandPool commandPool(device->createCommandPool(commandPoolCreateInfo), device);


		std::vector<vk::SurfaceFormatKHR> formats = physical_device->getSurfaceFormatsKHR(*surface);
		vk::Format color_format = (formats[0].format == vk::Format::eUndefined) ? vk::Format::eR8G8B8A8Unorm : formats[0].format;
		vk::Format depth_format = vk::Format::eD16Unorm;
		auto swapchain = vk::SharedSwapchainKHR(vulkan::create_swapchain(*physical_device, *device, *surface, width, height, color_format), device, surface);

		auto render_pass = vk::SharedRenderPass{ vulkan::create_render_pass(*device, color_format, depth_format), device };
		auto descriptor_set_binding = vk::DescriptorSetLayoutBinding{}
			.setBinding(0)
			.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
			.setStageFlags(vk::ShaderStageFlagBits::eFragment)
			.setDescriptorCount(1);
		auto descriptor_set_layout = device->createDescriptorSetLayoutUnique(vk::DescriptorSetLayoutCreateInfo{}.setBindings(descriptor_set_binding));
		auto pipeline_layout = vk::SharedPipelineLayout{ vulkan::create_pipeline_layout(*device, *descriptor_set_layout), device };

		vulkan::vertex_stage_info vertex_stage_info{
			vertex_path, "main", vk::VertexInputBindingDescription{0, 4 * sizeof(float)},
			std::vector<vk::VertexInputAttributeDescription>{{0, 0, vk::Format::eR32G32B32A32Sfloat, 0}}
		};
		vulkan::mesh_stage_info mesh_stage_info{
			mesh_path, "main"
		};
		vulkan::geometry_stage_info geometry_stage_info{
			geometry_path, "main",
		};
		auto pipeline = vk::SharedPipeline{
			vulkan::create_pipeline(*device,
					mesh_stage_info,
					fragment_path, *render_pass, *pipeline_layout).value, device };

		std::vector<vk::Image> swapchainImages = device->getSwapchainImagesKHR(*swapchain);
		font_loader font_loader{};
		auto bitmap = font_loader.get_bitmap();
		auto [vk_texture, vk_texture_memory, texture_view] =
			vulkan::create_texture(*physical_device, *device, vk::Format::eR8Unorm, bitmap->pitch, bitmap->rows, std::span{ bitmap->buffer, bitmap->pitch * bitmap->rows });
		vk::SharedImage texture{
			vk_texture, device };
		vk::SharedImageView shared_texture_view{ texture_view, device };

		auto texture_prepare_semaphore = device->createSemaphoreUnique(vk::SemaphoreCreateInfo{});
		{
			vk::CommandBuffer init_command_buffer{ device->allocateCommandBuffers(vk::CommandBufferAllocateInfo{}.setCommandBufferCount(1).setCommandPool(*commandPool)).front() };
			init_command_buffer.begin(vk::CommandBufferBeginInfo{});
			vulkan::set_image_layout(init_command_buffer, *texture, vk::ImageAspectFlagBits::eColor, vk::ImageLayout::ePreinitialized,
				vk::ImageLayout::eShaderReadOnlyOptimal, vk::AccessFlagBits(), vk::PipelineStageFlagBits::eTopOfPipe,
				vk::PipelineStageFlagBits::eFragmentShader);
			auto cmd = init_command_buffer;
			cmd.end();
			auto signal_semaphore = *texture_prepare_semaphore;
			auto command_submit_info = vk::CommandBufferSubmitInfo{}.setCommandBuffer(cmd);
			auto signal_semaphore_info = vk::SemaphoreSubmitInfo{}.setSemaphore(signal_semaphore).setStageMask(vk::PipelineStageFlagBits2::eTransfer);
			queue->submit2(vk::SubmitInfo2{}.setCommandBufferInfos(command_submit_info).setSignalSemaphoreInfos(signal_semaphore_info));
		}
		vk::SharedDeviceMemory texture_memory{ vk_texture_memory, device };
		auto descriptor_pool_size = vk::DescriptorPoolSize{}.setType(vk::DescriptorType::eCombinedImageSampler).setDescriptorCount(1);
		auto descriptor_pool = device->createDescriptorPoolUnique(vk::DescriptorPoolCreateInfo{}.setPoolSizes(descriptor_pool_size).setMaxSets(1));
		auto descriptor_set = std::move(device->allocateDescriptorSets(vk::DescriptorSetAllocateInfo{}.setDescriptorPool(*descriptor_pool).setSetLayouts(*descriptor_set_layout)).front());
		auto sampler = device->createSamplerUnique(vk::SamplerCreateInfo{});
		auto texture_image_info = vk::DescriptorImageInfo{}.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal).setImageView(texture_view).setSampler(*sampler);
		auto descriptor_set_write =
			vk::WriteDescriptorSet{}
			.setDstBinding(0)
			.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
			.setImageInfo(texture_image_info)
			.setDstSet(descriptor_set);
		device->updateDescriptorSets(descriptor_set_write, nullptr);

		std::vector<vk::SharedImageView> imageViews;
		std::vector<vk::UniqueSemaphore> render_complete_semaphores;
		std::vector<vk::SharedImage> depth_buffers;
		std::vector<vk::SharedImageView> depth_buffer_views;
		std::vector<vk::SharedDeviceMemory> depth_buffer_memories;
		std::vector<vk::SharedFramebuffer> framebuffers;
		imageViews.reserve(swapchainImages.size());
		vk::ImageViewCreateInfo imageViewCreateInfo({}, {},
			vk::ImageViewType::e2D, color_format, {}, { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
		for (auto image : swapchainImages) {
			imageViewCreateInfo.image = image;
			auto image_view = device->createImageView(imageViewCreateInfo);
			imageViews.emplace_back(vk::SharedImageView{ image_view, device });
			render_complete_semaphores.push_back(device->createSemaphoreUnique(vk::SemaphoreCreateInfo{}));

			auto [depth_buffer, depth_buffer_memory, depth_buffer_view] =
				vulkan::create_depth_buffer(*physical_device, *device, depth_format, width, height);
			depth_buffers.emplace_back(vk::SharedImage{ depth_buffer, device });
			depth_buffer_memories.emplace_back(vk::SharedDeviceMemory{ depth_buffer_memory, device });
			depth_buffer_views.emplace_back(vk::SharedImageView{ depth_buffer_view, device });
			framebuffers.emplace_back(
				vk::SharedFramebuffer{
				vulkan::create_framebuffer(*device, *render_pass,
						std::vector{image_view, depth_buffer_view}, {width, height}), device
				});
		}
		struct vertex { float x, y, z, w; };
		std::vector<vertex> vertices{
			{0,0,0.5f,1},
			{1,0,0.5f,1},
			{0,1,0.5f,1},
		};
		auto [vk_vertex_buffer, vk_vertex_buffer_memory, vertex_buffer_memory_size] =
			vulkan::create_vertex_buffer(*physical_device, *device, vertices);
		vk::SharedBuffer vertex_buffer{ vk_vertex_buffer, device };
		vk::SharedDeviceMemory vertex_buffer_memory{ vk_vertex_buffer_memory, device };

		vk::CommandBufferAllocateInfo commandBufferAllocateInfo(*commandPool, vk::CommandBufferLevel::ePrimary, swapchainImages.size());
		auto buffers = device->allocateCommandBuffers(commandBufferAllocateInfo);
		std::vector<vk::CommandBuffer> command_buffers(swapchainImages.size());
		vk::DispatchLoaderDynamic dldid(*instance, vkGetInstanceProcAddr, *device);
		for (int i = 0; i < swapchainImages.size(); i++) {
			simple_draw_command draw_command{ buffers[i], *render_pass, *pipeline_layout, *pipeline, descriptor_set, *framebuffers[i], width, height, dldid };
			command_buffers[i] = draw_command.get_command_buffer();
		}


		vulkan::present_manager present_manager{ device, 10 };
		std::ranges::for_each(from_0_count_n(1), [&present_manager, &device, &swapchain, &render_complete_semaphores, &command_buffers, &queue, &texture_prepare_semaphore](auto) {
			auto [present_complete_fence, acquire_image_semaphore] = present_manager.get_next();
			auto image_index = device->acquireNextImageKHR(*swapchain, UINT64_MAX, acquire_image_semaphore).value;

			auto& render_complete_semaphore = render_complete_semaphores[image_index];
			auto& command_buffer = command_buffers[image_index];

			{
				auto wait_semaphore_infos = std::array{
					vk::SemaphoreSubmitInfo{}.setSemaphore(acquire_image_semaphore).setStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput),
					vk::SemaphoreSubmitInfo{}.setSemaphore(*texture_prepare_semaphore).setStageMask(vk::PipelineStageFlagBits2::eAllCommands),
				};
				auto submit_cmd_info = vk::CommandBufferSubmitInfo{}.setCommandBuffer(command_buffer);
				auto signal_semaphore_info = vk::SemaphoreSubmitInfo{}.setSemaphore(*render_complete_semaphore).setStageMask(vk::PipelineStageFlagBits2::eAllCommands);
				queue->submit2(
					vk::SubmitInfo2{}
					.setWaitSemaphoreInfos(wait_semaphore_infos)
					.setCommandBufferInfos(submit_cmd_info)
					.setSignalSemaphoreInfos(signal_semaphore_info),
					present_complete_fence);
			}

			{
				std::array<vk::Semaphore, 1> wait_semaphores{ *render_complete_semaphore };
				std::array<vk::SwapchainKHR, 1> swapchains{ *swapchain };
				std::array<uint32_t, 1> indices{ image_index };
				vk::PresentInfoKHR present_info{ wait_semaphores, swapchains, indices };
				assert(queue->presentKHR(present_info) == vk::Result::eSuccess);
			}
			});
		while (false == glfwWindowShouldClose(window)) {
			auto [present_complete_fence, acquire_image_semaphore] = present_manager.get_next();
			auto image_index = device->acquireNextImageKHR(*swapchain, UINT64_MAX, acquire_image_semaphore).value;

			auto& render_complete_semaphore = render_complete_semaphores[image_index];
			auto& command_buffer = command_buffers[image_index];

			{
				auto wait_semaphore_infos = std::array{
					vk::SemaphoreSubmitInfo{}.setSemaphore(acquire_image_semaphore).setStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput),
				};
				auto submit_cmd_info = vk::CommandBufferSubmitInfo{}.setCommandBuffer(command_buffer);
				auto signal_semaphore_info = vk::SemaphoreSubmitInfo{}.setSemaphore(*render_complete_semaphore).setStageMask(vk::PipelineStageFlagBits2::eAllCommands);
				queue->submit2(
					vk::SubmitInfo2{}
					.setWaitSemaphoreInfos(wait_semaphore_infos)
					.setCommandBufferInfos(submit_cmd_info)
					.setSignalSemaphoreInfos(signal_semaphore_info),
					present_complete_fence);
			}

			{
				std::array<vk::Semaphore, 1> wait_semaphores{ *render_complete_semaphore };
				std::array<vk::SwapchainKHR, 1> swapchains{ *swapchain };
				std::array<uint32_t, 1> indices{ image_index };
				vk::PresentInfoKHR present_info{ wait_semaphores, swapchains, indices };
				assert(queue->presentKHR(present_info) == vk::Result::eSuccess);
			}
			glfwPollEvents();
		}
	}
private:
	glfwContext m_glfw_context;
};

class terminal_emulator {
public:
	void run() {
		m_render.run();
	}
private:
	vulkan_render m_render;
};

int main() {
	try {
		terminal_emulator emulator;
		emulator.run();
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
