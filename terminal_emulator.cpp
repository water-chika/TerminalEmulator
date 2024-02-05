#include "vulkan_render.hpp"

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
		FT_Library font_library;
		if (FT_Init_FreeType(&font_library)) {
			throw std::runtime_error{ "failed to initialize font library" };
		}
		FT_Face font_face;
		if (FT_New_Face(font_library, "C:\\Windows\\Fonts\\consola.ttf", 0, &font_face)) {
			throw std::runtime_error{ "failed to open font file" };
		}
		if (FT_Set_Char_Size(font_face, 0, 16 * 64, 512, 512)) {
			throw std::runtime_error{ "failed to set font size" };
		}
		auto glyph_index = FT_Get_Char_Index(font_face, 'A');
		assert(glyph_index != 0);
		if (FT_Load_Glyph(font_face, glyph_index, FT_LOAD_DEFAULT)) {
			throw std::runtime_error{ "failed to load glyph" };
		}
		if (FT_Render_Glyph(font_face->glyph, FT_RENDER_MODE_NORMAL)) {
			throw std::runtime_error{ "failed to render glyph" };
		}


		auto instance{ terminal::shared::create_instance() };

		auto physical_device{ terminal::shared::select_physical_device(instance) };
		uint32_t graphicsQueueFamilyIndex = terminal::shared::select_queue_family(physical_device);

		uint32_t width = 512, height = 512;
		auto [window, surface] = terminal::shared::create_window_and_get_surface(instance, width, height);

		vk::SharedDevice device{ terminal::shared::create_device(physical_device, graphicsQueueFamilyIndex) };
		vk::SharedQueue queue{ device->getQueue(graphicsQueueFamilyIndex, 0), device };

		vk::CommandPoolCreateInfo commandPoolCreateInfo({}, graphicsQueueFamilyIndex);
		vk::SharedCommandPool commandPool(device->createCommandPool(commandPoolCreateInfo), device);
		vk::SharedCommandBuffer init_command_buffer{ device->allocateCommandBuffers(vk::CommandBufferAllocateInfo{}.setCommandBufferCount(1).setCommandPool(*commandPool)).front(), device };
		init_command_buffer->begin(vk::CommandBufferBeginInfo{});


		std::vector<vk::SurfaceFormatKHR> formats = physical_device->getSurfaceFormatsKHR(*surface);
		vk::Format color_format = (formats[0].format == vk::Format::eUndefined) ? vk::Format::eR8G8B8A8Unorm : formats[0].format;
		vk::Format depth_format = vk::Format::eD16Unorm;
		auto swapchain = vk::SharedSwapchainKHR(terminal::create_swapchain(*physical_device, *device, *surface, width, height, color_format), device, surface);
		
		auto render_pass = vk::SharedRenderPass{ terminal::create_render_pass(*device, color_format, depth_format), device };
		auto descriptor_set_binding = vk::DescriptorSetLayoutBinding{}
		.setBinding(0)
			.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
			.setStageFlags(vk::ShaderStageFlagBits::eFragment)
			.setDescriptorCount(1);
		auto descriptor_set_layout = device->createDescriptorSetLayoutUnique(vk::DescriptorSetLayoutCreateInfo{}.setBindings(descriptor_set_binding));
		auto pipeline_layout = vk::SharedPipelineLayout{ terminal::create_pipeline_layout(*device, *descriptor_set_layout), device };

		terminal::vertex_stage_info vertex_stage_info{
            "vertex.spv", "main", vk::VertexInputBindingDescription{0, 4 * sizeof(float)},
            std::vector<vk::VertexInputAttributeDescription>{{0, 0, vk::Format::eR32G32B32A32Sfloat, 0}}
		};
		terminal::mesh_stage_info mesh_stage_info{
			"mesh.spv", "main"
		};
        terminal::geometry_stage_info geometry_stage_info{
            "geometry.spv", "main",
        };
		auto pipeline = vk::SharedPipeline{ 
            terminal::create_pipeline(*device,
                    mesh_stage_info,
                    "fragment.spv", *render_pass, *pipeline_layout).value, device };

		std::vector<vk::Image> swapchainImages = device->getSwapchainImagesKHR(*swapchain);
		auto glyph = font_face->glyph;
		auto bitmap = &glyph->bitmap;
		auto [vk_texture, vk_texture_memory, texture_view] = 
			terminal::create_texture(*physical_device, *device, vk::Format::eR8Unorm, bitmap->pitch, bitmap->rows, std::span{ bitmap->buffer, bitmap->pitch * bitmap->rows });
		vk::SharedImage texture{
			vk_texture, device};
		terminal::set_image_layout(*init_command_buffer, *texture, vk::ImageAspectFlagBits::eColor, vk::ImageLayout::ePreinitialized,
			vk::ImageLayout::eShaderReadOnlyOptimal, vk::AccessFlagBits(), vk::PipelineStageFlagBits::eTopOfPipe,
			vk::PipelineStageFlagBits::eFragmentShader);
		{
			auto cmd = *init_command_buffer;
			cmd.end();
			auto fence = device->createFenceUnique(vk::FenceCreateInfo{});
			vk::PipelineStageFlags stages = vk::PipelineStageFlagBits::eTransfer;
			queue->submit(vk::SubmitInfo{}.setCommandBuffers(cmd), * fence);
			device->waitForFences(*fence, true, UINT64_MAX);
		}
		vk::SharedDeviceMemory texture_memory{ vk_texture_memory, device };
		auto descriptor_pool_size = vk::DescriptorPoolSize{}.setType(vk::DescriptorType::eCombinedImageSampler).setDescriptorCount(1);
		auto descriptor_pool = device->createDescriptorPoolUnique(vk::DescriptorPoolCreateInfo{}.setPoolSizes(descriptor_pool_size).setMaxSets(1));
		auto descriptor_set = std::move(device->allocateDescriptorSetsUnique(vk::DescriptorSetAllocateInfo{}.setDescriptorPool(*descriptor_pool).setSetLayouts(*descriptor_set_layout)).front());
		auto sampler = device->createSamplerUnique(vk::SamplerCreateInfo{});
		auto texture_image_info = vk::DescriptorImageInfo{}.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal).setImageView(texture_view).setSampler(*sampler);
		auto descriptor_set_write =
			vk::WriteDescriptorSet{}
			.setDstBinding(0)
			.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
			.setImageInfo(texture_image_info)
			.setDstSet(*descriptor_set);
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
                terminal::create_depth_buffer(*physical_device, *device, depth_format, width, height);
			depth_buffers.emplace_back(vk::SharedImage{ depth_buffer, device });
			depth_buffer_memories.emplace_back(vk::SharedDeviceMemory{ depth_buffer_memory, device });
			depth_buffer_views.emplace_back(vk::SharedImageView{ depth_buffer_view, device });
			framebuffers.emplace_back(
                    vk::SharedFramebuffer{
                    terminal::create_framebuffer(*device, *render_pass, 
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
            terminal::create_vertex_buffer(*physical_device, *device, vertices);
		vk::SharedBuffer vertex_buffer{ vk_vertex_buffer, device };
		vk::SharedDeviceMemory vertex_buffer_memory{ vk_vertex_buffer_memory, device };

		vk::CommandBufferAllocateInfo commandBufferAllocateInfo(*commandPool, vk::CommandBufferLevel::ePrimary, swapchainImages.size());
		auto buffers = device->allocateCommandBuffers(commandBufferAllocateInfo);
		std::vector<vk::SharedCommandBuffer> command_buffers(swapchainImages.size());
		vk::DispatchLoaderDynamic dldid(*instance, vkGetInstanceProcAddr, *device);
		for (int i = 0; i < swapchainImages.size(); i++){
			command_buffers[i] = vk::SharedCommandBuffer{ buffers[i], device, commandPool };
			vk::CommandBufferBeginInfo begin_info{vk::CommandBufferUsageFlagBits::eSimultaneousUse};
			command_buffers[i]->begin(begin_info);
			std::array<vk::ClearValue, 2> clear_values;
			clear_values[0].color = vk::ClearColorValue{ 1.0f, 0.0f,0.0f,1.0f };
			clear_values[1].depthStencil = vk::ClearDepthStencilValue{ 1.0f, 0 };
			vk::RenderPassBeginInfo render_pass_begin_info{ *render_pass, *framebuffers[i], vk::Rect2D{vk::Offset2D{0,0}, vk::Extent2D{width,height}}, clear_values };
			command_buffers[i]->beginRenderPass(render_pass_begin_info, vk::SubpassContents::eInline);
			command_buffers[i]->bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline);
			command_buffers[i]->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *pipeline_layout, 0, *descriptor_set, nullptr);
			//command_buffers[i]->bindVertexBuffers(0, *vertex_buffer, { 0 });
			command_buffers[i]->setViewport(0, vk::Viewport(0, 0, width, height, 0, 1));
			command_buffers[i]->setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(width, height)));
			//command_buffers[i]->draw(3, 1, 0, 0);
			command_buffers[i]->drawMeshTasksEXT(2, 1, 1, dldid);
			command_buffers[i]->endRenderPass();
			command_buffers[i]->end();
		}

		terminal::present_manager present_manager{ device, 10 };
		while (false == glfwWindowShouldClose(window)) {
			auto [present_complete_fence, acquire_image_semaphore] = present_manager.get_next();
			auto image_index = device->acquireNextImageKHR(*swapchain, UINT64_MAX, acquire_image_semaphore).value;
			
			auto& render_complete_semaphore = render_complete_semaphores[image_index];
			auto& command_buffer = command_buffers[image_index];

			{
				std::array<vk::Semaphore, 1> wait_semaphores{ acquire_image_semaphore };
				std::array<vk::PipelineStageFlags, 1> stages{ vk::PipelineStageFlagBits::eColorAttachmentOutput };
				std::array<vk::CommandBuffer, 1> submit_command_buffers{ *command_buffer};
				std::array<vk::Semaphore, 1> signal_semaphores{ *render_complete_semaphore };
				queue->submit(vk::SubmitInfo{ wait_semaphores, stages, submit_command_buffers, signal_semaphores }, present_complete_fence);
			}
			
			{
				std::array<vk::Semaphore, 1> wait_semaphores{ *render_complete_semaphore };
				std::array<vk::SwapchainKHR, 1> swapchains{ *swapchain };
				std::array<uint32_t, 1> indices{ image_index };
				vk::PresentInfoKHR present_info{wait_semaphores, swapchains, indices};
				assert(queue->presentKHR(present_info) == vk::Result::eSuccess);
			}
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
