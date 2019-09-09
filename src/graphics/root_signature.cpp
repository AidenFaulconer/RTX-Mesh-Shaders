#include "root_signature.hpp"

#include "context.hpp"
#include "gfx_settings.hpp"

gfx::RootSignature::RootSignature(Context* context)
	: m_context(context), m_pipeline_layout(VK_NULL_HANDLE), m_pipeline_layout_info()
{

}

gfx::RootSignature::~RootSignature()
{
	auto logical_device = m_context->m_logical_device;

	vkDestroyPipelineLayout(logical_device, m_pipeline_layout, nullptr);
	for (auto& layout : m_descriptor_set_layouts)
	{
		vkDestroyDescriptorSetLayout(logical_device, layout, nullptr);
	}
}

void gfx::RootSignature::Compile()
{
	auto logical_device = m_context->m_logical_device;

	m_descriptor_set_layouts.resize(1);
	m_layout_bindings.resize(1);

	m_layout_bindings[0].binding = 0;
	m_layout_bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	m_layout_bindings[0].descriptorCount = 1;
	m_layout_bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	m_layout_bindings[0].pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutCreateInfo descriptor_set_create_info = {};
	descriptor_set_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	descriptor_set_create_info.bindingCount = m_layout_bindings.size();
	descriptor_set_create_info.pBindings = m_layout_bindings.data();

	if (vkCreateDescriptorSetLayout(logical_device, &descriptor_set_create_info, nullptr, &m_descriptor_set_layouts[0]) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create descriptor set layout!");
	}

	m_pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	m_pipeline_layout_info.setLayoutCount = m_descriptor_set_layouts.size();
	m_pipeline_layout_info.pSetLayouts = m_descriptor_set_layouts.data();
	m_pipeline_layout_info.pushConstantRangeCount = 0;
	m_pipeline_layout_info.pPushConstantRanges = nullptr;

	if (vkCreatePipelineLayout(logical_device, &m_pipeline_layout_info, nullptr, &m_pipeline_layout) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create pipeline layout!");
	}
}