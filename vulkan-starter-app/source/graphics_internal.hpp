#pragma once

#include <expected>
#include <string>
#include <memory>
#include <algorithm>

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

struct PointLight {
    alignas(16) glm::vec3 position;
    alignas(16) glm::vec3 color;
    float intensity;
};

struct ProjectorLight {
    alignas(16) glm::vec3 position;
    alignas(16) glm::vec3 direction;
    alignas(16) glm::vec3 color;
    float intensity;
    float angle;
};

struct Material {
    float ambient;
    float specular;
    float shiny;
};

struct Vertex {
	glm::vec3 position;
	glm::vec3 color;
	glm::vec3 norm;
};

struct FrameUniform {
	glm::mat4 view;
	glm::mat4 perspective;
	alignas(16) glm::vec3 camera_pos;
	alignas(16) glm::vec3 direction_light;
	alignas(16) glm::vec3 color_light;
};

struct ObjectUnifrom {
	glm::mat4 model;
	alignas(16) glm::vec3 color_multiply;
    alignas(16) Material material;
};

class Camera {
public:
	static bool is_persp;
	Camera(float speed, float sensitivity) noexcept;

	glm::mat4 getView() const noexcept;	

	void updateAngles(float dx, float dy) noexcept;
	void updatePos(glm::vec3 move, float dt) noexcept;
	const glm::vec3& getPos() const noexcept;

	glm::vec3 forward() const noexcept;
	glm::vec3 up() const noexcept;
	glm::vec3 right() const noexcept;
	glm::mat4 getOrthoOrPerspective() const;

	glm::mat4 getOrtho() const;
	glm::mat4 getPerspective() const;
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
	Pyramid(const std::array<glm::vec3, 4>& vertcies, const Material& material); 

	const std::array<Vertex, 12>& getVertex() const noexcept;
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

    Material& getMaterial() noexcept;

private:
	glm::mat4 getRotationMatrix() const;
	glm::mat4 getPositionMatrix() const;
	glm::mat4 getDeformationMatrix() const;

	glm::vec3 center_;
	glm::vec3 angles_{0, 0, 0};
	glm::vec3 deformation_{1, 1, 1};
	glm::vec3 color_multiply_{0.5, 0.5, 0.5};
    Material material_;
	std::array<Vertex, 12> verticies_;
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

	Camera cam{7.0f, 0.004};

	glm::vec3 direction_light{-0.3f, -1.0f, -0.5f};
	glm::vec3 color_light{1.0f, 1.0f, 1.0f};
	// END TRIAG
};

struct FrameData {
	VkFramebuffer framebuffer;
	VkCommandBuffer command_buffer;
};

enum class BufferType: std::size_t {
	Uniform = 0,
	Storage = 1
};

namespace {
inline constexpr std::array descriptor_type_for{
    VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
    VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
};

inline constexpr std::array buffer_usage_for{
    VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
};

}

template <BufferType type>
class BufferStorage {
	static constexpr std::size_t IDX = static_cast<std::size_t>(type);

    static constexpr VkDescriptorType desc_type =
        descriptor_type_for[IDX];

    static constexpr VkBufferUsageFlags usage =
        buffer_usage_for[IDX]
        | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

public:
    BufferStorage() = default;
    BufferStorage(const BufferStorage&) = delete;
    BufferStorage& operator=(const BufferStorage&) = delete;

    BufferStorage(BufferStorage&& o) noexcept { 
		moveFrom(std::move(o)); 
	}
    BufferStorage& operator=(BufferStorage&& o) noexcept {
        if (this != &o) { 
			destroy(); 
			moveFrom(std::move(o)); 
		}
        return *this;
    }
    ~BufferStorage() { 
		destroy(); 
	}

    static std::expected<VkDescriptorSetLayout, std::string>
    createLayout(VkDevice device) {
        VkDescriptorSetLayoutBinding b{
            .binding = 0,
            .descriptorType = desc_type,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        };
        VkDescriptorSetLayoutCreateInfo info{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = 1,
            .pBindings = &b,
        };
        VkDescriptorSetLayout layout = VK_NULL_HANDLE;
        if (vkCreateDescriptorSetLayout(device, &info, nullptr, &layout) != VK_SUCCESS) {
            return std::unexpected("Couldn't create desc set layout");
        }
        return layout;
    }

    static std::expected<BufferStorage, std::string>
    create(VkDevice device, VkDescriptorPool pool,
           VkDescriptorSetLayout layout, VmaAllocator allocator,
           VkDeviceSize size)
    {
        size = (size + 0xf) & ~0xf;

        BufferStorage h;
        h.device_ = device;
        h.allocator_ = allocator;
        h.desc_pool_ = pool;
        h.desc_set_layout_ = layout;
        h.size_ = size;

        const VkBufferCreateInfo buf_info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = size,
            .usage = usage,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        };
        const VmaAllocationCreateInfo alloc_info = {
            .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT
        			| VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
            .usage = VMA_MEMORY_USAGE_AUTO,
        };

        VmaAllocationInfo vma_info{};
        if (vmaCreateBuffer(allocator, &buf_info, &alloc_info, &h.buffer_, &h.alloc_, &vma_info) != VK_SUCCESS) {
            return std::unexpected("Couldn't create buffer");
        }
        h.data_ = reinterpret_cast<std::byte*>(vma_info.pMappedData);

        const VkDescriptorSetAllocateInfo set_alloc = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = h.desc_pool_,
            .descriptorSetCount = 1,
            .pSetLayouts = &h.desc_set_layout_,
        };
        if (vkAllocateDescriptorSets(device, &set_alloc, &h.desc_set_) != VK_SUCCESS) {
            h.destroy();
            return std::unexpected("Couldn't allocate descriptor set");
        }

        const VkDescriptorBufferInfo buf_desc = {
            .buffer = h.buffer_,
            .offset = 0,
            .range  = size,
        };
        const VkWriteDescriptorSet write = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet  = h.desc_set_,
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = desc_type,
            .pBufferInfo = &buf_desc,
        };
        vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);

        return h;
    }

    VkBuffer buffer() const noexcept { 
		return buffer_; 
	}

    VmaAllocation alloc() const noexcept { 
		return alloc_; 
	}

    VkDeviceSize size() const noexcept { 
		return size_; 
	}

    VkDescriptorSet descriptorSet() const noexcept { 
		return desc_set_; 
	}

    VkDescriptorSetLayout descriptorSetLayout() const noexcept { 
		return desc_set_layout_; 
	}

    bool isMapped() const noexcept { 
		return data_ != nullptr; 
	}

    std::byte* raw() noexcept { 
        return data_; 
    }
    const std::byte* raw() const noexcept { 
        return data_; 
    }

    void flush(VkDeviceSize offset = 0, VkDeviceSize sz = VK_WHOLE_SIZE) {
        if (alloc_) {
			vmaFlushAllocation(allocator_, alloc_, offset, sz);
		}
    }

    void destroy() noexcept {
        if (buffer_ != VK_NULL_HANDLE) {
            vmaDestroyBuffer(allocator_, buffer_, alloc_);
        }
        buffer_ = VK_NULL_HANDLE;
        alloc_ = nullptr;
        data_ = nullptr;
        desc_set_ = VK_NULL_HANDLE;
        desc_set_layout_ = VK_NULL_HANDLE;
        desc_pool_ = VK_NULL_HANDLE;
        device_ = VK_NULL_HANDLE;
        allocator_ = nullptr;
        size_ = 0;
    }

private:
    void moveFrom(BufferStorage&& o) noexcept {
        buffer_ = std::exchange(o.buffer_, VK_NULL_HANDLE);
        alloc_ = std::exchange(o.alloc_, nullptr);
        desc_set_ = std::exchange(o.desc_set_, VK_NULL_HANDLE);
        desc_set_layout_ = std::exchange(o.desc_set_layout_, VK_NULL_HANDLE);
        desc_pool_ = std::exchange(o.desc_pool_, VK_NULL_HANDLE);
        data_ = std::exchange(o.data_, nullptr);
        device_ = std::exchange(o.device_, VK_NULL_HANDLE);
        allocator_ = std::exchange(o.allocator_, nullptr);
        size_= std::exchange(o.size_, 0);
    }

    VkBuffer buffer_ = VK_NULL_HANDLE;
    VmaAllocation alloc_ = nullptr;
    VkDescriptorSet desc_set_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout desc_set_layout_ = VK_NULL_HANDLE;
    VkDescriptorPool desc_pool_ = VK_NULL_HANDLE;
    std::byte* data_ = nullptr;
    VmaAllocator allocator_ = nullptr;
    VkDevice device_ = VK_NULL_HANDLE;
    VkDeviceSize size_ = 0;
};

template<typename T, BufferType type = BufferType::Uniform>
class BufferHandle : BufferStorage<type>{
using Base = BufferStorage<type>;
public:
	using Base::createLayout;
    using Base::buffer;
    using Base::alloc;
    using Base::size;
    using Base::raw;
    using Base::descriptorSet;
    using Base::descriptorSetLayout;
    using Base::isMapped;
    using Base::flush;
    using Base::destroy;

	BufferHandle() = default;
    BufferHandle(BufferHandle&&) noexcept = default;
    BufferHandle& operator=(BufferHandle&&) noexcept = default;
    BufferHandle(const BufferHandle&) = delete;
    BufferHandle& operator=(const BufferHandle&) = delete;
    ~BufferHandle() = default;

	static std::expected<BufferHandle, std::string>
    createBuffer(VkDevice device, VkDescriptorPool pool,
                 VkDescriptorSetLayout layout, VmaAllocator allocator)
    {
        auto base = Base::create(device, pool, layout, allocator, sizeof(T));
        if (!base) return std::unexpected(base.error());

        BufferHandle h;
        static_cast<Base&>(h) = std::move(*base);
        return h;
    }

	T* data() noexcept { 
		return reinterpret_cast<T*>(this->raw()); 
	}
    const T* data() const noexcept { 
		return reinterpret_cast<const T*>(this->raw()); 
	}

    T& operator*() noexcept { 
		return *data(); 
	}
    const T& operator*()  const noexcept { 
		return *data(); 
	}
    T* operator->() noexcept { 
		return data(); 
	}
    const T* operator->() const noexcept { 
		return data(); 
	}

private:

};

template <typename T, BufferType type = BufferType::Storage>
class ArrayBuffer : BufferStorage<type>{
using Base = BufferStorage<type>;
public:
    using Base::createLayout;
    using Base::buffer;
    using Base::alloc;
    using Base::size;
    using Base::raw;
    using Base::descriptorSet;
    using Base::descriptorSetLayout;
    using Base::isMapped;
    using Base::flush;
    using Base::destroy;

	static constexpr uint32_t header_size = alignof(T);

	ArrayBuffer() = default;
    ArrayBuffer(ArrayBuffer&&) noexcept= default;
    ArrayBuffer& operator=(ArrayBuffer&&) noexcept = default;
    ArrayBuffer(const ArrayBuffer&) = delete;
    ArrayBuffer& operator=(const ArrayBuffer&) = delete;
    ~ArrayBuffer() = default;

	static std::expected<ArrayBuffer, std::string>
    createBuffer(
        VkDevice device, 
        VkDescriptorPool pool, 
        VkDescriptorSetLayout layout, 
        VmaAllocator allocator, 
        uint32_t count
    ) {
        auto base = Base::create(device, pool, layout, allocator, VkDeviceSize(sizeof(T)) * count + header_size);
        if (!base) return std::unexpected(base.error());

        ArrayBuffer h;
        h.capacity_ = count;
        static_cast<Base&>(h) = std::move(*base);
        h.getSize() = 0;
        return h;
    }

	T* data() noexcept { 
		return reinterpret_cast<T*>(this->raw() + header_size); 
	}
    const T* data() const noexcept { 
		return reinterpret_cast<const T*>(this->raw() + header_size); 
	}

    T& operator[](std::size_t i) noexcept { 
		return data()[i]; 
	}
    const T& operator[](std::size_t i) const noexcept { 
		return data()[i]; 
	}

    uint32_t count() const noexcept { 
		return getSize(); 
	}

    T* begin() noexcept { 
		return data(); 
	}
    T* end() noexcept { 
		return data() + getSize(); 
	}
    const T* begin() const noexcept { 
		return data(); 
	}
    const T* end() const noexcept { 
		return data() + getSize(); 
	}

    bool pushBack(const T& val) {
        if (getSize() >= capacity_) {
            return false;
        }
        std::construct_at(end(), val);
        ++getSize();
        flush();
        return true;
    }

    bool popBack() {
        if (getSize() == 0) {
            return false;
        }
        std::destroy_at(end() - 1);
        --getSize();
        flush();
        return true;
    }

    bool popAt(std::size_t i) {
        if (getSize() == 0) {
            return false;
        }
        getSize()--;

        std::shift_left(begin() + i , end(), 1);
        return true;
    }

private:
    uint32_t capacity_;
    uint32_t& getSize() noexcept {
        return *reinterpret_cast<uint32_t*>(this->raw());
    }

    uint32_t getSize() const noexcept {
        return *reinterpret_cast<const uint32_t*>(this->raw());
    }

};

class BufferRegistry {
public:
	template<typename T, BufferType type>
	static void registerBuffer(BufferHandle<T, type>& buffer) {
		buffers_.push_back(std::make_unique<BufferHandleErasure<T, type>>(buffer));	
	}

	template<typename T, BufferType type>
	static void registerBuffer(ArrayBuffer<T, type>& buffer) {
		buffers_.push_back(std::make_unique<ArrayBufferErasure<T, type>>(buffer));	
	}

	static void destroy() {
		for(std::unique_ptr<TypeErasureBase>& ptr: buffers_) {
			ptr->destroy();
		}
		buffers_.clear();
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

	template<typename T, BufferType type>
	class BufferHandleErasure: public TypeErasureBase {
	public:
		BufferHandleErasure(BufferHandle<T, type>& handle): handle_(handle) {}
		virtual void destroy() final {
			handle_.destroy();
		}
		VkDescriptorSetLayout descSetLayout() const noexcept final {
			return handle_.descriptorSetLayout();
		}
		BufferHandle<T>& handle() {
			return handle_;
		}
	private:
		BufferHandle<T, type>& handle_;
	};

	template<typename T, BufferType type>
	class ArrayBufferErasure: public TypeErasureBase {
	public:
		ArrayBufferErasure(ArrayBuffer<T, type>& handle): handle_(handle) {}
		virtual void destroy() final {
			handle_.destroy();
		}
		VkDescriptorSetLayout descSetLayout() const noexcept final {
			return handle_.descriptorSetLayout();
		}
		ArrayBuffer<T>& handle() {
			return handle_;
		}
	private:
		ArrayBuffer<T, type>& handle_;
	};

	static std::vector<std::unique_ptr<TypeErasureBase>> buffers_;

};


extern Context context;

void updateGlobalUniformBuffers();

void drawAllObjects(const FrameData& fd);

bool initialize(GLFWwindow* const window);
void shutdown();

void resize(uint32_t width, uint32_t height);

ArrayBuffer<PointLight>& getPointBuffer();
ArrayBuffer<ProjectorLight>& getProjBuffer();

FrameData prepare();
void submitAndPresent();

void registerPyramid(const Pyramid& pir) noexcept; 

} // namespace graphics::internal