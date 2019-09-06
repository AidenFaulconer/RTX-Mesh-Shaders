/*!
 *  \author    Viktor Zoutman
 *  \date      2019-2020
 *  \copyright GNU General Public License v3.0
 */

#include "render_window.hpp"

#include <array>

#include "../application.hpp"
#include "context.hpp"
#include "command_queue.hpp"
#include "fence.hpp"
#include "gfx_settings.hpp"

gfx::RenderWindow::RenderWindow(Context* context) : RenderTarget(context), m_frame_idx(0)
{
	auto surface_format = PickSurfaceFormat();
	auto present_mode = PickPresentMode();
	auto extent = ComputeSwapchainExtend();
	auto num_back_buffers = ComputeNumBackBuffers();

	auto capabilities = m_context->m_swapchain_support_details.m_capabilities;

	VkSwapchainCreateInfoKHR create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	create_info.surface = m_context->m_surface;
	create_info.minImageCount = num_back_buffers;
	create_info.imageFormat = surface_format.format;
	create_info.imageColorSpace = surface_format.colorSpace;
	create_info.imageExtent = extent;
	create_info.imageArrayLayers = 1;
	create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	create_info.queueFamilyIndexCount = 0;
	create_info.pQueueFamilyIndices = nullptr;
	create_info.preTransform = capabilities.currentTransform;
	create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	create_info.presentMode = present_mode;
	create_info.clipped = VK_TRUE;
	create_info.oldSwapchain = VK_NULL_HANDLE;

	if (vkCreateSwapchainKHR(m_context->m_logical_device, &create_info, nullptr, &m_swapchain) != VK_SUCCESS) {
		throw std::runtime_error("failed to create swap chain!");
	}

	GetSwapchainImages();
	CreateSwapchainImageViews();

	CreateRenderPass(VK_FORMAT_B8G8R8A8_UNORM);
	CreateFrameBuffers();
}

gfx::RenderWindow::~RenderWindow()
{
	auto logical_device = m_context->m_logical_device;

	for (auto& view : m_swapchain_image_views) {
		vkDestroyImageView(logical_device, view, nullptr);
	}
	vkDestroySwapchainKHR(logical_device, m_swapchain, nullptr);
}


void gfx::RenderWindow::AquireBackBuffer(Fence* fence)
{
	auto logical_device = m_context->m_logical_device;

	std::uint32_t new_frame_idx = 0;
	vkAcquireNextImageKHR(logical_device, m_swapchain, UINT64_MAX, fence->m_wait_semaphore, VK_NULL_HANDLE, &new_frame_idx);

	if (m_frame_idx != new_frame_idx)
	{
		std::runtime_error("Render window frame index is out of sync with the swap chain!");
	}
}

void gfx::RenderWindow::Present(CommandQueue* queue, Fence* fence)
{
	auto logical_device = m_context->m_logical_device;

	VkPresentInfoKHR present_info = {};
	present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	present_info.waitSemaphoreCount = 1;
	present_info.pWaitSemaphores = &fence->m_signal_semaphore;
	present_info.swapchainCount = 1;
	present_info.pSwapchains = &m_swapchain;
	present_info.pResults = nullptr;
	present_info.pImageIndices = &m_frame_idx;

	vkQueuePresentKHR(queue->m_queue, &present_info);

	m_frame_idx = (m_frame_idx + 1) % gfx::settings::num_back_buffers;
}

std::uint32_t gfx::RenderWindow::GetFrameIdx()
{
	return m_frame_idx;
}

VkSurfaceFormatKHR gfx::RenderWindow::PickSurfaceFormat()
{
	auto available_formats = m_context->m_swapchain_support_details.m_formats;

	for (const auto& format : available_formats)
	{
		if (format.format == gfx::settings::swapchain_format && format.colorSpace == gfx::settings::swapchain_color_space)
		{
			return format;
		}
	}

	throw std::runtime_error("Can't create swapchain with unsuported format or unsupported color space");
}

VkPresentModeKHR gfx::RenderWindow::PickPresentMode()
{
	auto available_present_modes =  m_context->m_swapchain_support_details.m_present_modes;

	for (const auto& present_mode : available_present_modes)
	{
		if (present_mode == gfx::settings::swapchain_present_mode)
		{
			return present_mode;
		}
	}

	throw std::runtime_error("Can't create swapchain with unsuported present mode");
}

VkExtent2D gfx::RenderWindow::ComputeSwapchainExtend()
{
	auto capabilities = m_context->m_swapchain_support_details.m_capabilities;
	if (capabilities.currentExtent.width != UINT32_MAX)
	{
		return capabilities.currentExtent;
	}
	else
	{
		auto app = m_context->m_app;
		VkExtent2D actual_extent = { app->GetWidth(), app->GetHeight() };

		actual_extent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actual_extent.width));
		actual_extent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actual_extent.height));

		return actual_extent;
	}
}

std::uint32_t gfx::RenderWindow::ComputeNumBackBuffers()
{
	auto capabilities = m_context->m_swapchain_support_details.m_capabilities;

	if (gfx::settings::num_back_buffers < capabilities.minImageCount || gfx::settings::num_back_buffers > capabilities.maxImageCount)
	{
		throw std::runtime_error("Invalid number of back buffers");
	}

	return gfx::settings::num_back_buffers;
}

void gfx::RenderWindow::GetSwapchainImages()
{
	auto logical_device = m_context->m_logical_device;

	auto num_back_buffers = 0u;
	vkGetSwapchainImagesKHR(logical_device, m_swapchain, &num_back_buffers, nullptr);
	m_swapchain_images.resize(num_back_buffers);
	vkGetSwapchainImagesKHR(logical_device, m_swapchain, &num_back_buffers, m_swapchain_images.data());
}


void gfx::RenderWindow::CreateSwapchainImageViews()
{
	auto logical_device = m_context->m_logical_device;

	m_swapchain_image_views.resize(gfx::settings::num_back_buffers);

	for (size_t i = 0; i < gfx::settings::num_back_buffers; i++) {
		VkImageViewCreateInfo create_info = {};
		create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		create_info.image = m_swapchain_images[i];
		create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
		create_info.format = gfx::settings::swapchain_format;
		create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		create_info.subresourceRange.baseMipLevel = 0;
		create_info.subresourceRange.levelCount = 1;
		create_info.subresourceRange.baseArrayLayer = 0;
		create_info.subresourceRange.layerCount = 1;

		if (vkCreateImageView(logical_device, &create_info, nullptr, &m_swapchain_image_views[i]) != VK_SUCCESS) {
			throw std::runtime_error("failed to create image views!");
		}
	}
}

void gfx::RenderWindow::CreateFrameBuffers()
{
	auto logical_device = m_context->m_logical_device;
	m_width = m_context->m_app->GetWidth();
	m_height = m_context->m_app->GetHeight();

	m_frame_buffers.resize(m_swapchain_image_views.size());

	for (std::size_t i = 0; i < m_swapchain_image_views.size(); i++)
	{
		VkImageView attachments[] =
		{
				m_swapchain_image_views[i]
		};

		VkFramebufferCreateInfo buffer_info = {};
		buffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		buffer_info.renderPass = m_render_pass;
		buffer_info.attachmentCount = 1;
		buffer_info.pAttachments = attachments;
		buffer_info.width = m_width;
		buffer_info.height = m_height;
		buffer_info.layers = 1;

		if (vkCreateFramebuffer(logical_device, &buffer_info, nullptr, &m_frame_buffers[i]) != VK_SUCCESS) {
			throw std::runtime_error("failed to create framebuffer!");
		}
	}
}