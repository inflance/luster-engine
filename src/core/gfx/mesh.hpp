#pragma once

#include "core/core.hpp"
#include "core/gfx/vertex_layout.hpp"
#include <memory>

namespace luster::gfx
{
    class Device;
    class Buffer;
    class CommandContext;

    class Mesh
    {
    public:
        Mesh() = default;
        ~Mesh() = default;

        void createCube(Device& device);
        void cleanup(Device& device);

        void bind(CommandContext& ctx) const;
        uint32_t indexCount() const { return indexCount_; }
        const VertexLayout* vertexLayout() const { return &vertexLayout_; }

    private:
        std::unique_ptr<Buffer> vertexBuffer_;
        std::unique_ptr<Buffer> indexBuffer_;
        VertexLayout vertexLayout_{};
        uint32_t indexCount_ = 0;
    };
}



