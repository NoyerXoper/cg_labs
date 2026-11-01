#pragma once

#include <expected>
#include <string>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>

#include <cstdint>
#include <array>
#include <vector>
#include <memory>
#include <utility>

#include <vulkan/vulkan_core.h>

#include <vk_mem_alloc.h>

struct GLFWwindow;

namespace graphics::internal {

constexpr float FAR = 100;
constexpr float NEAR = 0.1;

constexpr float LEFT = -100;
constexpr float RIGHT = 100;

constexpr float TOP = -100;
constexpr float BOTTOM = 100;

constexpr float FOV_Y = 45.0f;
constexpr float ASPECT = 16.0f/9.0f;

extern bool is_perspective;

struct Vertex {
	glm::vec3 position;
	glm::vec3 color;
};

struct GlobalUniforms {
	glm::mat4 perspective;
	glm::mat4 view;
	glm::mat4 model;
	glm::vec3 color_multiply;
};

class Camera {
public:
	Camera(float speed, float sensitivity) noexcept;

	glm::mat4 getView() const noexcept;	

	void updateAngles(float dx, float dy) noexcept;
	void updatePos(glm::vec3 move, float dt) noexcept;

	glm::vec3 forward() const noexcept;
	glm::vec3 up() const noexcept;
	glm::vec3 right() const noexcept;
private:
	glm::vec3 pos{0};
	glm::quat orientation{1, 0, 0, 0};

	static constexpr float pitch_limit = glm::radians(89.0f);
	float move_speed;
	float sensitivity;
};

class Pyramid;

class VertexIndexStorage {
public:
	static void registerFigure(Pyramid& pyramid);
	static void assembleBuffers() noexcept;
	static const std::vector<Vertex>& vertexData() noexcept;
	static const std::vector<uint32_t>& indexData() noexcept;
	static const std::vector<Pyramid*>& objectsData() noexcept;

private:
	static std::vector<Vertex> vertex_data;
	static std::vector<uint32_t> index_data;
	static std::vector<Pyramid*> objects;
};

class Pyramid {
public:
	Pyramid(const std::array<Vertex, 4>& vertcies); 

	const std::array<Vertex, 4>& getVertex() const noexcept;
	const std::array<uint32_t, 12>& getIndex() const noexcept;

	void setAngles(const glm::vec3& new_angles) noexcept;
	void setPos(const glm::vec3& new_pos) noexcept;
	void setDeformation(const glm::vec3& deformation) noexcept;	
	void setColorMultipiles(const glm::vec3& mult) noexcept;

	const glm::vec3& getAngles() const noexcept;
	const glm::vec3& getPos() const noexcept;
	const glm::vec3& getDeformation() const noexcept;	

	const glm::vec3& getColorMultiplier() const noexcept;	

	glm::mat4 getModel() const;

	glm::mat4 getOrthoOrPerspective() const;
private:
	glm::mat4 getRotationMatrix() const;
	glm::mat4 getPositionMatrix() const;
	glm::mat4 getDeformationMatrix() const;

	glm::mat4 getOrtho() const;

	glm::mat4 getPerspective() const;
	glm::vec3 center_;
	glm::vec3 angles_{0, 0, 0};
	glm::vec3 deformation_{1, 1, 1};
	glm::vec3 color_multiply_{0.5, 0.5, 0.5};
	std::array<Vertex, 4> verticies_;
	std::array<uint32_t, 12> indexes_;
};

struct Context {
	VkPhysicalDevice physical_device;
	VkDevice device;

	VmaAllocator allocator;

	VkQueue graphics_queue;
	uint32_t graphics_queue_index;

	VkFormat swapchain_format;
	VkExtent2D swapchain_extent;

	VkRenderPass render_pass;
	// TRIAG
	VkPipeline pipeline;
	VkPipelineLayout pipeline_layout;
	VkBuffer vertex_buffer;
	VkBuffer index_buffer;

	Vertex* vertex_memory;
	uint32_t* index_memory;

	Camera cam{7.0f, 0.003};
	// END TRIAG
};

struct FrameData {
	VkFramebuffer framebuffer;
	VkCommandBuffer command_buffer;
};

template<typename T>
class UniformBufferHandle {
public:
	UniformBufferHandle() = default;
    UniformBufferHandle(const UniformBufferHandle&) = delete;
    UniformBufferHandle& operator=(const UniformBufferHandle&) = delete;

    UniformBufferHandle(UniformBufferHandle&& o) noexcept
        : buffer_(std::exchange(o.buffer_, VK_NULL_HANDLE))
        , alloc_(std::exchange(o.alloc_, nullptr))
        , desc_set_(std::exchange(o.desc_set_, VK_NULL_HANDLE))
        , desc_set_layout_(std::exchange(o.desc_set_layout_, VK_NULL_HANDLE))
        , desc_pool_(std::exchange(o.desc_pool_, VK_NULL_HANDLE))
        , data_(std::exchange(o.data_, nullptr))
        , device_(std::exchange(o.device_, VK_NULL_HANDLE))
        , allocator_(std::exchange(o.allocator_, nullptr)) {}

    UniformBufferHandle& operator=(UniformBufferHandle&& o) noexcept {
        if (this != &o) {
            destroy();
            buffer_ = std::exchange(o.buffer_, VK_NULL_HANDLE);
            alloc_ = std::exchange(o.alloc_, nullptr);
            desc_set_ = std::exchange(o.desc_set_, VK_NULL_HANDLE);
            desc_set_layout_ = std::exchange(o.desc_set_layout_, VK_NULL_HANDLE);
            desc_pool_ = std::exchange(o.desc_pool_, VK_NULL_HANDLE);
            data_ = std::exchange(o.data_, nullptr);
            device_ = std::exchange(o.device_, VK_NULL_HANDLE);
            allocator_ = std::exchange(o.allocator_, nullptr);
        }
        return *this;
    }

	static std::expected<UniformBufferHandle<T>, std::string> createBuffer(VkDevice device, VmaAllocator allocator) {
		UniformBufferHandle h;
        h.device_ = device;
        h.allocator_ = allocator;

        const VkBufferCreateInfo buf_info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = (sizeof(T) + 0xf) & ~0xf,
            .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE
        };
        const VmaAllocationCreateInfo alloc_create_info = {
            .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT
                   | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
            .usage = VMA_MEMORY_USAGE_AUTO
        };

        VmaAllocationInfo alloc_info{};
        if (vmaCreateBuffer(allocator, &buf_info, &alloc_create_info,
                            &h.buffer_, &h.alloc_, &alloc_info) != VK_SUCCESS) {
            return std::unexpected("Couldn't create uniform buffer");
        }
        h.data_ = reinterpret_cast<T*>(alloc_info.pMappedData);

        const VkDescriptorSetLayoutBinding binding = {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT
        };
        const VkDescriptorSetLayoutCreateInfo layout_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = 1,
            .pBindings = &binding
        };
        if (vkCreateDescriptorSetLayout(device, &layout_info, nullptr,
                                        &h.desc_set_layout_) != VK_SUCCESS) {
            h.destroy();
            return std::unexpected("Couldn't create descriptor set layout");
        }

        const VkDescriptorPoolSize pool_size = {
            .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1
        };
        const VkDescriptorPoolCreateInfo pool_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .maxSets = 1,
            .poolSizeCount = 1,
            .pPoolSizes = &pool_size
        };
        if (vkCreateDescriptorPool(device, &pool_info, nullptr,
                                   &h.desc_pool_) != VK_SUCCESS) {
            h.destroy();
            return std::unexpected("Couldn't create descriptor pool");
        }

        const VkDescriptorSetAllocateInfo set_alloc = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = h.desc_pool_,
            .descriptorSetCount = 1,
            .pSetLayouts = &h.desc_set_layout_
        };
        if (vkAllocateDescriptorSets(device, &set_alloc, &h.desc_set_) != VK_SUCCESS) {
            h.destroy();
            return std::unexpected("Couldn't allocate descriptor set");
        }

        const VkDescriptorBufferInfo buf_desc = {
            .buffer = h.buffer_,
            .offset = 0,
            .range = (sizeof(T) + 0xf) & ~0xf
        };
        const VkWriteDescriptorSet write_desc = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = h.desc_set_,
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pBufferInfo = &buf_desc
        };
        vkUpdateDescriptorSets(device, 1, &write_desc, 0, nullptr);

        return h;
	}
	VkBuffer buffer() const noexcept {
		return buffer_;
	}

	VmaAllocation alloc() const noexcept {
		return alloc_;
	}

	T* data() noexcept { 
		return data_; 
	}
    const T* data() const noexcept { 
		return data_; 
	}
    VkDescriptorSet descriptorSet() const noexcept { 
		return desc_set_; 
	}

	VkDescriptorSetLayout descriptorSetLayout() const noexcept { 
		return desc_set_layout_; 
	}
	

	void destroy() {
		if (desc_pool_ != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(device_, desc_pool_, nullptr);
            desc_pool_ = VK_NULL_HANDLE;
            desc_set_ = VK_NULL_HANDLE;
        }
        if (desc_set_layout_ != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(device_, desc_set_layout_, nullptr);
            desc_set_layout_ = VK_NULL_HANDLE;
        }
        if (buffer_ != VK_NULL_HANDLE) {
            vmaDestroyBuffer(allocator_, buffer_, alloc_);
            buffer_ = VK_NULL_HANDLE;
            alloc_ = nullptr;
        }
        data_ = nullptr;
	}
private:
	VkBuffer buffer_ = VK_NULL_HANDLE;
	VmaAllocation alloc_ = VK_NULL_HANDLE;
	VkDescriptorSet desc_set_ = VK_NULL_HANDLE;
	VkDescriptorSetLayout desc_set_layout_ = VK_NULL_HANDLE;
	VkDescriptorPool desc_pool_ = VK_NULL_HANDLE;
	T* data_ = nullptr;
	VmaAllocator allocator_ = VK_NULL_HANDLE;
	VkDevice device_ = VK_NULL_HANDLE;
};

class UniformBufferStorage {
public:
	template<typename T>
	static void registerBuffer(UniformBufferHandle<T>& buffer) {
		buffers_.push_back(std::make_unique<TypeErasureDerived<T>>(buffer));	
	}

	static void destroy() {
		for(std::unique_ptr<TypeErasureBase>& ptr: buffers_) {
			ptr->destroy();
		}
		buffers_.clear();
	}
	
	template<typename T>
	UniformBufferHandle<T>& at(std::size_t i) {
		auto* derived = dynamic_cast<TypeErasureDerived<T>*>(buffers_[i].get());
		if (!derived) {
			throw std::runtime_error("UniformBufferStorage: wrong type");
		}

		return derived->handle();
	}

	static std::vector<VkDescriptorSetLayout> allLayouts() {
		std::vector<VkDescriptorSetLayout> result;
		result.reserve(buffers_.size());
		for (auto& p : buffers_) result.push_back(p->descSetLayout());
		return result;
	}

private:
	class TypeErasureBase {
	public:
		virtual void destroy() = 0;
		virtual VkDescriptorSetLayout descSetLayout() const noexcept = 0;

		virtual ~TypeErasureBase() = default;
	};

	template<typename T>
	class TypeErasureDerived: public TypeErasureBase {
	public:
		TypeErasureDerived(UniformBufferHandle<T>& handle): handle_(handle) {}
		virtual void destroy() final {
			handle_.destroy();
		}
		VkDescriptorSetLayout descSetLayout() const noexcept final {
			return handle_.descriptorSetLayout();
		}
		UniformBufferHandle<T>& handle() {
			return handle_;
		}
	private:
		UniformBufferHandle<T>& handle_;
	};

	static std::vector<std::unique_ptr<TypeErasureBase>> buffers_;

};

extern Context context;

void updateGlobalUniformBuffers();

void drawAllObjects(const FrameData& fd);

bool initialize(GLFWwindow* const window);
void shutdown();

void resize(uint32_t width, uint32_t height);

FrameData prepare();
void submitAndPresent();

void registerPyramid(const Pyramid& pir) noexcept; 

} // namespace graphics::internal