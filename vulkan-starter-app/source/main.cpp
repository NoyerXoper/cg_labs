#include <cstdint>
#include <climits>

#include <iostream>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>

#include "graphics_internal.hpp"
#include "application.hpp"

namespace {

constexpr int32_t default_window_width = 1280;
constexpr int32_t default_window_height = 720;

constexpr char default_window_title[] = "Vulkan Starter App";

GLFWwindow* glfw_window;

} // namespace

int main() {
	int status = EXIT_SUCCESS;

	graphics::internal::registerPyramid(graphics::internal::Pyramid{
		std::array<glm::vec3, 4>{
			glm::vec3 {0.0f,  18.0f, 60.0f},
			glm::vec3 {-15.0f,  -6.0f, 51.0f},
			glm::vec3 {15.0f,  -6.0f, 51.0f},
			glm::vec3 {0.0f,  -6.0f, 77.0f},
		}, {{0.25, 0.25, 0.25}, 64.0}
	});

	graphics::internal::registerPyramid(graphics::internal::Pyramid{
		std::array<glm::vec3, 4>{
			glm::vec3{0.0f,  18.0f / 5, -60.0f / 5},
			glm::vec3 {-15.0f / 5,  -6.0f / 5, -51.0f / 5},
			glm::vec3 {15.0f / 5,  -6.0f / 5, -51.0f / 5},
			glm::vec3 {0.0f,  -6.0f / 5, -77.0f / 5},
		}, {{0.1, 0.1, 0.1}, 256.0}
	});

	if (!glfwInit()) {
		std::cerr << "Failed to initialize GLFW\n";
		return EXIT_FAILURE;
	}

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	bool is_first_frame = true;

	glfw_window = glfwCreateWindow(default_window_width, default_window_height,
	                               default_window_title, nullptr, nullptr);
	if (glfw_window == nullptr) {
		status = EXIT_FAILURE;
		goto err_null_window;
	}

	glfwSetFramebufferSizeCallback(glfw_window, [](GLFWwindow*, int width, int height){
		if (width == 0 || height == 0) {
			return;
		}

		graphics::internal::resize(width, height);
	});

	if (ImGui::CreateContext() == nullptr) {
		std::cerr << "Failed to create ImGUI context\n";
		status = EXIT_FAILURE;
		goto err_imgui_init;
	}

	if (!ImGui_ImplGlfw_InitForVulkan(glfw_window, true)) {
		std::cerr << "Failed to initialize ImGUI GLFW backend for Vulkan renderer\n";
		status = EXIT_FAILURE;
		goto err_imgui_glfw_init;
	}

	if (!graphics::internal::initialize(glfw_window)) {
		std::cerr << "Failed to initialize graphics\n";
		status = EXIT_FAILURE;
		goto err_graphics_init;
	}

	if (!application::initialize()) {
		std::cerr << "Failed to initialize application\n";
		status = EXIT_FAILURE;
		goto err_application_init;
	}


	double prev_mx;
	double prev_my;

	while (!glfwWindowShouldClose(glfw_window)) {
		const double time = glfwGetTime();

		glfwPollEvents();
		ImGui_ImplGlfw_NewFrame();

		double mx = 0;
		double my = 0;
		glfwGetCursorPos(glfw_window, &mx, &my);
		if (is_first_frame) {
			prev_mx = mx;
			prev_my = my;
			is_first_frame = false;
		}
		double mdx = 0;
		double mdy = 0;
		if (glfwGetMouseButton(glfw_window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
			mdx = mx - prev_mx;
			mdy = my - prev_my;
		}
		prev_mx = mx;
		prev_my = my;

		glm::vec3 dir(0);

		if (glfwGetKey(glfw_window, GLFW_KEY_W) == GLFW_PRESS) dir.z += 1.0f;
		if (glfwGetKey(glfw_window, GLFW_KEY_S) == GLFW_PRESS) dir.z -= 1.0f;
		if (glfwGetKey(glfw_window, GLFW_KEY_D) == GLFW_PRESS) dir.x += 1.0f;
		if (glfwGetKey(glfw_window, GLFW_KEY_A) == GLFW_PRESS) dir.x -= 1.0f;
		if (glfwGetKey(glfw_window, GLFW_KEY_SPACE)      == GLFW_PRESS) dir.y += 1.0f;
		if (glfwGetKey(glfw_window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) dir.y -= 1.0f;

		ImGui::NewFrame();
		application::update(time, mdx, mdy, dir);
		ImGui::Render();

		graphics::internal::FrameData fd = graphics::internal::prepare();
		application::render(fd, time);
		graphics::internal::submitAndPresent();
	}

	application::shutdown();
err_application_init:
	graphics::internal::shutdown();
err_graphics_init:
	ImGui_ImplGlfw_Shutdown();
err_imgui_glfw_init:
	ImGui::DestroyContext();
err_imgui_init:
	glfwDestroyWindow(glfw_window);
err_null_window:
	glfwTerminate();

	return 0;
}