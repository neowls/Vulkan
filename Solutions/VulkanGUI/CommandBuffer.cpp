#include "CommandBuffer.h"
#include <stdexcept>


namespace CommandBuffer
{
	std::vector<VkCommandBuffer> commandBuffers;

	void CommandBuffer::create(VkDevice device, VkCommandPool commandPool, size_t count)
	{
		commandBuffers.resize(count);

		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = commandPool;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount = static_cast<uint32_t>(count);

		if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) != VK_SUCCESS)
		{
			throw std::runtime_error("커맨드 버퍼 할당 실패.\n");
		}
	}

	void CommandBuffer::recordAll(VkDevice device, VkRenderPass renderPass, const std::vector<VkFramebuffer>& framebuffers, VkExtent2D extent, VkPipeline graphicsPipeline)
	{
		for (size_t i = 0; i < commandBuffers.size(); ++i)
		{
			VkCommandBufferBeginInfo beginInfo{};
			beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

			if (vkBeginCommandBuffer(commandBuffers[i], &beginInfo) != VK_SUCCESS)
			{
				throw std::runtime_error("커맨드 버퍼 기록 시작 실패.\n");
			}

			VkRenderPassBeginInfo renderPassInfo{};
			renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			renderPassInfo.renderPass = renderPass;
			renderPassInfo.framebuffer = framebuffers[i];
			renderPassInfo.renderArea.offset = { 0,0 };
			renderPassInfo.renderArea.extent = extent;

			VkClearValue clearColor = { {{0.0f, 0.0f, 0.0f, 1.0f}} };
			renderPassInfo.clearValueCount = 1;
			renderPassInfo.pClearValues = &clearColor;

			vkCmdBeginRenderPass(commandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
			vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
			vkCmdEndRenderPass(commandBuffers[i]);

			if (vkEndCommandBuffer(commandBuffers[i]) != VK_SUCCESS)
			{
				throw std::runtime_error("커맨드 버퍼 기록 종료 실패.\n");
			}
		}
	}

	void CommandBuffer::cleanup(VkDevice device, VkCommandPool commandPool)
	{
		if (!commandBuffers.empty())
		{
			vkFreeCommandBuffers(device, commandPool, static_cast<uint32_t>(commandBuffers.size()), commandBuffers.data());
			commandBuffers.clear();
		}
	}
}