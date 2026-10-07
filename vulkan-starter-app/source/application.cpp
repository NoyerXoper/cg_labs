#include "application.hpp"

#include <cmath>
#include <iostream>
#include <numbers>

#include <imgui.h>

namespace application {

bool initialize() {
	return true;
}

void shutdown() {
	auto& context = graphics::internal::context;
	vkQueueWaitIdle(context.graphics_queue);
}

void update([[maybe_unused]] double time) {
	static constexpr auto normalise = [](float angle) {
		angle = std::fmod(angle, 2 * std::numbers::pi);
		if (angle < 0) {
			angle += 2 * std::numbers::pi;
		}
		return angle;
	};
	static constexpr float DEFAULT_SPEED = 0;
	static constexpr float DEFAULT_RADIUS = 3;
	static constexpr glm::vec3 DEFAULT_ANGLE_SPEED{};
	static float prev = 0;
	static auto& objects = graphics::internal::VertexIndexStorage::objectsData();
	static std::vector<float> prev_phase (objects.size(), 0);
	// ImGui::ShowDemoWindow();
	static bool is_open_pos = false;
	static bool is_open_color = false;
	static bool is_select_open = false;
	static std::vector<char> is_playing_anim(objects.size(), 0);
	static std::size_t selected = 0;
	if (objects.size() == 0) {
		return;
	}

	static std::vector<float> speeds(objects.size(), DEFAULT_SPEED);
	static std::vector<float> radiuses (objects.size(), DEFAULT_RADIUS);
	static std::vector<glm::vec3> angle_speeds (objects.size(), DEFAULT_ANGLE_SPEED);

	if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("Windows")) {
            ImGui::MenuItem("Select figure", nullptr, &is_select_open);
            ImGui::MenuItem("Position & animation", nullptr, &is_open_pos);
            ImGui::MenuItem("Color",                nullptr, &is_open_color);
            ImGui::MenuItem("Is perscpective",                nullptr, &graphics::internal::is_perspective);
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

	if(is_select_open) {
        std::string preview = "Object " + std::to_string(selected);
		ImGui::Begin("Selection of figure");
		if (ImGui::BeginCombo("##object", preview.c_str())) {
			for (int i = 0; i < objects.size(); ++i) {
				const bool is_selected = (selected == i);
				std::string label = "Object " + std::to_string(i);
				if (ImGui::Selectable(label.c_str(), is_selected)) {
					selected = i;
				}
				if (is_selected) {
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}
		ImGui::End();
	}

	if (is_open_pos) {
		ImGui::Begin("Position", &is_open_pos);
		ImGui::Text("XYZ Position");
		glm::vec3 pos = objects[selected]->getPos();
		bool did_pos_change = false;
		ImGui::BeginDisabled(is_playing_anim[selected]);
		did_pos_change |= ImGui::SliderFloat("<-x", &pos.x, -10, 10);
		did_pos_change |= ImGui::SliderFloat("<-y", &pos.y, -10, 10);
		did_pos_change |= ImGui::SliderFloat("<-z", &pos.z, -100, 0);
		if (did_pos_change) {
			objects[selected]->setPos(pos);
		}

		ImGui::Separator();

		ImGui::Text("Euler Angles");
		glm::vec3 angle = objects[selected]->getAngles();
		bool did_angle_change = false;
		did_angle_change |= ImGui::SliderFloat("<-yz", &angle.x, 0, 360);
		did_angle_change |= ImGui::SliderFloat("<-xz", &angle.y, 0, 360);
		did_angle_change |= ImGui::SliderFloat("<-xy", &angle.z, 0, 360);
		if (did_angle_change) {
			objects[selected]->setAngles(angle);
		}
		ImGui::EndDisabled();
		ImGui::Separator();
		ImGui::Text("Deformation");
		glm::vec3 deformation = objects[selected]->getDeformation();
		bool did_deformation_change = false;
		did_deformation_change |= ImGui::SliderFloat("<-x##deformation", &deformation.x, 0.001, 5);
		did_deformation_change |= ImGui::SliderFloat("<-y##deformation", &deformation.y, 0.001, 5);
		did_deformation_change |= ImGui::SliderFloat("<-z##deformation", &deformation.z, 0.001, 5);
		if (did_deformation_change) {
			objects[selected]->setDeformation(deformation);
		}
		ImGui::Separator();

		ImGui::Text("Animation");
		if(ImGui::Button("Reset animation")) {
			prev_phase[selected] = 0;
			speeds[selected] = DEFAULT_SPEED;
			radiuses[selected] = DEFAULT_RADIUS;
			angle_speeds[selected] = DEFAULT_ANGLE_SPEED;
		}
		bool v = is_playing_anim[selected] != 0;
		if(ImGui::Checkbox("Enable Animation", &v)) {
			is_playing_anim[selected] = v;
		}

		ImGui::SliderFloat("Radius", &radiuses[selected], 0.1, 10);
		ImGui::SliderFloat("Speed", &speeds[selected], -10, 10);
		ImGui::Separator();
		ImGui::SliderFloat("yz angle speed", &angle_speeds[selected].x, -360, 360);
		ImGui::SliderFloat("xz angle speed", &angle_speeds[selected].y, -360, 360);
		ImGui::SliderFloat("xy angle speed", &angle_speeds[selected].z, -360, 360);

		ImGui::End();
	}
	if (is_open_color) {
		ImGui::Begin("Color", &is_open_color);
			glm::vec3 c = objects[selected]->getColorMultiplier();
			if (ImGui::ColorEdit3("multiplier", &c.x)) {
				objects[selected]->setColorMultipiles(c);
			}

			ImGui::Text("vertex color: (%.2f, %.2f, %.2f)", c.x, c.y, c.z);
		ImGui::End();
	}
	for(std::size_t i = 0; i < objects.size(); ++i) {
		if (is_playing_anim[i]) { 
			float dt = time - prev;
			float current_phase = normalise(prev_phase[i] + speeds[i] / radiuses[i] * dt);

			glm::vec3 dphi = angle_speeds[i] * dt;
			objects[i]->setAngles(objects[i]->getAngles() + dphi);
			if(radiuses[i] > 1e-10) {
				glm::vec3 dr = speeds[i] * glm::vec3(std::cos(current_phase), std::sin(current_phase), 0) * dt;
				objects[i]->setPos(objects[i]->getPos() + dr);
			}
			prev_phase[i] = current_phase;
		}
	}
	prev = time;

	graphics::internal::updateGlobalUniformBuffers();
}

void render(const graphics::internal::FrameData& fd, double time) {
	static double prev_time = 0;
	if (fd.command_buffer == nullptr) {
		return;
	}

	double dt = time - prev_time;
	prev_time = time;


	vkResetCommandBuffer(fd.command_buffer, 0);
	
	VkCommandBufferBeginInfo buffer_begin_info {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
	};

	vkBeginCommandBuffer(fd.command_buffer, &buffer_begin_info);

	VkClearValue clear_values[] = {
		{
			.color = {{0.0f, 0.0f, 0.1f, 1.0f}}
		},
		{
			.depthStencil = {1.0f, 0}
		}
	};

	VkRenderPassBeginInfo render_pass_info {
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.renderPass = graphics::internal::context.render_pass,
		.framebuffer = fd.framebuffer,
		.renderArea = {
			.offset = {0, 0},
			.extent = graphics::internal::context.swapchain_extent
		},
		.clearValueCount = 2,
		.pClearValues = clear_values
	};

	vkCmdBeginRenderPass(fd.command_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
	vkCmdBindPipeline(fd.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphics::internal::context.pipeline);

	VkDeviceSize offset = 0;
	vkCmdBindVertexBuffers(fd.command_buffer, 0, 1, &graphics::internal::context.vertex_buffer, &offset);
	vkCmdBindIndexBuffer(fd.command_buffer, graphics::internal::context.index_buffer, 0, VK_INDEX_TYPE_UINT32);

	VkViewport viewport = {
		.x = 0.0f,
		.y = 0.0f,
		.width = static_cast<float>(graphics::internal::context.swapchain_extent.width),
		.height = static_cast<float>(graphics::internal::context.swapchain_extent.height),
		.minDepth = 0.0f,
		.maxDepth = 1.0f,
	};
	vkCmdSetViewport(fd.command_buffer, 0, 1, &viewport);

	VkRect2D scissors {
		.offset = {0, 0},
		.extent = graphics::internal::context.swapchain_extent
	};
	vkCmdSetScissor(fd.command_buffer, 0, 1, &scissors);

	auto& scene_objects = graphics::internal::VertexIndexStorage::objectsData();

	uint32_t drawn_verticies = 0;

	for (std::size_t i = 0; i < scene_objects.size(); ++i) {
		vkCmdBindDescriptorSets(
			fd.command_buffer, 
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			graphics::internal::context.pipeline_layout,
			0,
			1,
			&graphics::internal::context.uniform_descriptor_sets[i],
			0,
			nullptr
		);

		vkCmdDrawIndexed(fd.command_buffer, scene_objects[i]->getIndex().size(), 1, drawn_verticies, 0, 0);

		drawn_verticies += scene_objects[i]->getIndex().size();
	}

	vkCmdEndRenderPass(fd.command_buffer);
	vkEndCommandBuffer(fd.command_buffer);

}

} // namespace application