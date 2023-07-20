#include "gui.hpp"

#include <Primrose/engine/setup.hpp>
#include <Primrose/scene.hpp>
#include <stdexcept>
#include <iostream>
#include <map>
#include <imgui/imgui_internal.h>
#include <imgui/misc/cpp/imgui_stdlib.h>

using namespace Primrose;

VkDescriptorPool imguiDescriptorPool;

bool addObjectDialog = false;
bool newScene = false;
void guiKeyCb(int key, int mods, bool pressed) {
	if (pressed) {
		if (mods & GLFW_MOD_SHIFT && key == GLFW_KEY_A) {
			addObjectDialog = true;
		}

		if (mods & GLFW_MOD_CONTROL && key == GLFW_KEY_N) {
			newScene = true;
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
	ImGuiIO& io = ImGui::GetIO();

	ImVector<ImWchar> ranges;
	ImFontGlyphRangesBuilder builder;
	builder.AddText((const char*)u8"θ");
	builder.AddRanges(io.Fonts->GetGlyphRangesDefault());
	builder.BuildRanges(&ranges);

	ImFontConfig config;
	config.MergeMode = true;

	io.Fonts->AddFontFromFileTTF("resources/Crisp_plus.ttf", 24, nullptr, ranges.Data);
	io.Fonts->Build();

	VkCommandBuffer cmd = startSingleTimeCommandBuffer();
	ImGui_ImplVulkan_CreateFontsTexture(cmd);
	endSingleTimeCommandBuffer(cmd);
	ImGui_ImplVulkan_DestroyFontUploadObjects();

	// key callback
	glfwSetCharCallback(window, ImGui_ImplGlfw_CharCallback);
//	glfwSetCharModsCallback(window, ImGui_ImplGlfw_CharCallback);

	io.ConfigDragClickToInputText = true;
}

SceneNode* clickedNode = nullptr;
SceneNode* doubleClickedNode = nullptr;
std::unique_ptr<SceneNode>* dragSrc = nullptr;
std::unique_ptr<SceneNode>* dragDst = nullptr;
static void processNode(Scene& scene, std::unique_ptr<SceneNode>* nodePtr) {
	// https://www.avoyd.com/
	// ^ code for icons

	SceneNode* node = nodePtr->get();

	int treeFlags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_OpenOnArrow;

	if (node == clickedNode) treeFlags |= ImGuiTreeNodeFlags_Selected;
	if (node->children.size() == 0) treeFlags |= ImGuiTreeNodeFlags_Leaf;

	bool treeOpened = ImGui::TreeNodeEx(node, treeFlags, node->name.c_str());

	if (ImGui::BeginDragDropTarget()) {
		if (ImGui::AcceptDragDropPayload("uniqueptr")) {
			dragSrc = *((std::unique_ptr<SceneNode>**)ImGui::GetDragDropPayload()->Data);
			dragDst = nodePtr;
		}

		ImGui::EndDragDropTarget();
	}

	if (ImGui::BeginDragDropSource()) {
		ImGui::SetDragDropPayload("uniqueptr", &nodePtr, sizeof(nodePtr));
		ImGui::EndDragDropSource();
	}

	if (treeOpened) {
		if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) clickedNode = node;
		if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) doubleClickedNode = node;

		for (auto& child : node->children) {
			processNode(scene, &child);
		}

		ImGui::TreePop();
	}
}

static float getInc(float val) {
	// increment position by 1% of the power of 10 used in val
	float inc = 0;
	if (val != 0) inc = pow(10, floor(log(abs(val)) / log(10)) - 2);

	return fmax(0.01, inc); // at least inc by 0.01
}

static float getInc(glm::vec3& vec) {
	return getInc(fmax( // get inc of the largest component
		abs(vec[0]), fmax(
			abs(vec[1]), abs(vec[2]))));
}

bool addVec3(const char* label, glm::vec3& vec, bool* isScalar = nullptr) {
	bool value_changed = false;

	float inc = getInc(vec);

	auto style = ImGui::GetStyle();
	float p_min = 0;
	float p_max = 0;
	ImGui::BeginGroup();
	ImGui::PushID(label);

	int components = 3;
	if (isScalar != nullptr && *isScalar) {
		components = 1;
	}

	ImGui::PushMultiItemsWidths(components, ImGui::CalcItemWidth());

	std::vector<const char*> formats = {"X:%.2f", "Y:%.2f", "Z:%.2f"};

	for (int i = 0; i < components; i++) {
		ImGui::PushID(i);
		if (i > 0) ImGui::SameLine(0, style.ItemInnerSpacing.x);

		if (isScalar != nullptr && *isScalar) {
			value_changed |= ImGui::DragScalar("", ImGuiDataType_Float, &(vec[0]),
				inc, &p_min, &p_max, "%.2f", 0);
			vec[2] = vec[1] = vec[0];
		} else {
			value_changed |= ImGui::DragScalar("", ImGuiDataType_Float, &(vec[i]),
				inc, &p_min, &p_max, formats[i], 0);
		}

		ImGui::PopID();
		ImGui::PopItemWidth();
	}
	ImGui::PopID();

	const char* label_end = ImGui::FindRenderedTextEnd(label);
	if (label != label_end) {
		ImGui::SameLine(0, style.ItemInnerSpacing.x);
		ImGui::TextEx(label, label_end);
	}

	if (isScalar != nullptr && ImGui::IsItemClicked()) {
		if (*isScalar || (vec[0] == vec[1] && vec[1] == vec[2])) { // only toggle if isScalar or all components equal
			*isScalar = not *isScalar;
		}
	}

	ImGui::EndGroup();

	return value_changed;
}

bool addFloat(const char* label, float& val) {
	return ImGui::DragFloat(label, &val, getInc(val), 0, 0, "%.2f");
}

void updateGui(Primrose::Scene& scene) {
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	ImGui::ShowDemoWindow();

	ImGui::Begin("Scene", nullptr, ImGuiWindowFlags_MenuBar);
	if (ImGui::BeginMenuBar()) {
		if (ImGui::BeginMenu("File")) {
			if (ImGui::MenuItem("New", "Ctrl+N")) {
				newScene = true;
			}
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
	for (auto& node : scene.root) {
		processNode(scene, &node);
	}
	ImGui::EndChild();

	// if nodes were dragged during processNode
	if (dragSrc != nullptr && dragDst != nullptr) {
		SceneNode* srcNode = dragSrc->get();
		SceneNode* dstNode = dragDst->get();
		std::cout << dstNode->name << " received payload " << srcNode->name << std::endl;

		std::vector<std::unique_ptr<SceneNode>>* srcVector;
		if (srcNode->parent != nullptr) {
			srcVector = &srcNode->parent->children;
		} else {
			srcVector = &scene.root;
		}
		const auto it = std::find(srcVector->begin(), srcVector->end(), *dragSrc);

		dstNode->children.push_back(std::move(*it));
		srcNode->parent = dstNode;

		srcVector->erase(it);

		dragSrc = nullptr;
		dragDst = nullptr;
	}

	if (clickedNode != nullptr) {
		static SceneNode* oldNode = nullptr;
		bool switchedNode;
		if (clickedNode != oldNode) {
			oldNode = clickedNode;
			switchedNode = true;
		} else {
			switchedNode = false;
		}

		ImGui::BeginChild("Properties");

		ImGui::SeparatorText("SceneNode");

		ImGui::InputText("Name", &clickedNode->name);

		addVec3("Position", clickedNode->translate);

		static bool scaleIsScalar;
		if (switchedNode) {
			scaleIsScalar = clickedNode->scale[0] == clickedNode->scale[1]
				&& clickedNode->scale[1] == clickedNode->scale[2];
		}
		addVec3("Scale", clickedNode->scale, &scaleIsScalar);

		bool rotationChanged = false;
		{ // from DragScalarN, changed to add θ,x,y,z labels
			const char* label = "Rotation";

			const float angleInc = 0.1;
			const float axisInc = 0.01;

			const float angleMin = 0;
			const float angleMax = 360;
			const float axisMin = 0;
			const float axisMax = 1;

			auto style = ImGui::GetStyle();
			ImGui::BeginGroup();
			ImGui::PushID(label);
			ImGui::PushMultiItemsWidths(4, ImGui::CalcItemWidth());
			for (int i = 0; i < 4; i++) {
				ImGui::PushID(i);
				if (i > 0) ImGui::SameLine(0, style.ItemInnerSpacing.x);

				if (i == 0) rotationChanged |= ImGui::DragScalar("", ImGuiDataType_Float, &clickedNode->angle,
					angleInc, &angleMin, &angleMax, "θ:%.1f", 0);
				else if (i == 1) rotationChanged |= ImGui::DragScalar("", ImGuiDataType_Float, &(clickedNode->axis[0]),
					axisInc, &axisMin, &axisMax, "X:%.2f", 0);
				else if (i == 2) rotationChanged |= ImGui::DragScalar("", ImGuiDataType_Float, &(clickedNode->axis[1]),
					axisInc, &axisMin, &axisMax, "Y:%.2f", 0);
				else if (i == 3) rotationChanged |= ImGui::DragScalar("", ImGuiDataType_Float, &(clickedNode->axis[2]),
					axisInc, &axisMin, &axisMax, "Z:%.2f", 0);

				ImGui::PopID();
				ImGui::PopItemWidth();
			}
			ImGui::PopID();

			const char* label_end = ImGui::FindRenderedTextEnd(label);
			if (label != label_end) {
				ImGui::SameLine(0, style.ItemInnerSpacing.x);
				ImGui::TextEx(label, label_end);
			}

			ImGui::EndGroup();
		}
		if (rotationChanged) clickedNode->axis = glm::normalize(clickedNode->axis);

		static bool sizeIsScalar;
		if (switchedNode && clickedNode->type() == NODE_BOX) {
			sizeIsScalar = ((BoxNode*)clickedNode)->size[0] == ((BoxNode*)clickedNode)->size[1]
				&& ((BoxNode*)clickedNode)->size[1] == ((BoxNode*)clickedNode)->size[2];
		}

		ImGui::Checkbox("Hide", &clickedNode->hide);

		switch (clickedNode->type()) {
			case NODE_SPHERE:
				ImGui::SeparatorText("SphereNode");
				addFloat("Radius", ((SphereNode*)clickedNode)->radius);
				break;
			case NODE_BOX:
				ImGui::SeparatorText("BoxNode");
				addVec3("Size", ((BoxNode*)clickedNode)->size, &sizeIsScalar);
				break;
			case NODE_TORUS:
				ImGui::SeparatorText("TorusNode");
				addFloat("Ring Radius", ((TorusNode*)clickedNode)->ringRadius);
				addFloat("Major Radius", ((TorusNode*)clickedNode)->majorRadius);
				break;
			case NODE_LINE:
				ImGui::SeparatorText("LineNode");
				addFloat("Height", ((LineNode*)clickedNode)->height);
				addFloat("Radius", ((LineNode*)clickedNode)->radius);
				break;
			case NODE_CYLINDER:
				ImGui::SeparatorText("CylinderNode");
				addFloat("Radius", ((CylinderNode*)clickedNode)->radius);
				break;
			case NODE_UNION:
				ImGui::SeparatorText("UnionNode");
				break;
			case NODE_INTERSECTION:
				ImGui::SeparatorText("IntersectionNode");
				break;
			case NODE_DIFFERENCE:
				ImGui::SeparatorText("DifferenceNode");
				if (clickedNode->children.size() > 0) {
					ImGui::Text("Mark as subtract:");
				}
				for (const auto& child : clickedNode->children) {
					bool isSubtract = ((DifferenceNode*)clickedNode)->subtractNodes.contains(child.get());
					ImGui::PushID(child.get());
					if (ImGui::Checkbox(child->name.c_str(), &isSubtract)) {
						if (isSubtract) {
							((DifferenceNode*)clickedNode)->subtractNodes.insert(child.get());
						} else {
							((DifferenceNode*)clickedNode)->subtractNodes.erase(child.get());
						}
					}
					ImGui::PopID();
				}
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
		ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_HeaderActive]);
		ImGui::Text("Add Object");
		ImGui::PopStyleColor();

		ImGui::Separator();

		if (ImGui::Selectable("Sphere")) {
			scene.root.emplace_back(new SphereNode(1));
		} else if (ImGui::Selectable("Box")) {
			scene.root.emplace_back(new BoxNode(glm::vec3(1)));
		} else if (ImGui::Selectable("Torus")) {
			scene.root.emplace_back(new TorusNode(0.1, 1));
		} else if (ImGui::Selectable("Line")) {
			scene.root.emplace_back(new LineNode(1, 0.1));
		} else if (ImGui::Selectable("Cylinder")) {
			scene.root.emplace_back(new CylinderNode(1));
		} else if (ImGui::Selectable("Union")) {
			scene.root.emplace_back(new UnionNode());
		} else if (ImGui::Selectable("Intersection")) {
			scene.root.emplace_back(new IntersectionNode());
		} else if (ImGui::Selectable("Difference")) {
			scene.root.emplace_back(new DifferenceNode());
		}

		ImGui::EndPopup();
	}

	if (newScene) {
		scene.root.clear();
		clickedNode = nullptr;
		newScene = false;
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
