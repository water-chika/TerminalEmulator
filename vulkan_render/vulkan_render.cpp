#include "vulkan_render.hpp"

run_result vulkan_render::run()
{
    auto reused_acquire_image_semaphore = present_manager->get_next();
    auto image_index = device->acquireNextImageKHR(
                                 *swapchain, UINT64_MAX,
                                 reused_acquire_image_semaphore.semaphore)
                           .value;

    auto &render_complete_semaphore = render_complete_semaphores[image_index];
    auto &command_buffer = command_buffers[image_index];

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
        std::array<vk::Semaphore, 1> wait_semaphores{*render_complete_semaphore};
        std::array<vk::SwapchainKHR, 1> swapchains{*swapchain};
        std::array<uint32_t, 1> indices{image_index};
        vk::PresentInfoKHR present_info{wait_semaphores, swapchains, indices};
        assert(queue->presentKHR(present_info) == vk::Result::eSuccess);
    }
    return run_result::eContinue;
}