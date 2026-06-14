#include "Rendering/Renderer.h"

#include <vector>

#include "Rendering/backend/WGPUContext.h"

void RendererWGPU::initBondBuffer() {
    lineLayer_.bondVb = WGPUContext::instance().createBuffer(128, wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst, "Bond_Geometry");
    lineLayer_.bondVbCapacity = 128;
}

void RendererWGPU::drawBondsImpl(const RenderAtomsView& atoms, const RenderBondsView& bonds) {
    if (bonds.empty()) {
        return;
    }

    struct BondVertexBuildContext {
        const RenderAtomsView* atoms = nullptr;
        std::vector<glm::vec3>* verts = nullptr;

        static void append(size_t aIndex, size_t bIndex, void* userData) {
            auto& ctx = *static_cast<BondVertexBuildContext*>(userData);
            const RenderAtomsView& atoms = *ctx.atoms;
            if (aIndex >= atoms.count || bIndex >= atoms.count || !atoms.hasPositions()) {
                return;
            }

            ctx.verts->emplace_back(atoms.x[aIndex], atoms.y[aIndex], atoms.z[aIndex]);
            ctx.verts->emplace_back(atoms.x[bIndex], atoms.y[bIndex], atoms.z[bIndex]);
        }
    };

    std::vector<glm::vec3> verts;
    verts.reserve(bonds.count * 2);
    BondVertexBuildContext buildContext{.atoms = &atoms, .verts = &verts};
    bonds.forEach(BondVertexBuildContext::append, &buildContext);
    if (verts.empty()) {
        return;
    }

    const uint64_t bytes = verts.size() * sizeof(glm::vec3);
    if (bytes > lineLayer_.bondVbCapacity) {
        lineLayer_.bondVb = WGPUContext::instance().createBuffer(bytes * 2, wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst, "Bond_Geometry");
        lineLayer_.bondVbCapacity = bytes * 2;
    }
    WGPUContext::instance().queue()->writeBuffer(*lineLayer_.bondVb, 0, verts.data(), bytes);

    currentPass->setPipeline(*pipelines_.bond);
    currentPass->setBindGroup(0, *lineLayer_.bindGroups[lineUniformSlotIndex_ - 1], 0, nullptr);
    currentPass->setVertexBuffer(0, *lineLayer_.bondVb, 0, bytes);
    currentPass->draw(verts.size(), 1, 0, 0);
}
