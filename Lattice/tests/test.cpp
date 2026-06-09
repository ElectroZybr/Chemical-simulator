#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

#include <glm/vec3.hpp>

#include "Engine/NeighborSearch/BarnesHut/Octree.h"
#include "Engine/NeighborSearch/SpatialGrid.h"
#include "Engine/physics/Atom/AtomSort.h"
#include "Engine/physics/Atom/AtomStorage.h"
#include "Engine/physics/ForceFields/CoulombForceField.h"

struct OctreeTestSupport {
    static float charge(const OctreeNode& node) { return node.charge; }
    static void setCharge(OctreeNode& node, float value) { node.charge = value; }

    static const glm::vec3& center(const OctreeNode& node) { return node.center; }
    static void setCenter(OctreeNode& node, const glm::vec3& value) { node.center = value; }

    static const glm::vec3& dipole(const OctreeNode& node) { return node.dipoleMoment; }
    static void setDipole(OctreeNode& node, const glm::vec3& value) { node.dipoleMoment = value; }

    static size_t atomCount(const OctreeNode& node) { return node.atomCount; }
    static void setAtomCount(OctreeNode& node, size_t count) { node.atomCount = count; }

    static float size(const OctreeNode& node) { return node.size; }
    static void setSize(OctreeNode& node, float value) { node.size = value; }

    static size_t firstAtom(const OctreeNode& node) { return node.firstAtom; }
    static void setFirstAtom(OctreeNode& node, size_t value) { node.firstAtom = value; }

    static const OctreeNode* child(const OctreeNode& node, int index) { return node.children[index].get(); }
    static OctreeNode* child(OctreeNode& node, int index) { return node.children[index].get(); }
    static void setChild(OctreeNode& node, int index, std::unique_ptr<OctreeNode> childNode) { node.children[index] = std::move(childNode); }
};

static bool isClose(float a, float b, float eps = 1e-4f) {
    return std::fabs(a - b) <= eps;
}

static bool isClose(const glm::vec3& a, const glm::vec3& b, float eps = 1e-4f) {
    return isClose(a.x, b.x, eps) && isClose(a.y, b.y, eps) && isClose(a.z, b.z, eps);
}

static void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "TEST FAILED: " << message << std::endl;
        std::terminate();
    }
}

static void testOctreeBuildChargeAndChildren() {
    const size_t count = 3;
    std::vector<float> x{1.0f, 5.0f, 1.0f};
    std::vector<float> y{1.0f, 1.0f, 5.0f};
    std::vector<float> z{1.0f, 1.0f, 1.0f};
    std::vector<float> vx(count, 0.0f);
    std::vector<float> vy(count, 0.0f);
    std::vector<float> vz(count, 0.0f);
    std::vector<AtomData::Type> types(count, AtomData::Type::H);
    std::vector<float> charge{1.0f, -2.0f, 3.0f};

    AtomStorage atoms;
    atoms.init(count, count, x, y, z, vx, vy, vz, types, charge);

    SpatialGrid grid(glm::vec3(8.0f), 4.0f);
    grid.rebuild(atoms.xDataSpan(), atoms.yDataSpan(), atoms.zDataSpan());

    AtomSort sorter;
    sorter.mortonOrder(atoms, grid);

    OctreeNode root;
    root.build(atoms, grid);

    expect(isClose(OctreeTestSupport::charge(root), 2.0f), "Octree root charge should equal total atom charge");
    expect(OctreeTestSupport::atomCount(root) == count, "Octree root atom count should equal number of atoms");

    float childChargeSum = 0.0f;
    int nonEmptyChildCount = 0;
    for (int i = 0; i < 8; ++i) {
        const OctreeNode* child = OctreeTestSupport::child(root, i);
        if (child != nullptr) {
            ++nonEmptyChildCount;
            childChargeSum += OctreeTestSupport::charge(*child);
            expect(OctreeTestSupport::atomCount(*child) >= 1, "Non-empty child node must contain at least one atom");
        }
    }

    expect(nonEmptyChildCount == 1, "Only one top-level octant should contain all atoms for this configuration");
    expect(isClose(childChargeSum, 2.0f), "Sum of child charges should equal root charge");

    const OctreeNode* primaryChild = OctreeTestSupport::child(root, 0);
    expect(primaryChild != nullptr, "Primary top-level child must exist");

    float grandchildChargeSum = 0.0f;
    int nonEmptyGrandchildCount = 0;
    for (int i = 0; i < 8; ++i) {
        const OctreeNode* grandchild = OctreeTestSupport::child(*primaryChild, i);
        if (grandchild != nullptr) {
            ++nonEmptyGrandchildCount;
            grandchildChargeSum += OctreeTestSupport::charge(*grandchild);
            expect(OctreeTestSupport::atomCount(*grandchild) >= 1, "Non-empty grandchild node must contain at least one atom");
        }
    }

    expect(nonEmptyGrandchildCount == 3, "Three second-level octants should contain atoms for this configuration");
    expect(isClose(grandchildChargeSum, 2.0f), "Sum of grandchild charges should equal root charge");
}

static void testCoulombFarFieldApproximation() {
    const size_t count = 2;
    std::vector<float> x{0.0f, 100.0f};
    std::vector<float> y{0.0f, 0.0f};
    std::vector<float> z{0.0f, 0.0f};
    std::vector<float> vx(count, 0.0f);
    std::vector<float> vy(count, 0.0f);
    std::vector<float> vz(count, 0.0f);
    std::vector<AtomData::Type> types(count, AtomData::Type::H);
    std::vector<float> charge{1.0f, 1.0f};

    AtomStorage atoms;
    atoms.init(count, count, x, y, z, vx, vy, vz, types, charge);

    OctreeNode root;
    OctreeTestSupport::setSize(root, 1.0f);
    OctreeTestSupport::setCenter(root, glm::vec3(100.0f, 0.0f, 0.0f));
    OctreeTestSupport::setCharge(root, 1.0f);
    OctreeTestSupport::setDipole(root, glm::vec3(100.0f, 0.0f, 0.0f));
    OctreeTestSupport::setFirstAtom(root, 1);
    OctreeTestSupport::setAtomCount(root, 1);
    OctreeTestSupport::setChild(root, 0, std::make_unique<OctreeNode>(OctreeTestSupport::center(root)));
    OctreeNode* cluster = OctreeTestSupport::child(root, 0);
    OctreeTestSupport::setCharge(*cluster, 1.0f);
    OctreeTestSupport::setCenter(*cluster, OctreeTestSupport::center(root));
    OctreeTestSupport::setDipole(*cluster, OctreeTestSupport::dipole(root));
    OctreeTestSupport::setFirstAtom(*cluster, 1);
    OctreeTestSupport::setAtomCount(*cluster, 1);

    CoulombForceField field;
    float fx = 0.0f;
    float fy = 0.0f;
    float fz = 0.0f;
    float potentialEnergy = 0.0f;

    field.computeForce(atoms, 0, root, 0.7f, fx, fy, fz, potentialEnergy);

    const float d = 100.0f;
    const float d2 = d * d;
    const float invR = 1.0f / d;
    const float qqScale = CoulombForceField::kCoulombEvAngstrom * 1.0f * 1.0f;
    const float expectedForceX = -100.0f * qqScale * invR / d2;
    const float expectedPotential = 0.5f * qqScale * invR;

    expect(isClose(fx, expectedForceX, 1e-5f), "Far-field force should match monopole approximation in X direction");
    expect(isClose(fy, 0.0f, 1e-6f), "Far-field force should be zero in Y direction");
    expect(isClose(fz, 0.0f, 1e-6f), "Far-field force should be zero in Z direction");
    expect(isClose(potentialEnergy, expectedPotential, 1e-5f), "Far-field potential should match monopole approximation");
}

int main() {
    testOctreeBuildChargeAndChildren();
    testCoulombFarFieldApproximation();
    std::cout << "All Lattice octree/Coulomb tests passed." << std::endl;
    return 0;
}
