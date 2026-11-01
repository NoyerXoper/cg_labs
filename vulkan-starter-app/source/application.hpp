#pragma once

#include "graphics_internal.hpp"

namespace application {

bool initialize();
void shutdown();

void update(double time, double mdx, double mdy, glm::vec3 move_dir);
void render(const graphics::internal::FrameData& fd, double time);

} // namespace application