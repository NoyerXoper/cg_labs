#pragma once

#include <glm/glm.hpp>

#include <cstdint>
#include <array>
#include <vector>

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
	glm::mat4 matrix;
	glm::vec3 color_multiply;
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

	glm::mat4 getMatrixTransform() const;
private:
	glm::mat4 getRotationMatrix() const;
	glm::mat4 getPositionMatrix() const;
	glm::mat4 getDeformationMatrix() const;

	glm::mat4 getModel() const;

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

	std::vector<VkDescriptorSet> uniform_descriptor_sets;
	// END TRIAG
};

struct FrameData {
	VkFramebuffer framebuffer;
	VkCommandBuffer command_buffer;
};

extern Context context;

void updateGlobalUniformBuffers();

bool initialize(GLFWwindow* const window);
void shutdown();

void resize(uint32_t width, uint32_t height);

FrameData prepare();
void submitAndPresent();

void registerPyramid(const Pyramid& pir) noexcept; 

} // namespace graphics::internal