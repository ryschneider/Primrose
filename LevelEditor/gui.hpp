#ifndef SETUP_HPP
#define SETUP_HPP

#include <imgui/imgui.h>
#include <imgui/backends/imgui_impl_glfw.h>
#include <imgui/backends/imgui_impl_vulkan.h>
#include <Primrose/scene/scene.hpp>

extern Primrose::Node* doubleClickedNode;
extern std::filesystem::path lastLoadedScene;

void guiKeyCb(int key, int action, int mods);

void setupGui();
bool updateGui(Primrose::Scene& scene, float dt);
void renderGui(VkCommandBuffer& cmd);
void cleanupGui();

#endif
