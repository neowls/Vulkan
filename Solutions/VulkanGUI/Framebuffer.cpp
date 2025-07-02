#include <stdexcept>
#include "Framebuffer.h"
#include "GraphicsPipeline.h"

namespace Framebuffer
{
    std::vector<VkFramebuffer> framebuffers;

    void setup(VkDevice device, VkExtent2D extent, VkRenderPass renderPass, const std::vector<VkImageView>& imageViews)
    {
        framebuffers.resize(imageViews.size());
        for (size_t i = 0; i < imageViews.size(); ++i)
        {
            VkImageView attachments[] = { imageViews[i] };
            VkFramebufferCreateInfo info{};
            info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            info.renderPass = renderPass;
            info.attachmentCount = 1;
            info.pAttachments = attachments;
            info.width = extent.width;
            info.height = extent.height;
            info.layers = 1;


            if (vkCreateFramebuffer(device, &info, nullptr, &framebuffers[i]) != VK_SUCCESS)
                throw std::runtime_error("프레임버퍼 생성 실패");
        }
    }

    void cleanup(VkDevice device)
    {
        for (auto fb : framebuffers)
            vkDestroyFramebuffer(device, fb, nullptr);
        framebuffers.clear();
    }
}