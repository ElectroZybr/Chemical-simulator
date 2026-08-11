#include <memory>
#include <vector>

#include "Lattice/Engine/NeighborSearch/BarnesHut/Octree.h"
#include "Lattice/Engine/NeighborSearch/SpatialGrid.h"
#include "Lattice/Engine/Simulation.h"
#include "Lattice/Engine/physics/Atom/AtomSort.h"
#include "Lattice/Engine/physics/Atom/AtomStorage.h"
// #include "Lattice/Plugins/ClassicMD/ForceFields/CoulombForceField.h"
#include "Lattice/tests/TestSupport.h"

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

namespace {

    float bondDistance(const Lattice::Simulation& simulation, const Bond& bond) {
        const glm::vec3 a = simulation.atoms().pos(bond.aIndex);
        const glm::vec3 b = simulation.atoms().pos(bond.bIndex);
        return glm::length(a - b);
    }

    void testOctreeBuildChargeAndChildren() {
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
        atoms.reserve(count);
        for (size_t i = 0; i < count; ++i) {
            (void)atoms.addAtom(glm::vec3(x[i], y[i], z[i]), glm::vec3(vx[i], vy[i], vz[i]), types[i]);
            atoms.charge()[i] = charge[i];
        }

        SpatialGrid grid(glm::vec3(8.0f), 4.0f);
        grid.rebuild(atoms.x(), atoms.y(), atoms.z());

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

    // void testCoulombFarFieldApproximation() {
    //     const size_t count = 2;
    //     std::vector<float> x{0.0f, 100.0f};
    //     std::vector<float> y{0.0f, 0.0f};
    //     std::vector<float> z{0.0f, 0.0f};
    //     std::vector<float> vx(count, 0.0f);
    //     std::vector<float> vy(count, 0.0f);
    //     std::vector<float> vz(count, 0.0f);
    //     std::vector<AtomData::Type> types(count, AtomData::Type::H);
    //     std::vector<float> charge{1.0f, 1.0f};

    //     AtomStorage atoms;
    //     atoms.reserve(count);
    //     for (size_t i = 0; i < count; ++i) {
    //         (void)atoms.addAtom(glm::vec3(x[i], y[i], z[i]), glm::vec3(vx[i], vy[i], vz[i]), types[i]);
    //         atoms.charge()[i] = charge[i];
    //     }

    //     OctreeNode root;
    //     OctreeTestSupport::setSize(root, 1.0f);
    //     OctreeTestSupport::setCenter(root, glm::vec3(100.0f, 0.0f, 0.0f));
    //     OctreeTestSupport::setCharge(root, 1.0f);
    //     OctreeTestSupport::setDipole(root, glm::vec3(100.0f, 0.0f, 0.0f));
    //     OctreeTestSupport::setFirstAtom(root, 1);
    //     OctreeTestSupport::setAtomCount(root, 1);
    //     OctreeTestSupport::setChild(root, 0, std::make_unique<OctreeNode>(OctreeTestSupport::center(root)));
    //     OctreeNode* cluster = OctreeTestSupport::child(root, 0);
    //     OctreeTestSupport::setCharge(*cluster, 1.0f);
    //     OctreeTestSupport::setCenter(*cluster, OctreeTestSupport::center(root));
    //     OctreeTestSupport::setDipole(*cluster, OctreeTestSupport::dipole(root));
    //     OctreeTestSupport::setFirstAtom(*cluster, 1);
    //     OctreeTestSupport::setAtomCount(*cluster, 1);

    //     CoulombForceField field;
    //     float fx = 0.0f;
    //     float fy = 0.0f;
    //     float fz = 0.0f;
    //     float potentialEnergy = 0.0f;

    //     field.computeForce(atoms, 0, root, 0.7f, fx, fy, fz, potentialEnergy);

    //     const float d = 100.0f;
    //     const float d2 = d * d;
    //     const float invR = 1.0f / d;
    //     const float qqScale = CoulombForceField::kCoulombEvAngstrom * 1.0f;
    //     const float expectedForceX = -100.0f * qqScale * invR / d2;
    //     const float expectedPotential = 0.5f * qqScale * invR;

    //     expect(isClose(fx, expectedForceX, 1e-5f), "Far-field force should match monopole approximation in X direction");
    //     expect(isClose(fy, 0.0f, 1e-6f), "Far-field force should be zero in Y direction");
    //     expect(isClose(fz, 0.0f, 1e-6f), "Far-field force should be zero in Z direction");
    //     expect(isClose(potentialEnergy, expectedPotential, 1e-5f), "Far-field potential should match monopole approximation");
    // }

    void testWaterMoleculeBondsStayLocalAfterNeighborSort() {
        Lattice::Simulation simulation;
        simulation.createWorld(glm::vec3(40.0f, 40.0f, 40.0f));

        const std::filesystem::path waterPath = std::filesystem::path("Mods") / "Base" / "Molecules" / "h2o.mol";
        expect(simulation.loadMoleculeTemplate("h2o", waterPath), "Water template should load");

        Lattice::SpawnOptions options;
        options.min = glm::vec3(0.0f);
        options.max = glm::vec3(40.0f, 40.0f, 40.0f);
        options.margin = 6.0f;
        options.minDistance = 4.0f;
        options.maxAttempts = 128;
        options.randomRotation = true;

        const int moleculeCount = 20;
        for (int i = 0; i < moleculeCount; ++i) {
            expect(simulation.randomSpawn("h2o", options), "Each spawned water molecule should succeed");
        }

        expect(std::ranges::distance(simulation.bonds()) == moleculeCount * 2, "Each water molecule should contribute exactly two bonds");

        for (const Bond& bond : simulation.bonds()) {
            expect(bondDistance(simulation, bond) < 2.0f, "Freshly spawned water bond should stay short");
        }

        simulation.neighborList().rebuildPipeline(simulation.atoms(), simulation.world(), 0);

        for (const Bond& bond : simulation.bonds()) {
            expect(bondDistance(simulation, bond) < 2.0f, "Water bond should stay short after atom sorting/remap");
        }
    }

    void testNeighborListRebuildsWhenOnlyFixedAtomsAreAdded() {
        Lattice::Simulation simulation;
        simulation.createWorld(glm::vec3(20.0f, 20.0f, 20.0f));

        (void)simulation.appendAtomFast(glm::vec3(5.0f, 5.0f, 5.0f), glm::vec3(0.0f), AtomData::Type::H, false);
        (void)simulation.appendAtomFast(glm::vec3(8.0f, 5.0f, 5.0f), glm::vec3(0.0f), AtomData::Type::H, false);
        simulation.finishAtomBatch();
        simulation.neighborList().rebuildPipeline(simulation.atoms(), simulation.world(), 0);

        expect(!simulation.neighborList().needsRebuild(simulation.atoms()), "Freshly rebuilt neighbor list should be valid");

        (void)simulation.appendAtomFast(glm::vec3(6.0f, 5.0f, 5.0f), glm::vec3(0.0f), AtomData::Type::H, true);

        expect(simulation.neighborList().needsRebuild(simulation.atoms()), "Adding fixed atoms should invalidate neighbor list by atom count");
    }
}

void runNeighborSearchTests() {
    testOctreeBuildChargeAndChildren();
    // testCoulombFarFieldApproximation();
    testWaterMoleculeBondsStayLocalAfterNeighborSort();
    testNeighborListRebuildsWhenOnlyFixedAtomsAreAdded();
}
