#ifndef SETUP_HPP
#define SETUP_HPP

#include <imgui/imgui.h>
#include <imgui/backends/imgui_impl_glfw.h>
#include <imgui/backends/imgui_impl_vulkan.h>
#include <Primrose/scene.hpp>

extern Primrose::SceneNode* doubleClickedNode;
extern std::filesystem::path lastLoadedScene;

void guiKeyCb(int key, int mods, bool pressed);

void setupGui();
void updateGui(Primrose::Scene& scene);
void renderGui(VkCommandBuffer& cmd);
void cleanupGui();

#endif
