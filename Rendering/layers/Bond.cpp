#include "Rendering/Renderer.h"

#include <vector>

#include "Rendering/backend/WGPUContext.h"
#include "Renderer.h"

namespace {
void appendLine(std::vector<glm::vec3>& verts, glm::vec3 a, glm::vec3 b) {
    verts.push_back(a);
    verts.push_back(b);
}
}

void RendererWGPU::initBondBuffer() {
    lineLayer_.bondVb = WGPUContext::instance().createBuffer(128, wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst, "Bond_Geometry");
    lineLayer_.bondVbCapacity = 128;
    lineLayer_.phantomVb = WGPUContext::instance().createBuffer(128, wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst, "Phantom_Geometry");
    lineLayer_.phantomVbCapacity = 128;
}

void RendererWGPU::drawBondsImpl(const View::RenderAtomsView& atoms, const View::RenderBondsView& bonds, const glm::mat4& viewMatrix) {
    if (bonds.empty()) {
        return;
    }

    struct BondVertexBuildContext {
        const View::RenderAtomsView* atoms = nullptr;
        std::vector<glm::vec3>* verts = nullptr;
        glm::vec3 view{};

        static void append(const View::RenderBond& bond, void* userData) {
            auto& ctx = *static_cast<BondVertexBuildContext*>(userData);
            const View::RenderAtomsView& atoms = *ctx.atoms;
            if (bond.aIndex >= atoms.count || bond.bIndex >= atoms.count || !atoms.hasPositions()) {
                return;
            }

            glm::vec3 a(atoms.x[bond.aIndex], atoms.y[bond.aIndex], atoms.z[bond.aIndex]);
            glm::vec3 b(atoms.x[bond.bIndex], atoms.y[bond.bIndex], atoms.z[bond.bIndex]);

            // нормаль (связи всегда напрвлены к камере)
            glm::vec3 dir = glm::normalize(b - a);
            glm::vec3 normal = glm::normalize(glm::cross(dir, ctx.view));

            static constexpr float offset = 0.08f;
            switch (bond.order) {
                case 1:
                    appendLine(*ctx.verts, a, b);
                    break;
                case 2:
                    appendLine(*ctx.verts, a + normal * offset, b + normal * offset);
                    appendLine(*ctx.verts, a - normal * offset, b - normal * offset);
                    break;
                case 3:
                    appendLine(*ctx.verts, a, b);
                    appendLine(*ctx.verts,a + normal * offset, b + normal * offset);
                    appendLine(*ctx.verts,a - normal * offset, b - normal * offset);
                    break;
            }
        }
    };

    std::vector<glm::vec3> verts;
    verts.reserve(bonds.count * 2);
    BondVertexBuildContext buildContext{.atoms = &atoms, .verts = &verts, .view = -glm::vec3(viewMatrix[2])};
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

void RendererWGPU::drawPhantomImpl(const std::vector<glm::vec3>& lines, bool dashed) {
    if (lines.empty()) {
        return;
    }

    constexpr float kDashLength = 2.0f;
    constexpr float kGapLength = 1.25f;

    std::vector<glm::vec3> gpuLines;
    if (dashed) {
        gpuLines.reserve(lines.size() * 2);

        for (size_t i = 0; i + 1 < lines.size(); i += 2) {
            const glm::vec3 start = lines[i];
            const glm::vec3 end = lines[i + 1];
            const glm::vec3 delta = end - start;
            const float length = glm::length(delta);
            if (length <= 1e-5f) {
                continue;
            }

            const glm::vec3 direction = delta / length;
            float offset = 0.0f;
            while (offset < length) {
                const float dashStart = offset;
                const float dashEnd = std::min(offset + kDashLength, length);
                gpuLines.push_back(start + direction * dashStart);
                gpuLines.push_back(start + direction * dashEnd);
                offset += kDashLength + kGapLength;
            }
        }
    } else {
        gpuLines = lines;
    }

    if (gpuLines.empty()) {
        return;
    }

    const uint64_t bytes = gpuLines.size() * sizeof(glm::vec3);
    if (bytes > lineLayer_.phantomVbCapacity) {
        lineLayer_.phantomVb = WGPUContext::instance().createBuffer(bytes * 2, wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst,
                                                                    "Phantom_Geometry");
        lineLayer_.phantomVbCapacity = bytes * 2;
    }
    WGPUContext::instance().queue()->writeBuffer(*lineLayer_.phantomVb, 0, gpuLines.data(), bytes);

    currentPass->setPipeline(*pipelines_.bond);
    currentPass->setBindGroup(0, *lineLayer_.bindGroups[lineUniformSlotIndex_ - 1], 0, nullptr);
    currentPass->setVertexBuffer(0, *lineLayer_.phantomVb, 0, bytes);
    currentPass->draw(gpuLines.size(), 1, 0, 0);
}