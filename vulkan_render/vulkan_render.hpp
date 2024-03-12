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
    auto generate_char_set(auto& terminal_buffer) {
        std::set<char> char_set{};
        std::for_each(
            terminal_buffer.begin(),
            terminal_buffer.end(),
            [&char_set](auto c) {
                char_set.emplace(c);
            });
        return char_set;
    }
    auto generate_characters(auto& char_set) {
        std::vector<char> characters{};
        std::ranges::copy(char_set, std::back_inserter(characters));
        return characters;
    }
    auto generate_char_texture_indices(auto& characters) {
        std::map<char, int> char_texture_indices;
        std::ranges::for_each(from_0_count_n(characters.size()), [&characters, &char_texture_indices](auto i) {
            char_texture_indices.emplace(characters[i], static_cast<int>(i));
            });
        return char_texture_indices;
    }
    auto generate_char_indices_buf(auto& terminal_buffer, auto& char_texture_indices) {
        multidimention_vector<uint32_t> char_indices_buf{terminal_buffer.get_width(), terminal_buffer.get_height()};
        std::transform(
            terminal_buffer.begin(),
            terminal_buffer.end(),
            char_indices_buf.begin(),
            [&char_texture_indices](auto c) {
                return char_texture_indices[c];
            });
        return char_indices_buf;
    }
    void create_and_update_terminal_buffer_relate_data(
        auto descriptor_set, auto& sampler, auto& terminal_buffer,
        auto& imageViews) {
        //generated by attribute_dependence_parser from vulkan_render_prepare_create_and_update_terminal_buffer_relate_data.depend
        auto char_set = generate_char_set(terminal_buffer);


        character_count = char_set.size();


        auto characters = generate_characters(char_set);


        std::tie(texture, texture_memory, texture_view) = create_font_texture(physical_device, device, characters);


        auto char_texture_indices = generate_char_texture_indices(characters);


        char_indices = generate_char_indices_buf(terminal_buffer, char_texture_indices);


        char_indices_buffer = vk::SharedBuffer(vulkan::create_buffer(*device, char_indices.size() * sizeof(*std::begin(char_indices)),
            vk::BufferUsageFlagBits::eUniformBuffer), device);


        char_indices_buffer_memory = vk::SharedDeviceMemory(
            std::get<0>(
                vulkan::allocate_device_memory(*physical_device, *device, *char_indices_buffer,
                    vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent)),
            device);


        vulkan::copy_to_buffer(*device, *char_indices_buffer, *char_indices_buffer_memory, char_indices);


        device->bindBufferMemory(*char_indices_buffer, *char_indices_buffer_memory, 0);


        update_descriptor_set(descriptor_set, texture_view, sampler, char_indices_buffer);
    }
    void create_per_swapchain_image_resources(auto& swapchainImages, auto color_format, auto depth_format) {
        imageViews.reserve(swapchainImages.size());
        vk::ImageViewCreateInfo imageViewCreateInfo({}, {},
            vk::ImageViewType::e2D, color_format, {}, { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
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
    }
    void init(auto&& get_surface_from_extern, auto& terminal_buffer) {
        //generated by attribute_dependence_parser from vulkan_render_prepare_init.depend
        vk::Format depth_format = select_depth_format();


        auto descriptor_pool_size = get_descriptor_pool_size();


        auto descriptor_set_bindings = create_descriptor_set_bindings();


        std::array<std::string, 1> deviceExtensions{
            "VK_KHR_swapchain",
        };


        std::vector<const char*> device_extensions(deviceExtensions.size());
        std::ranges::transform(
            deviceExtensions,
            device_extensions.begin(),
            [](auto& ext) { return ext.data();  });


        physical_device = select_physical_device(instance);


        float queuePriority = 0.0f;


        auto queue_family_index = select_queue_family(physical_device);


        vk::SharedSurfaceKHR surface = get_surface(instance, get_surface_from_extern);


        auto surface_capabilities = get_surface_capabilities(physical_device, surface);


        swapchain_extent = get_surface_extent(surface_capabilities);


        vk::Format color_format = select_color_format(physical_device, surface);


        auto deviceQueueCreateInfo = vk::DeviceQueueCreateInfo{}.setQueueFamilyIndex(queue_family_index).setQueuePriorities(queuePriority);


        vk::StructureChain device_create_info{
            vk::DeviceCreateInfo{}
            .setQueueCreateInfos(deviceQueueCreateInfo)
            .setPEnabledExtensionNames(device_extensions),
            vk::PhysicalDeviceFeatures2{},
            //vk::PhysicalDeviceMeshShaderFeaturesEXT{}.setMeshShader(true).setTaskShader(true),
            vk::PhysicalDeviceMaintenance4Features{}.setMaintenance4(true),
            vk::PhysicalDeviceSynchronization2Features{}.setSynchronization2(true),
        };


        p_terminal_buffer = &terminal_buffer;


        device = vk::SharedDevice{ physical_device->createDevice(device_create_info.get<vk::DeviceCreateInfo>()) };


        queue = get_queue(device, queue_family_index);


        render_pass = create_render_pass(device, color_format, depth_format);


        sampler = device->createSamplerUnique(vk::SamplerCreateInfo());


        swapchain = create_swapchain(physical_device, device, surface, surface_capabilities, color_format);


        auto swapchainImages = device->getSwapchainImagesKHR(*swapchain);


        command_pool = create_command_pool(device, queue_family_index);


        descriptor_pool = create_descriptor_pool(device, descriptor_pool_size);


        descriptor_set_layout = create_descriptor_set_layout(device, descriptor_set_bindings);


        create_per_swapchain_image_resources(swapchainImages, color_format, depth_format);


        pipeline_layout = create_pipeline_layout(device, descriptor_set_layout);


        descriptor_set = allocate_descriptor_set(device, descriptor_set_layout);


        create_and_update_terminal_buffer_relate_data(
            descriptor_set, sampler, terminal_buffer, imageViews);
    }
    void notify_update() {
        create_and_update_terminal_buffer_relate_data(descriptor_set, sampler, *p_terminal_buffer,
            imageViews);
    }

protected:
    multidimention_vector<char>* p_terminal_buffer;
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
    multidimention_vector<uint32_t> char_indices;
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

class vertex_renderer : public vulkan_render_prepare {
public:
    struct vertex {
        float x, y, z, w;
    };
    auto create_pipeline(auto device, auto render_pass, auto pipeline_layout, uint32_t character_count) {
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
        vulkan::vertex_stage_info vertex_stage_info{
            vertex_shader_path, "main",
            vk::VertexInputBindingDescription{}.setBinding(0).setInputRate(vk::VertexInputRate::eVertex).setStride(sizeof(vertex)),
            std::vector{
                vk::VertexInputAttributeDescription{}.setBinding(0).setLocation(0).setOffset(0).setFormat(vk::Format::eR32G32B32A32Sfloat)
            },
            specialization.specialization_info
        };
        return vk::SharedPipeline{
            vulkan::create_pipeline(*device,
                    vertex_stage_info,
                    fragment_shader_path, *render_pass, *pipeline_layout).value, device };
    }
    void create_vertex_buffer(auto&& vertices) {
        auto [buffer, memory, memory_size] = vulkan::create_vertex_buffer(*physical_device, *device, vertices);
        vertex_buffer = vk::SharedBuffer{ buffer, device };
        vertex_buffer_memory = vk::SharedDeviceMemory{ memory, device };
    }
    void create_and_update_terminal_buffer_relate_data() {
        vulkan_render_prepare::create_and_update_terminal_buffer_relate_data(
            descriptor_set, sampler, *p_terminal_buffer, imageViews
        );
        std::vector<vertex> vertices{};
        auto& terminal_buffer = *p_terminal_buffer;
        for (int y = 0; y < terminal_buffer.get_dim1_size(); y++) {
            for (int x = 0; x < terminal_buffer.get_dim0_size(); x++) {
                float width = 2.0f / terminal_buffer.get_dim0_size();
                float height = 2.0f / terminal_buffer.get_dim1_size();
                float s_x = -1 + x * width;
                float s_y = -1 + y * height;
                auto index = char_indices[std::pair{ x, y }];
                const float tex_width = 1.0 / character_count;
                const float tex_advance = tex_width;
                const float tex_offset = tex_width * index;
                vertices.push_back(vertex{ s_x, s_y, tex_offset, 0 });
                vertices.push_back(vertex{ s_x + width, s_y, tex_offset + tex_advance, 0 });
                vertices.push_back(vertex{ s_x, s_y + height, tex_offset, 1 });
                vertices.push_back(vertex{ s_x, s_y + height, tex_offset, 1 });
                vertices.push_back(vertex{ s_x + width, s_y, tex_offset + tex_advance, 0 });
                vertices.push_back(vertex{ s_x + width,s_y + height, tex_offset + tex_advance, 1 });
            }
        }
        create_vertex_buffer(vertices);
        pipeline = create_pipeline(device, render_pass, pipeline_layout, character_count);
        vk::DispatchLoaderDynamic dldid(*instance, vkGetInstanceProcAddr, *device);
        for (integer_less_equal<decltype(imageViews.size())> i{ 0, imageViews.size() }; i < imageViews.size(); i++) {
            record_draw_command(
                command_buffers[i],
                *render_pass,
                *pipeline_layout,
                *pipeline,
                descriptor_set,
                *framebuffers[i],
                swapchain_extent, dldid);
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

    void record_draw_command(
            vk::CommandBuffer cmd,
            vk::RenderPass render_pass,
            vk::PipelineLayout pipeline_layout,
            vk::Pipeline pipeline,
            vk::DescriptorSet descriptor_set,
            vk::Framebuffer framebuffer,
            vk::Extent2D swapchain_extent,
            vk::DispatchLoaderDynamic dldid)
    {
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
            cmd.bindVertexBuffers(0, *vertex_buffer, { 0 });
            cmd.bindVertexBuffers(1, *char_indices_buffer, { 0 });
            cmd.setViewport(0, vk::Viewport(0, 0, swapchain_extent.width, swapchain_extent.height, 0, 1));
            cmd.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), swapchain_extent));
            cmd.draw(p_terminal_buffer->size()*6, 1, 0, 0);
            cmd.endRenderPass();
            cmd.end();
    }
protected:
    vk::SharedPipeline pipeline;
    std::vector<vk::CommandBuffer> command_buffers;

    vk::SharedBuffer vertex_buffer;
    vk::SharedDeviceMemory vertex_buffer_memory;
    vk::SharedBufferView vertex_buffer_view;
};

template<class Renderer>
class renderer_presenter : public Renderer {
public:
    void set_texture_image_layout() {
        auto texture_prepare_semaphore = present_manager->get_next();
        {
            vk::UniqueCommandBuffer init_command_buffer{
                std::move(Renderer::device->allocateCommandBuffersUnique(vk::CommandBufferAllocateInfo{}.setCommandBufferCount(1).setCommandPool(*Renderer::command_pool)).front()) };
            init_command_buffer->begin(vk::CommandBufferBeginInfo{});
            vulkan::set_image_layout(*init_command_buffer, *Renderer::texture, vk::ImageAspectFlagBits::eColor, vk::ImageLayout::ePreinitialized,
                vk::ImageLayout::eShaderReadOnlyOptimal, vk::AccessFlagBits(), vk::PipelineStageFlagBits::eTopOfPipe,
                vk::PipelineStageFlagBits::eFragmentShader);
            auto& cmd = init_command_buffer;
            cmd->end();
            auto signal_semaphore = texture_prepare_semaphore.semaphore;
            auto command_submit_info = vk::CommandBufferSubmitInfo{}.setCommandBuffer(*cmd);
            auto signal_semaphore_info = vk::SemaphoreSubmitInfo{}.setSemaphore(signal_semaphore).setStageMask(vk::PipelineStageFlagBits2::eTransfer);
            Renderer::queue->submit2(vk::SubmitInfo2{}.setCommandBufferInfos(command_submit_info).setSignalSemaphoreInfos(signal_semaphore_info));
            Renderer::queue->submit2(vk::SubmitInfo2{}.setWaitSemaphoreInfos(signal_semaphore_info), texture_prepare_semaphore.fence);
            auto res = Renderer::device->waitForFences(std::array<vk::Fence, 1>{texture_prepare_semaphore.fence}, true, UINT64_MAX);
            assert(res == vk::Result::eSuccess);
        }
    }
    void init(auto&& get_surface, auto& terminal_buffer) {
        Renderer::init(std::forward<decltype(get_surface)>(get_surface),
            terminal_buffer);
        present_manager = std::make_shared<vulkan::present_manager>(Renderer::device, 10);
        set_texture_image_layout();
    }
    run_result run()
    {
        auto reused_acquire_image_semaphore = present_manager->get_next();
        auto image_index = Renderer::device->acquireNextImageKHR(
            *Renderer::swapchain, UINT64_MAX,
            reused_acquire_image_semaphore.semaphore)
            .value;

        auto& render_complete_semaphore = Renderer::render_complete_semaphores[image_index];
        auto& command_buffer = Renderer::command_buffers[image_index];

        {
            auto wait_semaphore_infos = std::array{
                vk::SemaphoreSubmitInfo{}
                    .setSemaphore(reused_acquire_image_semaphore.semaphore)
                    .setStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput),
            };
            auto submit_cmd_info = vk::CommandBufferSubmitInfo{}.setCommandBuffer(command_buffer);
            auto signal_semaphore_info = vk::SemaphoreSubmitInfo{}.setSemaphore(*render_complete_semaphore).setStageMask(vk::PipelineStageFlagBits2::eAllCommands);
            Renderer::queue->submit2(
                vk::SubmitInfo2{}
                .setWaitSemaphoreInfos(wait_semaphore_infos)
                .setCommandBufferInfos(submit_cmd_info)
                .setSignalSemaphoreInfos(signal_semaphore_info),
                reused_acquire_image_semaphore.fence);
        }

        {
            std::array<vk::Semaphore, 1> wait_semaphores{ *render_complete_semaphore };
            std::array<vk::SwapchainKHR, 1> swapchains{ *Renderer::swapchain };
            std::array<uint32_t, 1> indices{ image_index };
            vk::PresentInfoKHR present_info{ wait_semaphores, swapchains, indices };
            auto res = Renderer::queue->presentKHR(present_info);
            assert(res == vk::Result::eSuccess || res == vk::Result::eSuboptimalKHR);
        }
        return run_result::eContinue;
    }
    void notify_update() {
        present_manager->wait_all();
        Renderer::notify_update();
        set_texture_image_layout();
    }
private:
    std::shared_ptr<vulkan::present_manager> present_manager;
};
