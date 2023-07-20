#include "gui.hpp"

#include <Primrose/engine/setup.hpp>
#include <Primrose/scene.hpp>
#include <stdexcept>
#include <iostream>
#include <map>

using namespace Primrose;

VkDescriptorPool imguiDescriptorPool;

bool addObjectDialog = false;
void guiKeyCb(int key, int mods, bool pressed) {
	if (pressed) {
		if (mods & GLFW_MOD_SHIFT && key == GLFW_KEY_A) {
			addObjectDialog = true;
		}
	}
}

void createImguiDescriptorPool() {
	VkDescriptorPoolSize pool_sizes[] =
		{
			{ VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
			{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
			{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
		};

	VkDescriptorPoolCreateInfo pool_info = {};
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	pool_info.maxSets = 1000;
	pool_info.poolSizeCount = std::size(pool_sizes);
	pool_info.pPoolSizes = pool_sizes;

	if (vkCreateDescriptorPool(device, &pool_info, nullptr, &imguiDescriptorPool) != VK_SUCCESS) {
		throw std::runtime_error("failed to create imgui descriptor pool");
	}
}

void checkVk(VkResult res) {
	if (res != VK_SUCCESS) {
		throw std::runtime_error("IMGUI VULKAN ERROR");
	}
}

void setupGui() {
	// https://vkguide.dev/docs/extra-chapter/implementing_imgui/

	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	ImGui_ImplGlfw_InitForVulkan(window, true);

	ImGui::StyleColorsDark();

	createImguiDescriptorPool();

	ImGui_ImplVulkan_InitInfo info{};
	info.Instance = instance;
	info.PhysicalDevice = physicalDevice;
	info.Device = device;
	info.Queue = graphicsQueue;
	info.DescriptorPool = imguiDescriptorPool;
	info.MinImageCount = swapchainFrames.size();
	info.ImageCount = swapchainFrames.size();
	info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
	info.CheckVkResultFn = checkVk;

	ImGui_ImplVulkan_Init(&info, renderPass);

	// create fonts
	VkCommandBuffer cmd = startSingleTimeCommandBuffer();
	ImGui_ImplVulkan_CreateFontsTexture(cmd);
	endSingleTimeCommandBuffer(cmd);
	ImGui_ImplVulkan_DestroyFontUploadObjects();

	// key callback
	glfwSetCharCallback(window, ImGui_ImplGlfw_CharCallback);
//	glfwSetCharModsCallback(window, ImGui_ImplGlfw_CharCallback);

}

SceneNode* clickedNode = nullptr;
static void processNode(SceneNode* node) {
	int treeFlags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth
		| ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;

	if (node == clickedNode) treeFlags |= ImGuiTreeNodeFlags_Selected;

	switch (node->type()) {
		case NODE_SPHERE:
		case NODE_BOX:
		case NODE_TORUS:
		case NODE_LINE:
		case NODE_CYLINDER:
			treeFlags |= ImGuiTreeNodeFlags_Leaf;
			break;
		case NODE_UNION:
		case NODE_INTERSECTION:
			if (((MultiNode*)node)->objects.size() == 0) treeFlags |= ImGuiTreeNodeFlags_Leaf;
			break;
		case NODE_DIFFERENCE:
			break;
	}

	if (ImGui::TreeNodeEx(node, treeFlags, node->name.c_str())) {
		if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) clickedNode = node;

		if (node->type() == NODE_DIFFERENCE) {
			processNode(((DifferenceNode*)node)->base.get());
			processNode(((DifferenceNode*)node)->subtract.get());
		} else if (node->type() == NODE_UNION || node->type() == NODE_INTERSECTION) {
			for (const auto& child : ((MultiNode*)node)->objects) {
				processNode(child.get());
			}
		}

		ImGui::TreePop();
	}
}

void updateGui(Primrose::Scene& scene) {
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	ImGui::ShowDemoWindow();

	ImGui::Begin("Scene", nullptr, ImGuiWindowFlags_MenuBar);
	if (ImGui::BeginMenuBar()) {
		if (ImGui::BeginMenu("File")) {
			if (ImGui::MenuItem("Open...", "Ctrl+O")) {
				std::cout << "open scene" << std::endl;
			}
			if (ImGui::MenuItem("Save", "Ctrl+S")) {
				std::cout << "save scene" << std::endl;
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Object")) {
			if (ImGui::MenuItem("Add", "Shift+A")) {
				addObjectDialog = true;
			};
			ImGui::EndMenu();
		}
		ImGui::EndMenuBar();
	}

	ImGui::BeginChild("Scene", ImVec2(ImGui::GetContentRegionAvail().x, 260), true);
	for (const auto& node : scene.root) {
		processNode(node.get());
	}
	ImGui::EndChild();

	if (clickedNode != nullptr) {
		ImGui::BeginChild("Properties");

		ImGui::SeparatorText("SceneNode");
		ImGui::InputFloat3("Position", &(clickedNode->translate[0]));
		ImGui::InputFloat3("Scale", &(clickedNode->scale[0]));
		ImGui::InputFloat("Angle", &clickedNode->angle);
		ImGui::InputFloat3("Axis", &(clickedNode->axis[0]));
//		ImGui::Edit

		switch (clickedNode->type()) {
			case NODE_SPHERE:
			case NODE_BOX:
			case NODE_TORUS:
			case NODE_LINE:
			case NODE_CYLINDER:
				break;
			case NODE_UNION:
			case NODE_INTERSECTION:
			case NODE_DIFFERENCE:
				break;
		}
		ImGui::EndChild();
	}

	ImGui::End();

	if (addObjectDialog) {
		ImGui::OpenPopup("Add Object");
		addObjectDialog = false;
	}

	ImGui::SetNextWindowBgAlpha(1);
	if (ImGui::BeginPopup("Add Object")) {
		if (ImGui::Selectable("Sphere")) {
//			scene.root.push_back()
		} else if (ImGui::Selectable("Box")) {
			std::cout << "Box" << std::endl;
		} else if (ImGui::Selectable("Torus")) {
			std::cout << "Torus" << std::endl;
		} else if (ImGui::Selectable("Line")) {
			std::cout << "Line" << std::endl;
		} else if (ImGui::Selectable("Cylinder")) {
			std::cout << "Cylinder" << std::endl;
		}
		ImGui::EndPopup();
	}

	ImGui::Render();
}

void renderGui(VkCommandBuffer& cmd) {
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
}

void cleanupGui() {
	vkDestroyDescriptorPool(device, imguiDescriptorPool, nullptr);
	ImGui_ImplGlfw_Shutdown();
	ImGui_ImplVulkan_Shutdown();
}
