#pragma once
#include "vulkan_utility.hpp"

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


class vulkan_render_prepare : public fix_instance_destroy {
public:
    auto select_physical_device(auto instance) {
        return vulkan::shared::select_physical_device(instance);
    }
    auto select_queue_family(auto physical_device) {
        return vulkan::shared::select_queue_family(physical_device);
    }
    auto create_device(auto physical_device, auto queue_family_index) {
        return vulkan::shared::create_device(physical_device, queue_family_index);
    }
    auto get_queue(auto device, auto queue_family_index) {
        return vulkan::shared::get_queue(device, queue_family_index, 0);
    }
    auto create_command_pool(auto device, auto queue_family_index) {
        return vulkan::shared::create_command_pool(device, queue_family_index);
    }
    auto get_surface(auto instance, auto&& get_surface_from_extern) {
        return vulkan::shared::get_surface(instance, get_surface_from_extern);
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
        auto& imageViews) {
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
        character_count = characters.size();
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
        
        std::tie(texture, texture_memory, texture_view) = create_font_texture(physical_device, device, characters);
        {
            auto [vk_char_indices_buffer, vk_char_indices_buffer_memory, vk_char_indices_buffer_size] =
                vulkan::create_uniform_buffer(*physical_device, *device,
                    char_indices_buf);
            char_indices_buffer = vk::SharedBuffer{ vk_char_indices_buffer, device };
            char_indices_buffer_memory = vk::SharedDeviceMemory{ vk_char_indices_buffer_memory, device };
        }
        update_descriptor_set(descriptor_set, texture_view, sampler, char_indices_buffer);
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
        pipeline_layout = {
            create_pipeline_layout(device, descriptor_set_layout)
        };
        auto descriptor_pool_size{
            get_descriptor_pool_size()
        };
        descriptor_pool = create_descriptor_pool(device, descriptor_pool_size);
        descriptor_set = allocate_descriptor_set(device, descriptor_set_layout);
        sampler = device->createSamplerUnique(vk::SamplerCreateInfo{});

        create_and_update_terminal_buffer_relate_data(
            descriptor_set, sampler, terminal_buffer, imageViews);

        // There should be some commands wait for texture load semaphore.
        // There will be a BUG, fix it!
    }
    void notify_update() {
        create_and_update_terminal_buffer_relate_data(descriptor_set, sampler, *p_terminal_buffer,
            imageViews);
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
    vk::SharedQueue queue;

    vk::SharedPipelineLayout pipeline_layout;
    uint32_t character_count;
};

class mesh_renderer : public vulkan_render_prepare {
public:
    auto create_pipeline(auto device, auto render_pass, auto pipeline_layout, uint32_t character_count) {
        vulkan::task_stage_info task_stage_info{
            task_shader_path, "main",
        };
        class char_count_specialization {
        public:
            char_count_specialization(uint32_t character_count)
                :
                m_count{ character_count },
                map_entry{ vk::SpecializationMapEntry{}.setConstantID(555).setOffset(0).setSize(sizeof(m_count)) },
                specialization_info{ vk::SpecializationInfo{}
                .setMapEntries(map_entry).setDataSize(sizeof(m_count)).setPData(&this->m_count) }
            {
            }
        public:
            uint32_t m_count;
            vk::SpecializationMapEntry map_entry;
            vk::SpecializationInfo specialization_info;
        };
        char_count_specialization specialization{
            character_count
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
    void create_and_update_terminal_buffer_relate_data() {
        vulkan_render_prepare::create_and_update_terminal_buffer_relate_data(
            descriptor_set, sampler, *p_terminal_buffer, imageViews
        );
        pipeline = create_pipeline(device, render_pass, pipeline_layout, character_count);
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
        vulkan_render_prepare::init(get_surface_from_extern, terminal_buffer);
        vk::CommandBufferAllocateInfo commandBufferAllocateInfo(
            *command_pool, vk::CommandBufferLevel::ePrimary, imageViews.size());
        command_buffers = device->allocateCommandBuffers(commandBufferAllocateInfo);
        create_and_update_terminal_buffer_relate_data();
    }
    void notify_update() {
        create_and_update_terminal_buffer_relate_data();
    }
protected:
    vk::SharedPipeline pipeline;
    std::vector<vk::CommandBuffer> command_buffers;
};

class vulkan_render : public mesh_renderer {
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
        mesh_renderer::init(std::forward<decltype(get_surface)>(get_surface),
            terminal_buffer);
        present_manager = std::make_shared<vulkan::present_manager>(device, 10);
        set_texture_image_layout();
    }
    run_result run();
    void notify_update() {
        present_manager->wait_all();
        mesh_renderer::notify_update();
        set_texture_image_layout();
    }
private:
    std::shared_ptr<vulkan::present_manager> present_manager;
};
