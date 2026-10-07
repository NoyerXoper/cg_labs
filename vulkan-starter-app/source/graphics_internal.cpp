#define GLM_ENABLE_EXPERIMENTAL

#include "graphics_internal.hpp"

#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <cstring>
#include <ranges>
#include <glm/gtx/string_cast.hpp>

#include <vulkan/vulkan.h>

#include <GLFW/glfw3.h>

#include <glm/gtx/matrix_operation.hpp>
#include <glm/gtx/euler_angles.hpp>

#include <VkBootstrap.h>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4100 4189 4324)
#endif // _MSC_VER
#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>
#ifdef _MSC_VER
#pragma warning(pop)
#endif // _MSC_VER

#include <backends/imgui_impl_vulkan.h>

namespace graphics::internal {

bool is_perspective = true;

std::vector<uint32_t> VertexIndexStorage::index_data = {};
std::vector<Vertex> VertexIndexStorage::vertex_data = {};
std::vector<Pyramid*> VertexIndexStorage::objects = {};

void VertexIndexStorage::registerFigure(Pyramid& obj) {
	objects.push_back(&obj);
}

void VertexIndexStorage::assembleBuffers() noexcept {
	std::size_t i = 0;
	for(auto obj_ptr: objects) {
		for(const Vertex& vert: obj_ptr->getVertex()) {
			vertex_data.push_back(vert);
		}

		for(uint32_t idx: obj_ptr->getIndex()) {
			index_data.push_back(idx + i);
		}

		i += obj_ptr->getVertex().size();
	}
}

const std::vector<Vertex>& VertexIndexStorage::vertexData() noexcept {
	return vertex_data;
}
const std::vector<uint32_t>& VertexIndexStorage::indexData() noexcept {
	return index_data;
}

const std::vector<Pyramid*>& VertexIndexStorage::objectsData() noexcept {
	return objects;
}

Pyramid::Pyramid(const std::array<Vertex, 4>& verticies): center_(0), verticies_(verticies) {
	for(Vertex& vert: verticies_) {
		center_ += vert.position;
	}
	center_ /= 4;
	float maxAbs = 0;
	for (Vertex& vert: verticies_) {
		vert.position -= center_;
		maxAbs = std::max(maxAbs, std::abs(vert.position.x));
		maxAbs = std::max(maxAbs, std::abs(vert.position.y));
		maxAbs = std::max(maxAbs, std::abs(vert.position.z));
	}
	if (maxAbs > 0) {
		for (Vertex& vert: verticies_) {
			vert.color = vert.position / maxAbs;
			vert.color = (vert.color + 1.0f) * 0.5f;
		}
	}

	constexpr std::array<std::array<uint32_t, 3>, 4> triangles = {{
		{0, 1, 2}, {0, 1, 3}, {0, 2, 3}, {1, 2, 3}
	}};
	std::size_t i = 0;
	for(auto [v0, v1, v2]: triangles) {
		glm::vec3 vec1 = verticies_[v1].position - verticies_[v0].position;
		glm::vec3 vec2 = verticies_[v2].position - verticies_[v0].position;
		glm::vec3 norm = glm::cross(vec1, vec2);
		glm::vec3 center = (verticies_[v0].position + verticies_[v1].position + verticies_[v2].position) / 3.0f;

		if (glm::dot(norm, center) > 0) {
			indexes_[i] = v1;
			indexes_[i + 1] = v0;
			indexes_[i + 2] = v2;
		} else {
			indexes_[i] = v0;
			indexes_[i + 1] = v1;
			indexes_[i + 2] = v2;
		}
		i += 3;
	}
}


const std::array<Vertex, 4>& Pyramid::getVertex() const noexcept {
	return verticies_;
}

const std::array<uint32_t, 12>& Pyramid::getIndex() const noexcept {
	return indexes_;
}

void Pyramid::setAngles(const glm::vec3& angles) noexcept {
	static constexpr auto normalise = [](float angle) {
		angle = std::fmod(angle, 360);
		if (angle < 0) {
			angle += 360;
		}
		return angle;
	};
	angles_ = {
		normalise(angles.x),
		normalise(angles.y),
		normalise(angles.z),
	};
}

void Pyramid::setPos(const glm::vec3& pos) noexcept {
	center_ = pos;
}

void Pyramid::setDeformation(const glm::vec3& deformation) noexcept {
	deformation_ = deformation;
}

void Pyramid::setColorMultipiles(const glm::vec3& mult) noexcept {
	color_multiply_ = mult;
}

const glm::vec3& Pyramid::getAngles() const noexcept {
	return angles_;
}

const glm::vec3& Pyramid::getPos() const noexcept {
	return center_;
}

const glm::vec3& Pyramid::getDeformation() const noexcept {
	return deformation_;
}

const glm::vec3& Pyramid::getColorMultiplier() const noexcept {
	return color_multiply_;
}

glm::mat4 Pyramid::getMatrixTransform() const {
	return (is_perspective ? getPerspective() : getOrtho()) * getModel();
}

glm::mat4 Pyramid::getRotationMatrix() const {
	return glm::eulerAngleXYZ(
        glm::radians(angles_.x),
        glm::radians(angles_.y),
        glm::radians(angles_.z)
    );
}

glm::mat4 Pyramid::getDeformationMatrix() const {
	return glm::diagonal4x4(glm::vec4(deformation_, 1));
}

glm::mat4 Pyramid::getPositionMatrix() const {
	return glm::translate(glm::mat4(1), center_);
}

glm::mat4 Pyramid::getModel() const {
	return getPositionMatrix() * getRotationMatrix() * getDeformationMatrix();
}

glm::mat4 Pyramid::getOrtho() const {
	return glm::orthoRH_ZO(LEFT, RIGHT, BOTTOM, TOP, NEAR, FAR);
}

glm::mat4 Pyramid::getPerspective() const {
	return glm::perspectiveRH_ZO(glm::radians(FOV_Y), ASPECT, NEAR, FAR);
}

namespace {

VkInstance vk_instance;
uint32_t vk_api_version;
VkSurfaceKHR vk_surface;

VkSwapchainKHR vk_swapchain;
std::vector<VkImage> vk_swapchain_images;
std::vector<VkImageView> vk_swapchain_image_views;
uint32_t vk_swapchain_current_image;

uint32_t vk_swapchain_resize_width;
uint32_t vk_swapchain_resize_height;
bool vk_swapchain_resize_require;

VkFormat vk_depth_buffer_format = VK_FORMAT_UNDEFINED;
VkImage vk_image_depth_buffer;
VmaAllocation vma_allocation_depth_buffer;
VkImageView vk_image_view_depth_buffer;

std::vector<VkFramebuffer> vk_framebuffers;

VkSemaphore vk_semaphore_image_available;
std::vector<VkSemaphore> vk_semaphores_image_finished;
VkFence vk_fence_frame_in_flight;

VkCommandPool vk_command_pool;
VkCommandBuffer vk_command_buffer;

VkDescriptorPool vk_imgui_descriptor_pool;
VkRenderPass vk_imgui_render_pass;
std::vector<VkFramebuffer> vk_imgui_framebuffers;
VkCommandPool vk_imgui_command_pool;
VkCommandBuffer vk_imgui_command_buffer;

std::vector<VkBuffer> vk_uniform_global_buffers;
std::vector<GlobalUniforms*> vk_uniform_global_memory;
std::vector<VmaAllocation> vk_uniform_buffer_global_allocations;
uint32_t vk_uniform_global_memory_size;
VkDescriptorSetLayout vk_descriptor_set_layout;
VkDeviceSize uniform_buffer_align;
VkDescriptorPool vk_uniform_buffer_descriptor_pool;
// std::vector<VkDescriptorSet> vk_uniform_buffer_descriptor_set;

VkShaderModule loadShaderModule(const char path[]) {
	std::ifstream file(path, std::ios::binary | std::ios::ate);
	if (!file) {
		std::cerr << "Couldn't open shader file:" << path << "\n";
		return nullptr;
	}
	const std::size_t size = static_cast<std::size_t>(file.tellg());
	std::vector<uint32_t> data(size / sizeof(uint32_t));

	file.seekg(0);
	file.read(reinterpret_cast<char*>(data.data()), size);
	file.close();
	
	VkShaderModuleCreateInfo info{
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = size,
		.pCode = data.data()
	};

	VkShaderModule shader;

	if (vkCreateShaderModule(context.device, &info, nullptr, &shader) != VK_SUCCESS) {
		std::cerr << "Couldn't load shader module\n";
		return nullptr;
	}
	return shader;

}

VkFormat selectDepthFormat(VkPhysicalDevice physical_device) {
	// Prefer the original format and preserve stencil support in the fallback.
	const VkFormat candidates[] = {
		VK_FORMAT_D24_UNORM_S8_UINT,
		VK_FORMAT_D32_SFLOAT_S8_UINT,
	};

	for (VkFormat format : candidates) {
		VkFormatProperties properties{};
		vkGetPhysicalDeviceFormatProperties(physical_device, format, &properties);
		if (properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
			return format;
		}
	}

	return VK_FORMAT_UNDEFINED;
}

bool initializeImGUI() {
	const VkDescriptorPoolSize descriptor_pool_sizes[] = {
		{
			.type = VK_DESCRIPTOR_TYPE_SAMPLER,
			.descriptorCount = IMGUI_IMPL_VULKAN_MINIMUM_SAMPLER_POOL_SIZE,
		},
		{
			.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
			.descriptorCount = IMGUI_IMPL_VULKAN_MINIMUM_SAMPLED_IMAGE_POOL_SIZE,
		},
	};

	const VkDescriptorPoolCreateInfo descriptor_pool = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
		.maxSets = uint32_t(vk_swapchain_images.size()),
		.poolSizeCount = sizeof(descriptor_pool_sizes) / sizeof(descriptor_pool_sizes[0]),
		.pPoolSizes = descriptor_pool_sizes,
	};

	if (vkCreateDescriptorPool(context.device, &descriptor_pool, nullptr,
							   &vk_imgui_descriptor_pool) != VK_SUCCESS) {
		std::cerr << "Failed to create Vulkan descriptor pool for ImGUI rendering\n";
		return false;
	}

	const VkAttachmentDescription render_pass_attachment = {
		.format = context.swapchain_format,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
		.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
		.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
		.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
	};

	const VkAttachmentReference render_pass_attachment_ref = {
		.attachment = 0,
		.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
	};

	const VkSubpassDescription render_pass_subpass = {
		.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
		.colorAttachmentCount = 1,
		.pColorAttachments = &render_pass_attachment_ref,
	};

	const VkSubpassDependency render_pass_dependency = {
		.srcSubpass = VK_SUBPASS_EXTERNAL,
		.dstSubpass = 0,
		.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
						 VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
	};

	const VkRenderPassCreateInfo render_pass = {
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		.attachmentCount = 1,
		.pAttachments = &render_pass_attachment,
		.subpassCount = 1,
		.pSubpasses = &render_pass_subpass,
		.dependencyCount = 1,
		.pDependencies = &render_pass_dependency,
	};

	if (vkCreateRenderPass(context.device, &render_pass, nullptr,
						   &vk_imgui_render_pass) != VK_SUCCESS) {
		std::cerr << "Failed to create Vulkan render pass for ImGUI rendering\n";
		return false;
	}

	const uint32_t swapchain_images_count = uint32_t(vk_swapchain_images.size());

	vk_imgui_framebuffers.resize(swapchain_images_count);

	VkFramebufferCreateInfo framebuffer = {
		.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		.renderPass = vk_imgui_render_pass,
		.attachmentCount = 1,
		.width = context.swapchain_extent.width,
		.height = context.swapchain_extent.height,
		.layers = 1,
	};

	for (uint32_t i = 0; i < swapchain_images_count; ++i) {
		framebuffer.pAttachments = &vk_swapchain_image_views[i];

		if (vkCreateFramebuffer(context.device, &framebuffer, nullptr,
								&vk_imgui_framebuffers[i]) != VK_SUCCESS) {
			std::cerr << "Failed to create Vulkan framebuffer #" << i << " for ImGUI rendering\n";
			return false;
		}
	}

	const VkCommandPoolCreateInfo command_pool = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		.queueFamilyIndex = context.graphics_queue_index,
	};

	if (vkCreateCommandPool(context.device, &command_pool, nullptr,
							&vk_imgui_command_pool) != VK_SUCCESS) {
		std::cerr << "Failed to create Vulkan command pool for ImGUI rendering\n";
		return false;
	}

	const VkCommandBufferAllocateInfo command_buffer = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = vk_imgui_command_pool,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = 1,
	};

	if (vkAllocateCommandBuffers(context.device, &command_buffer,
								 &vk_imgui_command_buffer) != VK_SUCCESS) {
		std::cerr << "Failed to create Vulkan command buffer for ImGUI rendering\n";
		return false;
	}

	ImGui_ImplVulkan_InitInfo init = {
		.ApiVersion = vk_api_version,
		.Instance = vk_instance,
		.PhysicalDevice = context.physical_device,
		.Device = context.device,
		.QueueFamily = context.graphics_queue_index,
		.Queue = context.graphics_queue,
		.DescriptorPool = vk_imgui_descriptor_pool,
		.MinImageCount = swapchain_images_count,
		.ImageCount = swapchain_images_count,
		.PipelineInfoMain = {
			.RenderPass = vk_imgui_render_pass,
		},
	};

	return ImGui_ImplVulkan_Init(&init);
}

void drawImGUI() {
	vkResetCommandBuffer(vk_imgui_command_buffer, 0);

	const VkCommandBufferBeginInfo command_buffer_begin = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
	};

	vkBeginCommandBuffer(vk_imgui_command_buffer, &command_buffer_begin);

	const VkRenderPassBeginInfo render_pass_begin = {
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.renderPass = vk_imgui_render_pass,
		.framebuffer = vk_imgui_framebuffers[vk_swapchain_current_image],
		.renderArea = { .extent = context.swapchain_extent },
	};

	vkCmdBeginRenderPass(vk_imgui_command_buffer, &render_pass_begin, VK_SUBPASS_CONTENTS_INLINE);

	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), vk_imgui_command_buffer);

	vkCmdEndRenderPass(vk_imgui_command_buffer);

	vkEndCommandBuffer(vk_imgui_command_buffer);
}

bool rebuildSwapchain(uint32_t width, uint32_t height) {
	vkQueueWaitIdle(context.graphics_queue);

	vkb::SwapchainBuilder sb(context.physical_device, context.device, vk_surface,
	                         context.graphics_queue_index, context.graphics_queue_index);

	auto sb_result = sb.set_desired_extent(width, height)
	                   .use_default_format_selection()
					   .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
					   .use_default_image_usage_flags()
					   .set_old_swapchain(vk_swapchain)
					   .build();
	if (!sb_result) {
		std::cerr << sb_result.error().message() << '\n';
	}

	auto vkb_swapchain = sb_result.value();

	for (size_t i = 0, n = vk_swapchain_image_views.size(); i < n; ++i) {
		vkDestroyFramebuffer(context.device, vk_imgui_framebuffers[i], nullptr);
		vkDestroyFramebuffer(context.device, vk_framebuffers[i], nullptr);
		vkDestroyImageView(context.device, vk_swapchain_image_views[i], nullptr);
	}

	vkDestroySwapchainKHR(context.device, vk_swapchain, nullptr);

	vk_swapchain = vkb_swapchain.swapchain;
	context.swapchain_format = vkb_swapchain.image_format;
	context.swapchain_extent = vkb_swapchain.extent;

	auto swapchain_images = vkb_swapchain.get_images().value();
	auto swapchain_image_views = vkb_swapchain.get_image_views().value();

	vk_swapchain_images = std::move(swapchain_images);
	vk_swapchain_image_views = std::move(swapchain_image_views);

	vkDestroyImageView(context.device, vk_image_view_depth_buffer, nullptr);
	vmaDestroyImage(context.allocator, vk_image_depth_buffer, vma_allocation_depth_buffer);

	const VkImageCreateInfo depth_buffer = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = vk_depth_buffer_format,
		.extent = { context.swapchain_extent.width, context.swapchain_extent.height, 1 },
		.mipLevels = 1,
		.arrayLayers = 1,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
	};

	const VmaAllocationCreateInfo depth_buffer_allocation = {
		.usage = VMA_MEMORY_USAGE_AUTO,
	};

	if (vmaCreateImage(context.allocator, &depth_buffer, &depth_buffer_allocation,
					   &vk_image_depth_buffer, &vma_allocation_depth_buffer,
					   nullptr) != VK_SUCCESS) {
		std::cerr << "Failed to allocate and create Vulkan image for depth buffer\n";
		return false;
	}

	const VkImageViewCreateInfo depth_buffer_view = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = vk_image_depth_buffer,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = vk_depth_buffer_format,
		.subresourceRange = {
			.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1,
		},
	};

	if (vkCreateImageView(context.device, &depth_buffer_view, nullptr,
						  &vk_image_view_depth_buffer) != VK_SUCCESS) {
		std::cerr << "Failed to create Vulkan image view for depth buffer\n";
		return false;
	}

	const uint32_t swapchain_images_count = uint32_t(vk_swapchain_images.size());

	VkImageView framebuffer_attachments[] = {
		VK_NULL_HANDLE,
		vk_image_view_depth_buffer,
	};

	const VkFramebufferCreateInfo framebuffer = {
		.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		.renderPass = context.render_pass,
		.attachmentCount = sizeof(framebuffer_attachments) / sizeof(framebuffer_attachments[0]),
		.pAttachments = framebuffer_attachments,
		.width = context.swapchain_extent.width,
		.height = context.swapchain_extent.height,
		.layers = 1,
	};

	vk_framebuffers.resize(swapchain_images_count);

	for (uint32_t i = 0; i < swapchain_images_count; ++i) {
		framebuffer_attachments[0] = vk_swapchain_image_views[i];

		if (vkCreateFramebuffer(context.device, &framebuffer, nullptr,
								&vk_framebuffers[i]) != VK_SUCCESS) {
			std::cerr << "Failed to create Vulkan framebuffer #" << i << '\n';
			return false;
		}
	}

	vk_imgui_framebuffers.resize(swapchain_images_count);

	VkFramebufferCreateInfo imgui_framebuffer = {
		.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		.renderPass = vk_imgui_render_pass,
		.attachmentCount = 1,
		.width = context.swapchain_extent.width,
		.height = context.swapchain_extent.height,
		.layers = 1,
	};

	for (uint32_t i = 0; i < swapchain_images_count; ++i) {
		imgui_framebuffer.pAttachments = &vk_swapchain_image_views[i];

		if (vkCreateFramebuffer(context.device, &imgui_framebuffer, nullptr,
								&vk_imgui_framebuffers[i]) != VK_SUCCESS) {
			std::cerr << "Failed to create Vulkan framebuffer #" << i << " for ImGUI rendering\n";
			return false;
		}
	}

	vk_swapchain_resize_require = false;

	return true;
}

// TRIAG

VmaAllocation vertex_buffer_allocation = nullptr;

bool createVertexBufferForShaders() {
	context.vertex_buffer = nullptr;

	VkBufferCreateInfo buffer_info {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = VertexIndexStorage::vertexData().size() * sizeof(Vertex),
		.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE
	};

	VmaAllocationCreateInfo alloc_info {
		.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
		.usage = VMA_MEMORY_USAGE_AUTO
	};
	if (vmaCreateBuffer(context.allocator, &buffer_info, &alloc_info, &context.vertex_buffer, &vertex_buffer_allocation, nullptr) != VK_SUCCESS) {
		std::cerr << "Failed to allocate vertex buffer\n";
        return false;
	}

	if(vmaMapMemory(context.allocator, vertex_buffer_allocation, (void**)&context.vertex_memory) != VK_SUCCESS) {
		std::cerr << "Failed to map vertex buffer\n";
        return false;
	}

	std::memcpy(context.vertex_memory, VertexIndexStorage::vertexData().data(), VertexIndexStorage::vertexData().size() * sizeof(Vertex));
    return true;
}

VmaAllocation index_buffer_allocation = nullptr;

bool createIndexBufferForShaders() {
	context.index_buffer = nullptr;

	VkDeviceSize idx_size_bytes = VertexIndexStorage::indexData().size() * sizeof(VertexIndexStorage::indexData().front());

	VkBufferCreateInfo buffer_info {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = idx_size_bytes,
		.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE
	};

	VmaAllocationCreateInfo alloc_info {
		.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
		.usage = VMA_MEMORY_USAGE_AUTO
	};

	VmaAllocationInfo alloc_result {};
	if (vmaCreateBuffer(context.allocator, &buffer_info, &alloc_info, &context.index_buffer, &index_buffer_allocation, &alloc_result) != VK_SUCCESS) {
		std::cerr << "Failed to create vertex buffer\n";
        return false;
	}
	std::memcpy(alloc_result.pMappedData, VertexIndexStorage::indexData().data(), idx_size_bytes);
    return true;
}

bool createUniformBuffer() {
	// TOCHANGE - gota use multiple uniform structs
	const VkBufferCreateInfo global_uniform_buffer_info = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = (sizeof(GlobalUniforms) + 0xf) & ~0xf,
		.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE
	};

	const VmaAllocationCreateInfo global_uniform_buffer_allocation_info	{
		.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
		.usage = VMA_MEMORY_USAGE_AUTO
	};
	uint32_t N = VertexIndexStorage::objectsData().size();
	vk_uniform_global_buffers.resize(N);
	vk_uniform_global_memory.resize(N);
	context.uniform_descriptor_sets.resize(N);
	vk_uniform_buffer_global_allocations.resize(N);
	for (std::size_t i = 0; i < vk_uniform_global_buffers.size(); ++i) {
		VmaAllocationInfo alloc_info{};

		if (vmaCreateBuffer(context.allocator, &global_uniform_buffer_info, &global_uniform_buffer_allocation_info, &vk_uniform_global_buffers[i], &vk_uniform_buffer_global_allocations[i], &alloc_info) != VK_SUCCESS) {
			std::cerr << "Couldn't create global uniform buffer";
			return false;
		}

		vk_uniform_global_memory[i] = reinterpret_cast<GlobalUniforms*>(alloc_info.pMappedData);
	}

	VkDescriptorSetLayoutBinding descriptor_set_bindings[] = {
		{
			.binding = 0,
			.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT
		}
	};

	const VkDescriptorSetLayoutCreateInfo descripotr_set_layout_info {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.bindingCount = sizeof(descriptor_set_bindings) / sizeof(descriptor_set_bindings[0]),
		.pBindings = descriptor_set_bindings
	};

	if (vkCreateDescriptorSetLayout(context.device, &descripotr_set_layout_info, nullptr, &vk_descriptor_set_layout) != VK_SUCCESS) {
		std::cerr << "Couldn't create descriptor set layout\n";
		return false;
	} 

	const VkDescriptorPoolSize descriptor_pool_sizes[] = {
		{
			.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.descriptorCount = N,
		}
	};

	const VkDescriptorPoolCreateInfo descriptor_pool_info {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.maxSets = N,
		.poolSizeCount = sizeof(descriptor_pool_sizes) / sizeof(descriptor_pool_sizes[0]),
		.pPoolSizes = descriptor_pool_sizes
	};

	if (vkCreateDescriptorPool(context.device, &descriptor_pool_info, nullptr, &vk_uniform_buffer_descriptor_pool) != VK_SUCCESS) {
		std::cerr << "Couldn't create desc pool for unifrom dyn buffer\n";
		return false;
	}

	std::vector<VkDescriptorSetLayout> layouts(N, vk_descriptor_set_layout);

	VkDescriptorSetAllocateInfo descriptor_set_allocate_info {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.descriptorPool = vk_uniform_buffer_descriptor_pool,
		.descriptorSetCount = N,
		.pSetLayouts = layouts.data()
	};

	if(vkAllocateDescriptorSets(context.device, &descriptor_set_allocate_info, context.uniform_descriptor_sets.data()) != VK_SUCCESS) {
		std::cerr << "Couldn't allocate desc set\n";
		return false;
	}

	for(auto&& [buffer, set]: std::views::zip(vk_uniform_global_buffers, context.uniform_descriptor_sets)) {
		VkDescriptorBufferInfo buffer_desc_info {
			.buffer = buffer,
			.offset = 0,
			.range = sizeof(GlobalUniforms)
		};

		VkWriteDescriptorSet write_desc {
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = set,
			.dstBinding = 0,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.pBufferInfo = &buffer_desc_info
		};

		vkUpdateDescriptorSets(context.device, 1, &write_desc, 0, nullptr);
	}

	return true;
}

bool createTriagonalPipeLine() {
	VkShaderModule vertex_shader = loadShaderModule(SHADER_DIR "/shader.vert.spv");
	VkShaderModule fragment_shader = loadShaderModule(SHADER_DIR "/shader.frag.spv");

	if (!vertex_shader || !fragment_shader) {
		std::cerr << "Failed to load shaders\n";
		return false;
	}
	VkPipelineShaderStageCreateInfo stages[2] = {
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_VERTEX_BIT,
			.module = vertex_shader,
			.pName = "main"
		},
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
			.module = fragment_shader,
			.pName = "main"
		}
	};


	VkVertexInputBindingDescription binding {
		.binding = 0,
		.stride = sizeof(Vertex),
		.inputRate = VK_VERTEX_INPUT_RATE_VERTEX
	};

	VkVertexInputAttributeDescription attributes[2] = {
		{
			.location = 0,
			.binding = 0,
			.format = VK_FORMAT_R32G32B32_SFLOAT,
			.offset = offsetof(Vertex, position)
		},
		{
			.location = 1,
			.binding = 0,
			.format = VK_FORMAT_R32G32B32_SFLOAT,
			.offset = offsetof(Vertex, color)

		}
	};

	VkPipelineVertexInputStateCreateInfo vertex_input_info {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.vertexBindingDescriptionCount = 1,
		.pVertexBindingDescriptions = &binding,
		.vertexAttributeDescriptionCount = 2,
		.pVertexAttributeDescriptions = attributes
	};

	VkPipelineInputAssemblyStateCreateInfo input_assembly_info {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
		.primitiveRestartEnable = false	
	};

	VkPipelineViewportStateCreateInfo viewport_state_info  {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.viewportCount = 1,
		.scissorCount = 1
	};

	VkDynamicState dynamic_states[] = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR
	};
	VkPipelineDynamicStateCreateInfo dynamic_state_info {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.dynamicStateCount = 2,
		.pDynamicStates = dynamic_states
	};

	VkPipelineRasterizationStateCreateInfo raster {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.polygonMode = VK_POLYGON_MODE_FILL,
		.cullMode = VK_CULL_MODE_BACK_BIT,
		.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
		.lineWidth = 1.0f
	};

	VkPipelineDepthStencilStateCreateInfo depth_stencil{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		.depthTestEnable = true,
		.depthWriteEnable = true,
		.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
		.depthBoundsTestEnable = false,
		.stencilTestEnable = false,
	};

	VkPipelineColorBlendAttachmentState blend_attachment{
		.blendEnable = false,
		.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
	};

    VkPipelineColorBlendStateCreateInfo color_blend{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.attachmentCount = 1,
		.pAttachments = &blend_attachment
	};

	VkPipelineLayoutCreateInfo layout_info{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = 1,
		.pSetLayouts = &vk_descriptor_set_layout
	};
    if (vkCreatePipelineLayout(context.device, &layout_info, nullptr,
                               &context.pipeline_layout) != VK_SUCCESS) {
        std::cerr << "Failed to create pipeline layout\n";
        return false;
    }

	VkPipelineMultisampleStateCreateInfo multisample{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
	};

    VkGraphicsPipelineCreateInfo pipeline_info{
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.stageCount = 2,
		.pStages = stages,
		.pVertexInputState = &vertex_input_info,
		.pInputAssemblyState = &input_assembly_info,
		.pViewportState = &viewport_state_info,
		.pRasterizationState = &raster,
		.pMultisampleState = &multisample,
		.pDepthStencilState = &depth_stencil,
		.pColorBlendState = &color_blend,
		.pDynamicState = &dynamic_state_info,
		.layout = context.pipeline_layout,
		.renderPass = context.render_pass,
		.subpass = 0,
	};

    if (vkCreateGraphicsPipelines(context.device, VK_NULL_HANDLE, 1,
                                  &pipeline_info, nullptr,
                                  &context.pipeline) != VK_SUCCESS) {
        std::cerr << "Failed to create graphics pipeline\n";
        return false;
    }

    vkDestroyShaderModule(context.device, vertex_shader, nullptr);
    vkDestroyShaderModule(context.device, fragment_shader, nullptr);

    return true;
}
// END TRIAG
std::vector<Pyramid> pyramids;

void registerPyramidsToStorage() {
	for (Pyramid& pir: pyramids) {
		VertexIndexStorage::registerFigure(pir);
	}
}

} // namespace

Context context;

void updateGlobalUniformBuffers() {
	std::size_t N = context.uniform_descriptor_sets.size();

	for(std::size_t i = 0; i < N; ++i) {
		vk_uniform_global_memory[i]->matrix = VertexIndexStorage::objectsData()[i]->getMatrixTransform();
		vk_uniform_global_memory[i]->color_multiply = VertexIndexStorage::objectsData()[i]->getColorMultiplier();
		vmaFlushAllocation(context.allocator, vk_uniform_buffer_global_allocations[i], 0, sizeof(GlobalUniforms));
	}
}

void registerPyramid(const Pyramid& pir) noexcept {
	pyramids.push_back(pir);
}

bool initialize(GLFWwindow* const window) {
	vkb::InstanceBuilder ib;

	/*
	pyramids.emplace_back(std::array<Vertex, 4>{
		Vertex {{0.0f - 5,  -18.0f, -60.0f}, {1, 0, 0}},
		Vertex {{-15.0f - 5,  6.0f, -51.0f}, {0, 1, 0}},
		Vertex {{15.0f - 5,  6.0f, -51.0f}, {0, 0, 1}},
		Vertex {{0.0f - 5,  6.0f, -77.0f}, {1, 1, 1}},
	});
	*/

	registerPyramidsToStorage();

	VertexIndexStorage::assembleBuffers();

	auto ibr = ib.require_api_version(VK_MAKE_VERSION(1, 1, 0))
				 .request_validation_layers()
				 .build();

	auto vkb_instance = ibr.value();
	vk_instance = vkb_instance.instance;
	vk_api_version = vkb_instance.api_version;

	if (glfwCreateWindowSurface(vk_instance, window, nullptr, &vk_surface) != VK_SUCCESS) {
		const char *message = nullptr;
		glfwGetError(&message);
		std::cerr << message << '\n';
		return false;
	}

	vkb::PhysicalDeviceSelector pds(vkb_instance, vk_surface);

	auto pds_result = pds.prefer_gpu_device_type(vkb::PreferredDeviceType::discrete)
						 .require_present()
						 .select();
	if (!pds_result) {
		std::cerr << pds_result.error().message() << '\n';
		return false;
	}

	auto vkb_physical_device = pds_result.value();

	vk_depth_buffer_format = selectDepthFormat(vkb_physical_device.physical_device);
	if (vk_depth_buffer_format == VK_FORMAT_UNDEFINED) {
		std::cerr << "No supported depth/stencil attachment format found\n";
		return false;
	}

	vkb::DeviceBuilder db(vkb_physical_device);

	auto db_result = db.build();
	if (!db_result) {
		std::cerr << db_result.error().message() << '\n';
		return false;
	}

	auto vkb_device = db_result.value();

	context.physical_device = vkb_device.physical_device;
	context.device = vkb_device.device;

	if (auto result = vkb_device.get_queue(vkb::QueueType::graphics); result) {
		context.graphics_queue = result.value();
	} else {
		std::cerr << result.error().message() << '\n';
		return false;
	}

	if (auto result = vkb_device.get_queue_index(vkb::QueueType::graphics); result) {
		context.graphics_queue_index = result.value();
	} else {
		std::cerr << result.error().message() << '\n';
		return false;
	}

	const VmaAllocatorCreateInfo allocator = {
		.physicalDevice = context.physical_device,
		.device = context.device,
		.instance = vk_instance,
		.vulkanApiVersion = vk_api_version,
	};

	if (vmaCreateAllocator(&allocator, &context.allocator) != VK_SUCCESS) {
		std::cerr << "Failed to create Vulkan Memory Allocator\n";
		return false;
	}

	vkb::SwapchainBuilder sb(vkb_device);

	auto sb_result = sb.use_default_format_selection()
					   .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
					   .use_default_image_usage_flags()
					   .build();
	if (!sb_result) {
		std::cerr << sb_result.error().message() << '\n';
		return false;
	}

	auto vkb_swapchain = sb_result.value();

	vk_swapchain = vkb_swapchain.swapchain;
	context.swapchain_format = vkb_swapchain.image_format;
	context.swapchain_extent = vkb_swapchain.extent;
	vk_swapchain_images = vkb_swapchain.get_images().value();
	vk_swapchain_image_views = vkb_swapchain.get_image_views().value();
	vk_swapchain_current_image = UINT32_MAX;

	const uint32_t swapchain_images_count = uint32_t(vk_swapchain_images.size());

	const VkImageCreateInfo depth_buffer = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = vk_depth_buffer_format,
		.extent = { context.swapchain_extent.width, context.swapchain_extent.height, 1 },
		.mipLevels = 1,
		.arrayLayers = 1,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
	};

	const VmaAllocationCreateInfo depth_buffer_allocation = {
		.usage = VMA_MEMORY_USAGE_AUTO,
	};

	if (vmaCreateImage(context.allocator, &depth_buffer, &depth_buffer_allocation,
					   &vk_image_depth_buffer, &vma_allocation_depth_buffer,
					   nullptr) != VK_SUCCESS) {
		std::cerr << "Failed to allocate and create Vulkan image for depth buffer\n";
		return false;
	}

	const VkImageViewCreateInfo depth_buffer_view = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = vk_image_depth_buffer,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = vk_depth_buffer_format,
		.subresourceRange = {
			.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1,
		},
	};

	if (vkCreateImageView(context.device, &depth_buffer_view, nullptr,
						  &vk_image_view_depth_buffer) != VK_SUCCESS) {
		std::cerr << "Failed to create Vulkan image view for depth buffer\n";
		return false;
	}

	const VkAttachmentDescription render_pass_attachments[] = {
		{
			.format = context.swapchain_format,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		},
		{
			.format = vk_depth_buffer_format,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		},
	};

	const VkAttachmentReference render_pass_color_attachment = {
		.attachment = 0,
		.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
	};

	const VkAttachmentReference render_pass_depth_attachment = {
		.attachment = 1,
		.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
	};

	const VkSubpassDescription render_pass_subpass = {
		.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
		.colorAttachmentCount = 1,
		.pColorAttachments = &render_pass_color_attachment,
		.pDepthStencilAttachment = &render_pass_depth_attachment,
	};

	const VkRenderPassCreateInfo render_pass = {
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		.attachmentCount = sizeof(render_pass_attachments) / sizeof(render_pass_attachments[0]),
		.pAttachments = render_pass_attachments,
		.subpassCount = 1,
		.pSubpasses = &render_pass_subpass,
	};

	if (vkCreateRenderPass(context.device, &render_pass, nullptr, &context.render_pass) != VK_SUCCESS) {
		std::cerr << "Failed to create Vulkan render pass\n";
		return false;
	}
	if(!createVertexBufferForShaders()) {
		std::cerr << "Couldn't create memory buffer for vertex\n";
		return false;
	}

	if(!createIndexBufferForShaders()) {
		std::cerr << "Couldn't create memory buffer for idx\n";
		return false;
	}
	if(!createUniformBuffer()) {
		std::cerr << "Couldn't create uniform buffer\n";
		return false;

	}
	if(!createTriagonalPipeLine()) {
		std::cerr << "Couldn't create pipline for triangle\n";
		return false;
	}

	VkImageView framebuffer_attachments[] = {
		VK_NULL_HANDLE,
		vk_image_view_depth_buffer
	};

	const VkFramebufferCreateInfo framebuffer = {
		.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		.renderPass = context.render_pass,
		.attachmentCount = sizeof(framebuffer_attachments) / sizeof(framebuffer_attachments[0]),
		.pAttachments = framebuffer_attachments,
		.width = context.swapchain_extent.width,
		.height = context.swapchain_extent.height,
		.layers = 1,
	};

	vk_framebuffers.resize(swapchain_images_count);

	for (uint32_t i = 0; i < swapchain_images_count; ++i) {
		framebuffer_attachments[0] = vk_swapchain_image_views[i];

		if (vkCreateFramebuffer(context.device, &framebuffer, nullptr,
								&vk_framebuffers[i]) != VK_SUCCESS) {
			std::cerr << "Failed to create Vulkan framebuffer #" << i << '\n';
			return false;
		}
	}

	const VkSemaphoreCreateInfo semaphore = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };

	const VkFenceCreateInfo fence = {
		.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
		.flags = VK_FENCE_CREATE_SIGNALED_BIT,
	};

	vk_semaphores_image_finished.resize(swapchain_images_count);

	for (uint32_t i = 0; i < swapchain_images_count; ++i) {
		if (vkCreateSemaphore(context.device, &semaphore, nullptr,
							  &vk_semaphores_image_finished[i]) != VK_SUCCESS) {
			std::cerr << "Failed to create Vulkan semaphore #" << i << " for finished image\n";
			return false;
		}
	}

	if (vkCreateSemaphore(context.device, &semaphore, NULL,
						  &vk_semaphore_image_available) != VK_SUCCESS) {
		std::cerr << "Failed to create Vulkan semaphore for available image\n";
		return false;
	}

	if (vkCreateFence(context.device, &fence, nullptr, &vk_fence_frame_in_flight) != VK_SUCCESS) {
		std::cerr << "Failed to create Vulkan semaphore for image in flight\n";
		return false;
	}

	const VkCommandPoolCreateInfo command_pool = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		.queueFamilyIndex = context.graphics_queue_index,
	};

	if (vkCreateCommandPool(context.device, &command_pool, nullptr, &vk_command_pool) != VK_SUCCESS) {
		std::cerr << "Failed to create Vulkan command pool\n";
		return false;
	}

	const VkCommandBufferAllocateInfo command_buffers = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = vk_command_pool,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = 1,
	};

	if (vkAllocateCommandBuffers(context.device, &command_buffers, &vk_command_buffer) != VK_SUCCESS) {
		std::cerr << "Failed to allocate Vulkan command buffer\n";
		return false;
	}

	if (!initializeImGUI()) {
		std::cerr << "Failed to initialize ImGUI Vulkan rendering backend\n";
		return false;
	}

	return true;
}

void shutdown() {
	vkQueueWaitIdle(context.graphics_queue);

	ImGui_ImplVulkan_Shutdown();

	vkDestroyPipeline(context.device, context.pipeline, nullptr);
	vkDestroyPipelineLayout(context.device, context.pipeline_layout, nullptr);

	vkDestroyCommandPool(context.device, vk_imgui_command_pool, nullptr);
	for (size_t i = 0, n = vk_imgui_framebuffers.size(); i < n; ++i) {
		vkDestroyFramebuffer(context.device, vk_imgui_framebuffers[i], nullptr);
	}
	vkDestroyRenderPass(context.device, vk_imgui_render_pass, nullptr);
	vkDestroyDescriptorPool(context.device, vk_imgui_descriptor_pool, nullptr);

	vkDestroyCommandPool(context.device, vk_command_pool, nullptr);

	vkDestroyFence(context.device, vk_fence_frame_in_flight, nullptr);
	for (size_t i = 0, n = vk_swapchain_images.size(); i < n; ++i) {
		vkDestroySemaphore(context.device, vk_semaphores_image_finished[i], nullptr);
	}
	vkDestroySemaphore(context.device, vk_semaphore_image_available, nullptr);

	for (size_t i = 0, n = vk_swapchain_images.size(); i < n; ++i) {
		vkDestroyFramebuffer(context.device, vk_framebuffers[i], nullptr);
	}
	vkDestroyRenderPass(context.device, context.render_pass, nullptr);

	vkDestroyImageView(context.device, vk_image_view_depth_buffer, nullptr);
	vmaDestroyImage(context.allocator, vk_image_depth_buffer, vma_allocation_depth_buffer);

	for (size_t i = 0, n = vk_swapchain_images.size(); i < n; ++i) {
		vkDestroyImageView(context.device, vk_swapchain_image_views[i], nullptr);
	}
	vkDestroySwapchainKHR(context.device, vk_swapchain, nullptr);
	vmaUnmapMemory(context.allocator, vertex_buffer_allocation);
	vmaDestroyBuffer(context.allocator, context.vertex_buffer, vertex_buffer_allocation);
	vmaDestroyBuffer(context.allocator, context.index_buffer, index_buffer_allocation);
	for(auto&& [buffer, alloc]: std::views::zip(vk_uniform_global_buffers, vk_uniform_buffer_global_allocations)) {
		vmaDestroyBuffer(context.allocator, buffer, alloc);
	}
	vkDestroyDescriptorPool(context.device, vk_uniform_buffer_descriptor_pool, nullptr);
	vkDestroyDescriptorSetLayout(context.device, vk_descriptor_set_layout, nullptr);
	vmaDestroyAllocator(context.allocator);

	vkDestroyDevice(context.device, nullptr);

	vkDestroySurfaceKHR(vk_instance, vk_surface, nullptr);
	vkDestroyInstance(vk_instance, nullptr);
}

void resize(uint32_t width, uint32_t height) {
	if (width == 0 || height == 0) {
		return;
	}

	vk_swapchain_resize_width = width;
	vk_swapchain_resize_height = height;

	vk_swapchain_resize_require = true;
}

FrameData prepare() {
	vkWaitForFences(context.device, 1, &vk_fence_frame_in_flight, VK_TRUE, UINT64_MAX);

retry_acquire:
	switch (vkAcquireNextImageKHR(context.device, vk_swapchain, UINT64_MAX,
								  vk_semaphore_image_available, VK_NULL_HANDLE,
								  &vk_swapchain_current_image)) {
	case VK_SUCCESS:
		break;

	case VK_ERROR_OUT_OF_DATE_KHR:
		rebuildSwapchain(vk_swapchain_resize_width, vk_swapchain_resize_height);
		goto retry_acquire;

	case VK_SUBOPTIMAL_KHR:
		std::cerr << "Swapchain is suboptimal for rendering!\n";
		break;

	default:
		std::cerr << "Failed to present Vulkan swapchain image\n";
		return {};
	}

	vkResetFences(context.device, 1, &vk_fence_frame_in_flight);

	return {
		.framebuffer = vk_framebuffers[vk_swapchain_current_image],
		.command_buffer = vk_command_buffer,
	};
}

void submitAndPresent() {
	drawImGUI();

	const VkPipelineStageFlags stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	const VkCommandBuffer command_buffers[] = {
		vk_command_buffer,
		vk_imgui_command_buffer,
	};

	const VkSubmitInfo submit = {
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &vk_semaphore_image_available,
		.pWaitDstStageMask = &stage,
		.commandBufferCount = sizeof(command_buffers) / sizeof(command_buffers[0]),
		.pCommandBuffers = command_buffers,
		.signalSemaphoreCount = 1,
		.pSignalSemaphores = &vk_semaphores_image_finished[vk_swapchain_current_image],
	};

	vkQueueSubmit(context.graphics_queue, 1, &submit, vk_fence_frame_in_flight);

	const VkPresentInfoKHR present = {
		.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &vk_semaphores_image_finished[vk_swapchain_current_image],
		.swapchainCount = 1,
		.pSwapchains = &vk_swapchain,
		.pImageIndices = &vk_swapchain_current_image,
	};

	VkResult result = vkQueuePresentKHR(context.graphics_queue, &present);
	if (result == VK_ERROR_OUT_OF_DATE_KHR ||
	    result == VK_SUBOPTIMAL_KHR ||
	    vk_swapchain_resize_require) {
		rebuildSwapchain(vk_swapchain_resize_width, vk_swapchain_resize_height);
	} else if (result != VK_SUCCESS) {
		std::cerr << "Failed to present Vulkan swapchain image\n";
	}
}

} // namespace graphics::internal
