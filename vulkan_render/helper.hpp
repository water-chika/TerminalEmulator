#pragma once

#include <string>
#include <vector>

#include <GLFW/glfw3.h>

static auto get_required_instance_extensions() {
  uint32_t count;
  const char** extensions = glfwGetRequiredInstanceExtensions(&count);
  std::vector<std::string> res(count);
  for (int i = 0; i < count; i++) {
      res[i] = extensions[i];
  }
  return res;
}
