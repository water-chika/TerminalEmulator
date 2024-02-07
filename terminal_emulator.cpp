#include "vulkan_render.hpp"
#include "font_loader.hpp"
#include "shader_path.hpp"

#include <set>
#include <map>
#include <memory>
#include <utility>
#include <functional>

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
		assert(GLFW_TRUE == glfwVulkanSupported());
	}
	~glfwContext() {
		glfwTerminate();
	}
};

enum class run_result {
	eContinue,
	eBreak,
};

class window_manager {
public:
	window_manager() : window{ create_window() } {

	}
	auto get_window() {
		return window;
	}
	run_result run() {
		glfwPollEvents();
		return glfwWindowShouldClose(window) ? run_result::eBreak : run_result::eContinue;
	}
private:
	static GLFWwindow* create_window() {
		uint32_t width = 512, height = 512;
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		return glfwCreateWindow(width, height, "Terminal Emulator", nullptr, nullptr);
	}
	glfwContext glfw_context;
	GLFWwindow* window;
};

class fix_instance_destroy {
public:
	fix_instance_destroy()
		: instance{ vulkan::shared::create_instance() }
	{}
protected:
	vk::SharedInstance instance;
};

class vulkan_render : fix_instance_destroy {
public:
	void init(auto&& get_surface) {
		auto physical_device{ vulkan::shared::select_physical_device(instance) };
		uint32_t graphicsQueueFamilyIndex = vulkan::shared::select_queue_family(physical_device);

		device = vk::SharedDevice{ vulkan::shared::create_device(physical_device, graphicsQueueFamilyIndex) };
		queue = vk::SharedQueue{ device->getQueue(graphicsQueueFamilyIndex, 0), device };

		vk::CommandPoolCreateInfo commandPoolCreateInfo({}, graphicsQueueFamilyIndex);
		command_pool = vk::SharedCommandPool(device->createCommandPool(commandPoolCreateInfo), device);

		vk::SurfaceKHR vk_surface = get_surface(*instance);
		auto surface = vk::SharedSurfaceKHR{ vk_surface, instance };

		std::vector<vk::SurfaceFormatKHR> formats = physical_device->getSurfaceFormatsKHR(*surface);
		vk::Format color_format = (formats[0].format == vk::Format::eUndefined) ? vk::Format::eR8G8B8A8Unorm : formats[0].format;
		vk::Format depth_format = vk::Format::eD16Unorm;
		vk::SurfaceCapabilitiesKHR surface_caps{};
		physical_device->getSurfaceCapabilitiesKHR(*surface, &surface_caps);
		auto width = surface_caps.currentExtent.width, height = surface_caps.currentExtent.height;
		swapchain = vk::SharedSwapchainKHR(vulkan::create_swapchain(*physical_device, *device, *surface, width, height, color_format), device, surface);

		render_pass = vk::SharedRenderPass{ vulkan::create_render_pass(*device, color_format, depth_format), device };
		auto descriptor_set_bindings = std::array{
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
		auto descriptor_set_layout = device->createDescriptorSetLayoutUnique(vk::DescriptorSetLayoutCreateInfo{}.setBindings(descriptor_set_bindings));
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
		pipeline = vk::SharedPipeline{
			vulkan::create_pipeline(*device,
					mesh_stage_info,
					fragment_path, *render_pass, *pipeline_layout).value, device };

		std::vector<vk::Image> swapchainImages = device->getSwapchainImagesKHR(*swapchain);

		std::string str = "hello world!";
		std::set<char> char_set{};
		std::ranges::copy(str, std::inserter(char_set, char_set.begin()));
		std::vector<char> characters{};
		std::ranges::copy(char_set, std::back_inserter(characters));
		std::map<char, int> char_texture_indices;
		std::ranges::for_each(from_0_count_n(characters.size()), [&characters, &char_texture_indices](auto i) {
			char_texture_indices.emplace(characters[i], static_cast<int>(i));
			});
		std::vector<int> str_texture_indices{};
		std::ranges::transform(str, std::back_inserter(str_texture_indices), [&char_texture_indices](auto c) {
			return char_texture_indices[c];
			});

		font_loader font_loader{};
		uint32_t font_width = 32, font_height = 32;
		font_loader.set_char_size(font_width, font_height);
		auto [vk_texture, vk_texture_memory, vk_texture_view] =
			vulkan::create_texture(*physical_device, *device,
				vk::Format::eR8Unorm,
				font_width * std::size(char_set), font_height,
				[&font_loader, font_width, font_height, &characters](char* ptr, int pitch) {
					std::ranges::for_each(from_0_count_n(characters.size()), [&font_loader, font_width, font_height, &characters, ptr, pitch](auto i) {
						font_loader.render_char(characters[i]);
						auto glyph = font_loader.get_glyph();
						uint32_t start_row = font_height - glyph->bitmap_top - 1;
						for (int row = 0; row < glyph->bitmap.rows; row++) {
							for (int x = 0; x < glyph->bitmap.width; x++) {
								ptr[(start_row + row) * pitch + glyph->bitmap_left + i * font_width + x] =
									glyph->bitmap.buffer[row * glyph->bitmap.pitch + x];
							}
						}
						});
				});
		texture = vk::SharedImage{
			vk_texture, device };
		texture_view = vk::SharedImageView{ vk_texture_view, device };
		auto [vk_char_indices_buffer, vk_char_indices_buffer_memory, vk_char_indices_buffer_size] =
			vulkan::create_uniform_buffer(*physical_device, *device,
				str_texture_indices);
		char_indices_buffer = vk::SharedBuffer{ vk_char_indices_buffer, device };
		char_indices_buffer_memory = vk::SharedDeviceMemory{ vk_char_indices_buffer_memory, device };
		present_manager = std::make_shared<vulkan::present_manager>(device, 10);
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
		texture_memory = vk::SharedDeviceMemory{ vk_texture_memory, device };
		auto descriptor_pool_size = std::array{
			vk::DescriptorPoolSize{}.setType(vk::DescriptorType::eCombinedImageSampler).setDescriptorCount(1),
			vk::DescriptorPoolSize{}.setType(vk::DescriptorType::eUniformBuffer).setDescriptorCount(1),
		};
		descriptor_pool = device->createDescriptorPoolUnique(vk::DescriptorPoolCreateInfo{}.setPoolSizes(descriptor_pool_size).setMaxSets(1));
		auto descriptor_set = std::move(device->allocateDescriptorSets(vk::DescriptorSetAllocateInfo{}.setDescriptorPool(*descriptor_pool).setSetLayouts(*descriptor_set_layout)).front());
		sampler = device->createSamplerUnique(vk::SamplerCreateInfo{});
		auto texture_image_info = vk::DescriptorImageInfo{}.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal).setImageView(vk_texture_view).setSampler(*sampler);
		auto char_texture_indices_info = vk::DescriptorBufferInfo{}.setBuffer(vk_char_indices_buffer).setOffset(0).setRange(vk::WholeSize);
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
		//auto [vk_vertex_buffer, vk_vertex_buffer_memory, vertex_buffer_memory_size] =
		//	vulkan::create_vertex_buffer(*physical_device, *device, vertices);
		//vertex_buffer = vk::SharedBuffer{ vk_vertex_buffer, device };
		//vertex_buffer_memory = vk::SharedDeviceMemory{ vk_vertex_buffer_memory, device };

		vk::CommandBufferAllocateInfo commandBufferAllocateInfo(*command_pool, vk::CommandBufferLevel::ePrimary, swapchainImages.size());
		auto buffers = device->allocateCommandBuffers(commandBufferAllocateInfo);
		command_buffers.resize(swapchainImages.size());
		vk::DispatchLoaderDynamic dldid(*instance, vkGetInstanceProcAddr, *device);
		for (int i = 0; i < swapchainImages.size(); i++) {
			simple_draw_command draw_command{ buffers[i], *render_pass, *pipeline_layout, *pipeline, descriptor_set, *framebuffers[i], width, height, dldid };
			command_buffers[i] = draw_command.get_command_buffer();
		}



		// There should be some commands wait for texture load semaphore.
		// There will be a BUG, fix it!
	}
	run_result run() {
		auto reused_acquire_image_semaphore = present_manager->get_next();
		auto image_index = device->acquireNextImageKHR(
			*swapchain, UINT64_MAX,
			reused_acquire_image_semaphore.semaphore).value;

		auto& render_complete_semaphore = render_complete_semaphores[image_index];
		auto& command_buffer = command_buffers[image_index];

		{
			auto wait_semaphore_infos = std::array{
				vk::SemaphoreSubmitInfo{}
				.setSemaphore(reused_acquire_image_semaphore.semaphore)
				.setStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput),
			};
			auto submit_cmd_info = vk::CommandBufferSubmitInfo{}.setCommandBuffer(command_buffer);
			auto signal_semaphore_info = vk::SemaphoreSubmitInfo{}.setSemaphore(*render_complete_semaphore).setStageMask(vk::PipelineStageFlagBits2::eAllCommands);
			queue->submit2(
				vk::SubmitInfo2{}
				.setWaitSemaphoreInfos(wait_semaphore_infos)
				.setCommandBufferInfos(submit_cmd_info)
				.setSignalSemaphoreInfos(signal_semaphore_info),
				reused_acquire_image_semaphore.fence);
		}

		{
			std::array<vk::Semaphore, 1> wait_semaphores{ *render_complete_semaphore };
			std::array<vk::SwapchainKHR, 1> swapchains{ *swapchain };
			std::array<uint32_t, 1> indices{ image_index };
			vk::PresentInfoKHR present_info{ wait_semaphores, swapchains, indices };
			assert(queue->presentKHR(present_info) == vk::Result::eSuccess);
		}
		return run_result::eContinue;
	}
	~vulkan_render() {
		present_manager->wait_all();
	}
private:
	vk::SharedDevice device;
	vk::SharedCommandPool command_pool;
	vk::SharedSwapchainKHR swapchain;
	vk::UniqueDescriptorPool descriptor_pool;
	vk::SharedPipeline pipeline;
	vk::SharedRenderPass render_pass;
	std::shared_ptr<vulkan::present_manager> present_manager;
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

class terminal_emulator {
public:
	terminal_emulator() {
		GLFWwindow* window = window_manager.get_window();
		m_render.init([window](VkInstance instance) {
			VkSurfaceKHR surface;
			assert(VK_SUCCESS == glfwCreateWindowSurface(instance, window, nullptr, &surface));
			return surface;
			});
	}
	void run() {
		auto runs = std::array<std::function<run_result()>, 2>{
			[&window_manager=window_manager]() { return window_manager.run(); },
			[&m_render=m_render]() { return m_render.run(); },
		};
		while (
			std::ranges::all_of(runs, [](auto f) {
			return f() == run_result::eContinue;
			})
			);
	}
private:
	window_manager window_manager;
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
