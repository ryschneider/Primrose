#include "gui.hpp"

#include <Primrose/engine/setup.hpp>
#include <Primrose/scene/scene.hpp>
#include <Primrose/scene/primitive_node.hpp>
#include <Primrose/scene/node_visitor.hpp>
#include <stdexcept>
#include <iostream>
#include <map>
#include <imgui/imgui_internal.h>
#include <imgui/misc/cpp/imgui_stdlib.h>
#include <tinyfiledialogs.h>

using namespace Primrose;

VkDescriptorPool imguiDescriptorPool;

std::filesystem::path lastLoadedScene = "scenes/";

static bool addObject = false;
static bool duplicateObject = false;
static bool deleteObject = false;

static bool newScene = false;
static bool openScene = false;
static bool saveScene = false;

static bool undoEdit = false;
static bool redoEdit = false;

Node* clickedNode = nullptr;
Node* doubleClickedNode = nullptr;
Node* dragSrc = nullptr;
Node* dragDst = nullptr;

void guiKeyCb(int key, int action, int mods) {
	if (action == GLFW_PRESS) {
		if (mods & GLFW_MOD_CONTROL) {
			if (key == GLFW_KEY_N) newScene = true;
			if (key == GLFW_KEY_O) openScene = true;
			if (key == GLFW_KEY_S) saveScene = true;
		}

		if (mods & GLFW_MOD_SHIFT) {
			if (key == GLFW_KEY_A) addObject = true;
			if (key == GLFW_KEY_D) duplicateObject = true;
		}

		if (key == GLFW_KEY_DELETE) deleteObject = true;
	}

	if (action == GLFW_PRESS || action == GLFW_REPEAT) {
		if (key == GLFW_KEY_UP && clickedNode != nullptr) {
			if (clickedNode == clickedNode->getParent()->getChildren()[0].get()) {
				clickedNode = clickedNode->getParent();
			} else {
				clickedNode = (clickedNode->locationInParent() - 1)->get();
				while (clickedNode->getChildren().size() > 0) {
					clickedNode = clickedNode->getChildren().back().get();
				}
			}

			if (clickedNode->getParent() == nullptr) {
				clickedNode = nullptr; // root node
			}
		}

		if (key == GLFW_KEY_DOWN && clickedNode != nullptr) {
			if (clickedNode->getChildren().size() > 0) {
				clickedNode = clickedNode->getChildren()[0].get();
			} else {
				while (clickedNode == clickedNode->getParent()->getChildren().back().get()) {
					// while parent is the last sibling, move up parents

					if (clickedNode->getParent() == nullptr) {
						return; // already at last node in root
					}

					clickedNode = clickedNode->getParent();
				}

				// get next sibling
				clickedNode = (clickedNode->locationInParent() + 1)->get();
			}
		}

		if (key == GLFW_KEY_LEFT && clickedNode != nullptr) {
			if (clickedNode->getParent()->getParent() != nullptr) { // don't switch to root node
				clickedNode = clickedNode->getParent();
			}
		}

		if (key == GLFW_KEY_RIGHT && clickedNode != nullptr) {
			if (clickedNode->getChildren().size() > 0) {
				clickedNode = clickedNode->getChildren()[0].get();
			}
		}

		if (mods & GLFW_MOD_CONTROL) {
			if (key == GLFW_KEY_Z) undoEdit = true;
			if (key == GLFW_KEY_Y) redoEdit = true;
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

void renderGui(VkCommandBuffer& cmd) {
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
}

void cleanupGui() {
	vkDestroyDescriptorPool(device, imguiDescriptorPool, nullptr);
	ImGui_ImplGlfw_Shutdown();
	ImGui_ImplVulkan_Shutdown();
}



static void addTreeNode(Scene& scene, Node* node) {
	// https://www.avoyd.com/
	// ^ code for icons

	int treeFlags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_OpenOnArrow;

	if (node == clickedNode) treeFlags |= ImGuiTreeNodeFlags_Selected;
	if (node->getChildren().size() == 0) treeFlags |= ImGuiTreeNodeFlags_Leaf;

	bool treeOpened = ImGui::TreeNodeEx(node, treeFlags, node->name.c_str());

	if (ImGui::BeginDragDropTarget()) {
		if (ImGui::AcceptDragDropPayload("uniqueptr")) {
			dragSrc = *((Node**)ImGui::GetDragDropPayload()->Data);
			dragDst = node;
		}

		ImGui::EndDragDropTarget();
	}

	if (ImGui::BeginDragDropSource()) {
		ImGui::SetDragDropPayload("uniqueptr", &node, sizeof(node));
		ImGui::EndDragDropSource();
	}

	if (treeOpened) {
		if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) clickedNode = node;
		if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) doubleClickedNode = node;

		for (auto& child : node->getChildren()) {
			addTreeNode(scene, child.get());
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

bool addFloat(const char* label, float& val, float min = 0, float max = 0) {
	return ImGui::DragFloat(label, &val, getInc(val), min, max, "%.2f");
}

bool Splitter(bool split_vertically, float thickness, float* size1, float* size2,
	float min_size1, float min_size2, float splitter_long_axis_size = -1.0f) {

	// https://github.com/ocornut/imgui/issues/319#issuecomment-345795629
	using namespace ImGui;
	ImGuiContext& g = *GImGui;
	ImGuiWindow* guiWindow = g.CurrentWindow;
	ImGuiID id = guiWindow->GetID("##Splitter");
	ImRect bb;

	bb.Min = guiWindow->DC.CursorPos;
	if (split_vertically) bb.Min.x += *size1;
	else bb.Min.y += *size1;

	ImVec2 item = CalcItemSize(split_vertically ? ImVec2(thickness, splitter_long_axis_size) : ImVec2(splitter_long_axis_size, thickness), 0.0f, 0.0f);
	bb.Max = ImVec2(bb.Min.x + item.x, bb.Min.y + item.y);
	return SplitterBehavior(bb, id, split_vertically ? ImGuiAxis_X : ImGuiAxis_Y, size1, size2, min_size1, min_size2, 0.0f);
}

class DisplayNodeProperties : public NodeVisitor {
public:
	DisplayNodeProperties(bool* modified, bool switchedNode, bool* sizeIsScalar)
		: modified(modified), switchedNode(switchedNode), sizeIsScalar(sizeIsScalar) {}
	bool* modified;
	bool switchedNode;
	bool* sizeIsScalar;

	void visit(SphereNode* node) override {
		ImGui::SeparatorText("SphereNode");
		*modified |= addFloat("Radius", node->radius);
	}
	void visit(BoxNode* node) override {
		if (switchedNode) {
			*sizeIsScalar = node->size[0] == node->size[1] && node->size[1] == node->size[2];
		}

		ImGui::SeparatorText("BoxNode");
		*modified |= addVec3("Size", node->size, sizeIsScalar);
	}
	void visit(TorusNode* node) override {
		ImGui::SeparatorText("TorusNode");
		*modified |= addFloat("Ring Radius", node->ringRadius);
		*modified |= addFloat("Major Radius", node->majorRadius);
	}
	void visit(LineNode* node) override {
		ImGui::SeparatorText("LineNode");
		*modified |= addFloat("Height", node->height);
		*modified |= addFloat("Radius", node->radius);
	}
	void visit(CylinderNode* node) override {
		ImGui::SeparatorText("CylinderNode");
		*modified |= addFloat("Radius", node->radius);
	}
	void visit(UnionNode* node) override {
		ImGui::SeparatorText("UnionNode");
	}
	void visit(IntersectionNode* node) override {
		ImGui::SeparatorText("IntersectionNode");
	}
	void visit(DifferenceNode* node) override {
		ImGui::SeparatorText("DifferenceNode");
		if (node->getChildren().size() > 0) {
			ImGui::Text("Mark as subtract:");
		}
		for (const auto& child: clickedNode->getChildren()) {
			bool isSubtract = node->subtractNodes.contains(child.get());
			ImGui::PushID(child.get());
			if (ImGui::Checkbox(child->name.c_str(), &isSubtract)) {
				*modified = true;
				if (isSubtract) {
					node->subtractNodes.insert(child.get());
				} else {
					node->subtractNodes.erase(child.get());
				}
			}
			ImGui::PopID();
		}
	}
};

const static int numTempSaves = 50;
static int currentTempSave = 0;
static int numUndos = 0;
void makeTempSave(Scene& scene) {
	scene.saveScene(fmt::format("C:\\temp\\temp_scene_{}.json", currentTempSave++));
	currentTempSave %= numTempSaves;
	numUndos = 0;
}
void undoTempSave(Scene& scene) {
	if (numUndos == 0) scene.saveScene(fmt::format("C:\\temp\\temp_scene_{}.json", currentTempSave));
//	if (numUndos >= numTempSaves-1) return;

	numUndos += 1;
	currentTempSave -= 1;
	currentTempSave = (currentTempSave + numTempSaves) % numTempSaves;

	scene.root.clear();
	clickedNode = nullptr;
	scene.importScene(fmt::format("C:\\temp\\temp_scene_{}.json", currentTempSave));
}
void redoTempSave(Scene& scene) {
//	if (numUndos == 0) return;

	numUndos -= 1;
	currentTempSave += 1;
	currentTempSave %= numTempSaves;

	scene.root.clear();
	clickedNode = nullptr;
	scene.importScene(fmt::format("C:\\temp\\temp_scene_{}.json", currentTempSave));
}

bool updateGui(Primrose::Scene& scene, float dt) {
	bool modifiedScene = false;

	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
	ImGui::Begin("FPS", nullptr,
		ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration);
	ImGui::Text("FPS: %.2f", 1.f / dt);
	ImGui::End();

	ImGui::ShowDemoWindow();

	ImGui::Begin("Scene", nullptr, ImGuiWindowFlags_MenuBar);
	if (ImGui::BeginMenuBar()) {
		if (ImGui::BeginMenu("File")) {
			if (ImGui::MenuItem("New", "Ctrl+N")) newScene = true;
			if (ImGui::MenuItem("Open...", "Ctrl+O")) openScene = true;
			if (ImGui::MenuItem("Save", "Ctrl+S")) saveScene = true;
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Object")) {
			if (ImGui::MenuItem("Add", "Shift+A")) addObject = true;
			if (ImGui::MenuItem("Duplicate", "Shift+D")) duplicateObject = true;
			if (ImGui::MenuItem("Delete", "Del")) deleteObject = true;
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Edit")) {
			if (ImGui::MenuItem("Undo", "Ctrl+Z")) undoEdit = true;
			if (ImGui::MenuItem("Redo", "Ctrl+Y")) redoEdit = true;
			ImGui::EndMenu();
		}
		ImGui::EndMenuBar();
	}

	static float hierarchySize = 260;
	float propertiesSize = ImGui::GetContentRegionAvail().y - hierarchySize - 4;
	Splitter(false, 8, &hierarchySize, &propertiesSize, 8, 8, ImGui::GetContentRegionAvail().x);

	ImGui::BeginChild("Scene", ImVec2(ImGui::GetContentRegionAvail().x, hierarchySize));
	for (auto& node : scene.root.getChildren()) {
		addTreeNode(scene, node.get());
	}
	ImGui::EndChild();

	ImGui::BeginChild("Properties", ImVec2(ImGui::GetContentRegionAvail().x, propertiesSize));

	// if nodes were dragged during addTreeNode
	if (dragSrc != nullptr && dragDst != nullptr) {
		modifiedScene = true;

		if (dragDst == dragSrc->getParent()) {
//			dragSrc->swap(dragDst);
			dragSrc->reparent(dragDst->getParent());
		} else {
			dragSrc->reparent(dragDst);
		}

		dragSrc = nullptr;
		dragDst = nullptr;
	}

	if (clickedNode != nullptr) {
		static Node* oldNode = nullptr;
		bool switchedNode;
		if (clickedNode != oldNode) {
			oldNode = clickedNode;
			switchedNode = true;
		} else {
			switchedNode = false;
		}

		ImGui::SeparatorText("SceneNode");

//		ImGui::Text("0x%p", clickedNode);

		modifiedScene |= ImGui::InputText("Name", &clickedNode->name);

		modifiedScene |= addVec3("Position", clickedNode->translate);

		static bool scaleIsScalar;
		if (switchedNode) {
			scaleIsScalar = clickedNode->scale[0] == clickedNode->scale[1]
				&& clickedNode->scale[1] == clickedNode->scale[2];
		}
		modifiedScene |= addVec3("Scale", clickedNode->scale, &scaleIsScalar);

		bool rotationChanged = false;
		{ // from DragScalarN, changed to add θ,x,y,z labels
			const char* label = "Rotation";

			const float angleInc = 0.1;
			const float axisInc = 0.01;

			const float angleMin = 0;
			const float angleMax = 360;
			const float axisMin = -1;
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
		if (rotationChanged) {
			modifiedScene = true;
			clickedNode->axis = glm::normalize(clickedNode->axis);
		}

		modifiedScene |= ImGui::Checkbox("Hide", &clickedNode->hide);

		static bool sizeIsScalar;
		DisplayNodeProperties visitor(&modifiedScene, switchedNode, &sizeIsScalar);
		clickedNode->accept(&visitor);
	}

	ImGui::EndChild();
	ImGui::End();

	if (addObject) {
		addObject = false;
		ImGui::OpenPopup("Add Object");
	}

	ImGui::SetNextWindowBgAlpha(1);
	if (ImGui::BeginPopup("Add Object")) {
		ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_HeaderActive]);
		ImGui::Text("Add Object");
		ImGui::PopStyleColor();

		ImGui::Separator();

		Node* parent;
		if (clickedNode != nullptr) {
			parent = clickedNode->getParent();
		} else {
			parent = reinterpret_cast<Node*>(&scene.root);
		}

		modifiedScene = true;
		if (ImGui::Selectable("Sphere")) {
			new SphereNode(parent, 1);
		} else if (ImGui::Selectable("Box")) {
			new BoxNode(parent, glm::vec3(1));
		} else if (ImGui::Selectable("Torus")) {
			new TorusNode(parent, 0.1, 1);
		} else if (ImGui::Selectable("Line")) {
			new LineNode(parent, 1, 0.1);
		} else if (ImGui::Selectable("Cylinder")) {
			new CylinderNode(parent, 1);
		} else if (ImGui::Selectable("Union")) {
			new UnionNode(parent);
		} else if (ImGui::Selectable("Intersection")) {
			new IntersectionNode(parent);
		} else if (ImGui::Selectable("Difference")) {
			new DifferenceNode(parent);
		} else {
			modifiedScene = false;
		}

		ImGui::EndPopup();
	}

	if (duplicateObject) {
		modifiedScene = true;
		duplicateObject = false;

		if (clickedNode != nullptr) {
//			clickedNode->duplicate();
		}
	}

	if (openScene) {
		modifiedScene = true;
		openScene = false;

		const char* patterns = "*.json";
		const char* path = tinyfd_openFileDialog("Save Scene", lastLoadedScene.string().c_str(), 1,
			&patterns, "Scene File", false);

		if (path) {
			scene.root.clear();
			clickedNode = nullptr;
			scene.importScene(path);
			lastLoadedScene = path;
		}
	}

	if (saveScene) {
		saveScene = false;

		const char* patterns = "*.json";
		const char* path = tinyfd_saveFileDialog("Save Scene", lastLoadedScene.string().c_str(), 1,
			&patterns, "Scene File");

		if (path) {
			std::cout << "Saving scene to " << std::filesystem::path(path).make_preferred() << std::endl;
			scene.saveScene(path);
			lastLoadedScene = path;
		}
	}

	if (deleteObject) {
		modifiedScene = true;
		deleteObject = false;

		if (clickedNode != nullptr) {
			delete clickedNode;
			clickedNode = nullptr;
		}
	}

	if (newScene) {
		modifiedScene = true;
		newScene = false;

		int warning = tinyfd_messageBox("Level Editor", "Create a new scene?",
			"yesno", "question", 0);
		if (warning == 1) { // yes
			scene.root.clear();
			clickedNode = nullptr;
		}
	}

	ImGui::Render();

	if (modifiedScene) {
		makeTempSave(scene);
	} else if (undoEdit) {
		modifiedScene = true;
		undoEdit = false;
		undoTempSave(scene);
	} else if (redoEdit) {
		modifiedScene = true;
		redoEdit = false;
		redoTempSave(scene);
	}

	return modifiedScene;
}


