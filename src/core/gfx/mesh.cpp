#include "core/gfx/mesh.hpp"
#include "core/gfx/buffer.hpp"
#include "core/gfx/device.hpp"
#include "core/gfx/command_context.hpp"
#include <cstring>

namespace luster::gfx
{
	void Mesh::createCube(Device& device)
	{
		struct V
		{
			float px, py, pz;
			float r, g, b;
		};
		const V verts[] = {
			{-0.5f, -0.5f, 0.5f, 1, 0, 0}, {0.5f, -0.5f, 0.5f, 0, 1, 0}, {0.5f, 0.5f, 0.5f, 0, 0, 1},
			{-0.5f, 0.5f, 0.5f, 1, 1, 0},
			{-0.5f, -0.5f, -0.5f, 1, 0, 1}, {0.5f, -0.5f, -0.5f, 0, 1, 1}, {0.5f, 0.5f, -0.5f, 1, 1, 1},
			{-0.5f, 0.5f, -0.5f, 0.5, 0.5, 0.5}
		};
		const uint16_t indices[] = {
			0, 1, 2, 2, 3, 0, 1, 5, 6, 6, 2, 1, 5, 4, 7, 7, 6, 5, 4, 0, 3, 3, 7, 4, 3, 2, 6, 6, 7, 3, 4, 5, 1, 1, 0, 4
		};
		indexCount_ = 36;

		vertexLayout_ = VertexLayout{};
		vertexLayout_.setBinding(0, sizeof(float) * 6, VK_VERTEX_INPUT_RATE_VERTEX);
		vertexLayout_.addAttribute(0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0);
		vertexLayout_.addAttribute(1, 0, VK_FORMAT_R32G32B32_SFLOAT, sizeof(float) * 3);

		vertexBuffer_ = std::make_unique<Buffer>();
		BufferCreateInfo vbi{};
		vbi.size = sizeof(verts);
		vbi.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		vbi.properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		vertexBuffer_->create(device, vbi);
		vertexBuffer_->upload(device, verts, sizeof(verts));

		indexBuffer_ = std::make_unique<Buffer>();
		BufferCreateInfo ibi{};
		ibi.size = sizeof(indices);
		ibi.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		ibi.properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		indexBuffer_->create(device, ibi);
		indexBuffer_->upload(device, indices, sizeof(indices));
	}

	void Mesh::cleanup(Device& device)
	{
		if (indexBuffer_)
		{
			indexBuffer_->cleanup(device);
			indexBuffer_.reset();
		}
		if (vertexBuffer_)
		{
			vertexBuffer_->cleanup(device);
			vertexBuffer_.reset();
		}
		indexCount_ = 0;
		vertexLayout_ = VertexLayout{};
	}

	void Mesh::bind(CommandContext& ctx) const
	{
		const VkBuffer vb = vertexBuffer_ ? vertexBuffer_->handle() : VK_NULL_HANDLE;
		constexpr VkDeviceSize ofs = 0;
		if (vb) ctx.bindVertexBuffers(0, &vb, &ofs, 1);
		if (indexBuffer_) ctx.bindIndexBuffer(indexBuffer_->handle(), 0, VK_INDEX_TYPE_UINT16);
	}
}
