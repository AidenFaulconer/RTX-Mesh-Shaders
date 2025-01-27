/*!
 *  \author    Viktor Zoutman
 *  \date      2019-2020
 *  \copyright GNU General Public License v3.0
 */

#include "context.hpp"

#include <vector>
#include <GLFW/glfw3.h>
#include <map>
#include <set>
#include <string>
#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#include "../application.hpp"
#include "gfx_settings.hpp"
#include "../util/log.hpp"

namespace internal
{

	static VKAPI_ATTR VkBool32 VKAPI_CALL ValidationLayerCallback(
			VkDebugUtilsMessageSeverityFlagBitsEXT severity,
			VkDebugUtilsMessageTypeFlagsEXT type,
			const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
			[[maybe_unused]] void* user_data)
	{

		std::string prefix = "";

		switch(type)
		{
			case VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT:
			default:
				prefix += "[VALIDATION:COMMON]";
				break;
			case VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT :
				prefix += "[VALIDATION:SPEC]";
				break;
			case VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT :
				prefix += "[VALIDATION:PERF]";
				return VK_FALSE;
				break;
		}

		switch(severity)
		{
			case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
				LOG("{} Validation Layer: {}", prefix, callback_data->pMessage);
				break;
			default:
			case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
				LOGW("{} Validation Layer: {}", prefix, callback_data->pMessage);
				break;
			case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
				LOGE("{} Validation Layer: {}", prefix, callback_data->pMessage);
				break;
		}


		return VK_FALSE;
	}

	static 	VkDebugUtilsMessengerCreateInfoEXT GetDebugMessengerCreateInfo()
	{
		VkDebugUtilsMessengerCreateInfoEXT create_info = {};
		create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		create_info.pfnUserCallback = internal::ValidationLayerCallback;
		create_info.pUserData = nullptr;

		return create_info;
	}

	VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* create_info, const VkAllocationCallbacks* allocator, VkDebugUtilsMessengerEXT* debug_messenger)
	{
		auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
		if (func != nullptr)
		{
			return func(instance, create_info, allocator, debug_messenger);
		} else
		{
			return VK_ERROR_EXTENSION_NOT_PRESENT;
		}
	}

	void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debug_messenger, const VkAllocationCallbacks* allocator)
	{
		auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
		if (func != nullptr)
		{
			func(instance, debug_messenger, allocator);
		}
	}

} /* internal */

gfx::Context::Context(Application* app)
	: m_app_info(),
	m_instance_create_info(),
	m_instance(VK_NULL_HANDLE),
	m_debug_messenger_create_info(),
	m_debug_messenger(),
	m_logical_device(VK_NULL_HANDLE),
	m_physical_device(VK_NULL_HANDLE),
	m_physical_device_features(),
	m_physical_device_properties(),
	m_physical_device_mem_properties(),
	m_surface(VK_NULL_HANDLE),
	m_app(app)
{

	std::uint32_t glfw_extension_count;
	const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);

	// TODO: Check if the glfw extensions are supported.
	std::vector<const char*> extensions(glfw_extensions, glfw_extensions + glfw_extension_count);

	// Add instance extensions
	for (auto const & ex : settings::instance_extensions)
	{
		extensions.push_back(ex);
	}

	if (gfx::settings::enable_validation_layers)
	{
		m_debug_messenger_create_info = internal::GetDebugMessengerCreateInfo();
		if (!HasValidationLayerSupport())
		{
			LOGE("Requested validation layers but doesn't support it");
		}
		extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	}

	auto app_version = VK_MAKE_VERSION(1, 0, 0);

	m_app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	m_app_info.pApplicationName = "Mesh Shader Demo";
	m_app_info.applicationVersion = app_version;
	m_app_info.pEngineName = "irrelevant";
	m_app_info.engineVersion = app_version;
	m_app_info.apiVersion = VK_API_VERSION_1_1;

	m_instance_create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	m_instance_create_info.pApplicationInfo = &m_app_info;
	m_instance_create_info.pNext = gfx::settings::enable_validation_layers ? &m_debug_messenger_create_info : nullptr;
	m_instance_create_info.enabledLayerCount = gfx::settings::enable_validation_layers ? gfx::settings::validation_layers.size() : 0;
	m_instance_create_info.ppEnabledLayerNames = gfx::settings::validation_layers.data();
	m_instance_create_info.enabledExtensionCount = extensions.size();
	m_instance_create_info.ppEnabledExtensionNames = extensions.data();

	auto result = vkCreateInstance(&m_instance_create_info, nullptr, &m_instance);
	if (result != VK_SUCCESS)
	{
		LOGC("Failed to create a Vulkan instance.");
	}

	if (gfx::settings::enable_validation_layers)
	{
		EnableDebugCallback();
	}

	CreateSurface();

	// Get the device and its properties.
	m_physical_device = FindPhysicalDevice();

	m_physical_device_raytracing_properties = {};
	m_physical_device_raytracing_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PROPERTIES_NV;
	m_physical_device_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	m_physical_device_properties.pNext = &m_physical_device_raytracing_properties;

	vkGetPhysicalDeviceProperties2(m_physical_device, &m_physical_device_properties);

	vkGetPhysicalDeviceMemoryProperties(m_physical_device, &m_physical_device_mem_properties);

	VkPhysicalDevice8BitStorageFeaturesKHR bit8storage_features = {};
	bit8storage_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_8BIT_STORAGE_FEATURES_KHR;
	bit8storage_features.storageBuffer8BitAccess = true;
	bit8storage_features.uniformAndStorageBuffer8BitAccess = true;

	VkPhysicalDeviceFloat16Int8FeaturesKHR float16int8_features = {};
	float16int8_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FLOAT16_INT8_FEATURES_KHR;
	float16int8_features.pNext = &bit8storage_features;

	VkPhysicalDeviceMeshShaderFeaturesNV nv_features = {};
	nv_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_NV;
	nv_features.meshShader = 1;
	nv_features.taskShader = 1;
	nv_features.pNext = &float16int8_features;

	m_physical_device_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	m_physical_device_features.pNext = &nv_features;

	vkGetPhysicalDeviceFeatures2(m_physical_device, &m_physical_device_features);

	// Enable mesh shading
	m_queue_family_indices = FindQueueFamilies(m_physical_device);
	m_swapchain_support_details = GetSwapChainSupportDetails(m_physical_device);

	CreateLogicalDevice();

	if (gfx::settings::enable_validation_layers)
	{
		SetupDebugMarkerExtension();
	}

	SetupMeshShadingExtension();
	SetupRayTracingExtension();

	SetupVMA();
}

gfx::Context::~Context()
{
	vmaDestroyAllocator(m_vma_allocator);

	vkDestroySurfaceKHR(m_instance, m_surface, nullptr);

	if (gfx::settings::enable_validation_layers)
	{
		internal::DestroyDebugUtilsMessengerEXT(m_instance, m_debug_messenger, nullptr);
	}

	vkDestroyDevice(m_logical_device, nullptr);
	vkDestroyInstance(m_instance, nullptr);
}

std::vector<VkExtensionProperties> gfx::Context::GetSupportedExtensions()
{
	std::uint32_t extension_count = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr);

	std::vector<VkExtensionProperties> extensions(extension_count);
	vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, extensions.data());

	return extensions;
}

std::vector<VkExtensionProperties> gfx::Context::GetSupportedDeviceExtensions()
{
	return GetSupportedDeviceExtensions(m_physical_device);
}

VkPhysicalDeviceProperties2 gfx::Context::GetPhysicalDeviceProperties()
{
	return m_physical_device_properties;
}

VkPhysicalDeviceRayTracingPropertiesNV gfx::Context::GetRayTracingDeviceProperties()
{
	return m_physical_device_raytracing_properties;
}

const VkPhysicalDeviceMemoryProperties* gfx::Context::GetPhysicalDeviceMemoryProperties()
{
	VkPhysicalDeviceMemoryProperties const * properties = new (VkPhysicalDeviceMemoryProperties);
	vmaGetMemoryProperties(m_vma_allocator, &properties);
	return properties;
}

std::vector<VkExtensionProperties> gfx::Context::GetSupportedDeviceExtensions(VkPhysicalDevice device)
{
	std::uint32_t extension_count = 0;
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, nullptr);

	std::vector<VkExtensionProperties> extensions(extension_count);
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, extensions.data());

	return extensions;
}

bool gfx::Context::HasValidationLayerSupport()
{
	std::uint32_t layerCount;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

	std::vector<VkLayerProperties> availableLayers(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

	for (const char* layerName : gfx::settings::validation_layers)
	{
		bool layerFound = false;

		for (const auto& layerProperties : availableLayers)
		{
			if (strcmp(layerName, layerProperties.layerName) == 0)
			{
				layerFound = true;
				break;
			}
		}

		if (!layerFound)
		{
			return false;
		}
	}

	return true;
}

std::uint32_t gfx::Context::GetDirectQueueFamilyIdx()
{
	return m_queue_family_indices.direct_family.value();
}

void gfx::Context::WaitForDevice()
{
	vkDeviceWaitIdle(m_logical_device);
}

std::uint32_t gfx::Context::FindMemoryType(std::uint32_t filter, VkMemoryPropertyFlags properties)
{
	for (uint32_t i = 0; i < m_physical_device_mem_properties.memoryTypeCount; i++)
	{
		if ((filter & (1 << i)) && (m_physical_device_mem_properties.memoryTypes[i].propertyFlags & properties) == properties)
		{
			return i;
		}
	}

	LOGC("failed to find suitable memory type!");
}

VmaStats gfx::Context::CalculateVMAStats()
{
	VmaStats stats;
	vmaCalculateStats(m_vma_allocator, &stats);
	return stats;
}

void gfx::Context::CreateSurface()
{
	if (glfwCreateWindowSurface(m_instance, m_app->GetWindow(), NULL, &m_surface) != VK_SUCCESS)
	{
		LOGC("failed to create window surface!");
	}
}

void gfx::Context::CreateLogicalDevice()
{
	float queue_priority = 1;

	VkDeviceQueueCreateInfo queue_create_info = {};
	queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queue_create_info.queueFamilyIndex = m_queue_family_indices.direct_family.value();
	queue_create_info.queueCount = 1;
	queue_create_info.pQueuePriorities = &queue_priority;

	VkDeviceCreateInfo create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	create_info.pQueueCreateInfos = &queue_create_info;
	create_info.queueCreateInfoCount = 1;
	//create_info.pEnabledFeatures = &m_physical_device_features.features;
	create_info.pEnabledFeatures = nullptr;
	create_info.pNext = &m_physical_device_features;

	auto x = (VkPhysicalDeviceMeshShaderFeaturesNV*)m_physical_device_features.pNext;

	create_info.enabledExtensionCount = gfx::settings::device_extensions.size();
	create_info.ppEnabledExtensionNames = gfx::settings::device_extensions.data();
	create_info.ppEnabledLayerNames = gfx::settings::validation_layers.data();
	create_info.enabledLayerCount = gfx::settings::enable_validation_layers ? static_cast<uint32_t>(gfx::settings::validation_layers.size()) : 0;

	if (vkCreateDevice(m_physical_device, &create_info, nullptr, &m_logical_device) != VK_SUCCESS)
	{
		LOGC("failed to create logical device!");
	}
}

VkPhysicalDevice gfx::Context::FindPhysicalDevice()
{
	VkPhysicalDevice retval = VK_NULL_HANDLE;

	std::uint32_t device_count = 0;
	vkEnumeratePhysicalDevices(m_instance, &device_count, nullptr);

	if (device_count == 0)
	{
		LOGC("failed to find GPUs with Vulkan support!");
	}

	std::vector<VkPhysicalDevice> devices(device_count);
	vkEnumeratePhysicalDevices(m_instance, &device_count, devices.data());

	std::multimap<int, VkPhysicalDevice> candidates;

	for (const auto& device : devices)
	{
		int score = GetDeviceSuitabilityRating(device);
		candidates.insert(std::make_pair(score, device));
	}

	// Check if the best candidate is suitable at all
	if (candidates.rbegin()->first > 0)
	{
		retval = candidates.rbegin()->second;
	}
	else
	{
		LOGC("failed to find a suitable GPU!");
	}

	return retval;
}

bool gfx::Context::HasExtensionSupport(VkPhysicalDevice device)
{
	auto available_extensions = GetSupportedDeviceExtensions(device);

	std::set<std::string> required_extensions(gfx::settings::device_extensions.begin(), gfx::settings::device_extensions.end());

	for (const auto& extension : available_extensions)
	{
		required_extensions.erase(extension.extensionName);
	}

	return required_extensions.empty();
}

gfx::SwapChainSupportDetails gfx::Context::GetSwapChainSupportDetails(VkPhysicalDevice device)
{
	SwapChainSupportDetails details;

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_surface, &details.m_capabilities);

	std::uint32_t format_count;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &format_count, nullptr);
	if (format_count != 0) {
		details.m_formats.resize(format_count);
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &format_count, details.m_formats.data());
	}

	std::uint32_t present_mode_count;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &present_mode_count, nullptr);
	if (present_mode_count != 0) {
		details.m_present_modes.resize(present_mode_count);
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &present_mode_count, details.m_present_modes.data());
	}

	return details;
}

std::uint32_t gfx::Context::GetDeviceSuitabilityRating(VkPhysicalDevice device)
{
	// Application can't function without a graphics queue family.
	QueueFamilyIndices queue_family_indices = FindQueueFamilies(device);
	if (!queue_family_indices.HasDirectFamily())
	{
		return 0;
	}

	bool supports_required_extensions = HasExtensionSupport(device);
	if (!supports_required_extensions)
	{
		return 0;
	}

	auto swapchain_support_details = GetSwapChainSupportDetails(device);
	bool swapchain_adaquate = !swapchain_support_details.m_formats.empty() && !swapchain_support_details.m_present_modes.empty();
	if (!swapchain_adaquate)
	{
		return 0;
	}

	VkPhysicalDeviceProperties properties;
	VkPhysicalDeviceFeatures features;
	vkGetPhysicalDeviceProperties(device, &properties);
	vkGetPhysicalDeviceFeatures(device, &features);

	std::uint32_t score = 0;

	// Discrete GPUs have a significant performance advantage
	if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
	{
		score += 1000;
	}

	// Maximum possible size of textures affects graphics quality
	score += properties.limits.maxImageDimension2D;

	// Application can't function without geometry shaders or tessellation.
	if (!features.geometryShader || !features.tessellationShader)
	{
		return 0;
	}

	return score;
}

gfx::QueueFamilyIndices gfx::Context::FindQueueFamilies(VkPhysicalDevice device)
{
	QueueFamilyIndices retval;

	std::uint32_t family_count = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &family_count, nullptr);

	std::vector<VkQueueFamilyProperties> families(family_count);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &family_count, families.data());

	for (std::size_t i = 0; i < families.size(); i++)
	{
		VkBool32 present_support = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_surface, &present_support);

		const auto & family = families[i];
		if (family.queueCount > 0 && family.queueFlags & VK_QUEUE_GRAPHICS_BIT && present_support)
		{
			retval.direct_family = i;
		}
	}

	return retval;
}

void gfx::Context::EnableDebugCallback()
{
	if (internal::CreateDebugUtilsMessengerEXT(m_instance, &m_debug_messenger_create_info, nullptr, &m_debug_messenger) != VK_SUCCESS)
	{
		LOGC("failed to set up debug messenger!");
	}
}

void gfx::Context::SetupDebugMarkerExtension()
{
	DebugMarkerSetObjectTag = reinterpret_cast<PFN_vkDebugMarkerSetObjectTagEXT>(vkGetDeviceProcAddr(m_logical_device, "vkDebugMarkerSetObjectTagEXT"));
	DebugMarkerSetObjectName = reinterpret_cast<PFN_vkDebugMarkerSetObjectNameEXT>(vkGetDeviceProcAddr(m_logical_device, "vkDebugMarkerSetObjectNameEXT"));
	CmdDebugMarkerBegin = reinterpret_cast<PFN_vkCmdDebugMarkerBeginEXT>(vkGetDeviceProcAddr(m_logical_device, "vkCmdDebugMarkerBeginEXT"));
	CmdDebugMarkerEnd = reinterpret_cast<PFN_vkCmdDebugMarkerEndEXT>(vkGetDeviceProcAddr(m_logical_device, "vkCmdDebugMarkerEndEXT"));
	CmdDebugMarkerInsert = reinterpret_cast<PFN_vkCmdDebugMarkerInsertEXT>(vkGetDeviceProcAddr(m_logical_device, "vkCmdDebugMarkerInsertEXT"));
}

void gfx::Context::SetupMeshShadingExtension()
{
	CmdDrawMeshTasksNV = reinterpret_cast<PFN_vkCmdDrawMeshTasksNV>(vkGetDeviceProcAddr(m_logical_device, "vkCmdDrawMeshTasksNV"));
}

void gfx::Context::SetupRayTracingExtension()
{
	vkCreateAccelerationStructureNV = reinterpret_cast<PFN_vkCreateAccelerationStructureNV>(vkGetDeviceProcAddr(m_logical_device, "vkCreateAccelerationStructureNV"));
	vkDestroyAccelerationStructureNV = reinterpret_cast<PFN_vkDestroyAccelerationStructureNV>(vkGetDeviceProcAddr(m_logical_device, "vkDestroyAccelerationStructureNV"));
	vkBindAccelerationStructureMemoryNV = reinterpret_cast<PFN_vkBindAccelerationStructureMemoryNV>(vkGetDeviceProcAddr(m_logical_device, "vkBindAccelerationStructureMemoryNV"));
	vkGetAccelerationStructureHandleNV = reinterpret_cast<PFN_vkGetAccelerationStructureHandleNV>(vkGetDeviceProcAddr(m_logical_device, "vkGetAccelerationStructureHandleNV"));
	vkGetAccelerationStructureMemoryRequirementsNV = reinterpret_cast<PFN_vkGetAccelerationStructureMemoryRequirementsNV>(vkGetDeviceProcAddr(m_logical_device, "vkGetAccelerationStructureMemoryRequirementsNV"));
	vkCmdBuildAccelerationStructureNV = reinterpret_cast<PFN_vkCmdBuildAccelerationStructureNV>(vkGetDeviceProcAddr(m_logical_device, "vkCmdBuildAccelerationStructureNV"));
	vkCreateRayTracingPipelinesNV = reinterpret_cast<PFN_vkCreateRayTracingPipelinesNV>(vkGetDeviceProcAddr(m_logical_device, "vkCreateRayTracingPipelinesNV"));
	vkGetRayTracingShaderGroupHandlesNV = reinterpret_cast<PFN_vkGetRayTracingShaderGroupHandlesNV>(vkGetDeviceProcAddr(m_logical_device, "vkGetRayTracingShaderGroupHandlesNV"));
	vkCmdTraceRaysNV = reinterpret_cast<PFN_vkCmdTraceRaysNV>(vkGetDeviceProcAddr(m_logical_device, "vkCmdTraceRaysNV"));
}

void gfx::Context::SetupVMA()
{
	VmaAllocatorCreateInfo allocator_info = {};
	allocator_info.physicalDevice = m_physical_device;
	allocator_info.device = m_logical_device;
	allocator_info.frameInUseCount = settings::num_back_buffers - 1;

	vmaCreateAllocator(&allocator_info, &m_vma_allocator);
}

bool gfx::QueueFamilyIndices::HasDirectFamily()
{
	return direct_family.has_value();
}
