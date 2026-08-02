#include <array>
#include <filesystem>

#include "Lattice/Engine/Simulation.h"
#include "Lattice/tests/TestSupport.h"

namespace {
    float bondDistance(const Lattice::Simulation& simulation, const Bond& bond) {
        const glm::vec3 a = simulation.atoms().pos(bond.aIndex);
        const glm::vec3 b = simulation.atoms().pos(bond.bIndex);
        return glm::length(a - b);
    }

    void testSpawnWaterMoleculeCreatesLocalAtoms() {
        Lattice::Simulation simulation;
        simulation.createWorld(glm::vec3(20.0f, 20.0f, 20.0f));

        const std::filesystem::path waterPath = std::filesystem::path("Mods") / "Base" / "Molecules" / "h2o.mol";
        expect(simulation.loadMoleculeTemplate("h2o", waterPath), "Water template should load");
        expect(simulation.spawnMolecule("h2o", glm::vec3(10.0f, 10.0f, 10.0f), glm::mat3(1.0f), false), "Direct water spawn should succeed");
        expect(std::ranges::distance(simulation.bonds()) == 2, "Direct water spawn should create two bonds");

        for (const Bond& bond : simulation.bonds()) {
            expect(bondDistance(simulation, bond) < 2.0f, "Directly spawned water bond should stay short");
        }
    }

    void testSpawnNitrogenMoleculeCreatesStableBond() {
        Lattice::Simulation simulation;
        simulation.createWorld(glm::vec3(20.0f, 20.0f, 20.0f));

        const std::filesystem::path nitrogenPath = std::filesystem::path("Mods") / "Base" / "Molecules" / "n2.mol";
        expect(simulation.loadMoleculeTemplate("n2", nitrogenPath), "Nitrogen template should load");
        expect(simulation.spawnMolecule("n2", glm::vec3(10.0f, 10.0f, 10.0f), glm::mat3(1.0f), false), "Direct nitrogen spawn should succeed");
        expect(std::ranges::distance(simulation.bonds()) == 1, "Direct nitrogen spawn should create one bond");

        for (const Bond& bond : simulation.bonds()) {
            expect(bondDistance(simulation, bond) < 1.5f, "Directly spawned nitrogen bond should stay short");
        }
    }

    void testSpawnAdditionalDiatomicMoleculesCreateStableBond() {
        constexpr std::array<std::string_view, 7> molecules = {"cl2", "f2", "br2", "hf", "hcl", "co", "no"};

        for (std::string_view moleculeName : molecules) {
            Lattice::Simulation simulation;
            simulation.createWorld(glm::vec3(20.0f, 20.0f, 20.0f));

            const std::filesystem::path moleculePath = std::filesystem::path("Mods") / "Base" / "Molecules" / (std::string(moleculeName) + ".mol");
            expect(simulation.loadMoleculeTemplate(std::string(moleculeName), moleculePath), "Molecule template should load");
            expect(simulation.spawnMolecule(std::string(moleculeName), glm::vec3(10.0f, 10.0f, 10.0f), glm::mat3(1.0f), false),
                   "Direct diatomic spawn should succeed");
            expect(std::ranges::distance(simulation.bonds()) == 1, "Direct diatomic spawn should create one bond");

            for (const Bond& bond : simulation.bonds()) {
                expect(bondDistance(simulation, bond) < 3.0f, "Directly spawned diatomic bond should stay short");
            }
        }
    }

    void testCheckedMoleculeSpawnRejectsBlockedPoint() {
        Lattice::Simulation simulation;
        simulation.createWorld(glm::vec3(20.0f, 20.0f, 20.0f));

        const std::filesystem::path waterPath = std::filesystem::path("Mods") / "Base" / "Molecules" / "h2o.mol";
        expect(simulation.loadMoleculeTemplate("h2o", waterPath), "Water template should load");

        simulation.createAtom(glm::vec3(10.0f, 10.0f, 10.0f), glm::vec3(0.0f), AtomData::Type::O, false);
        const size_t atomCountBefore = simulation.atoms().size();

        expect(!simulation.canSpawnMolecule("h2o", glm::vec3(10.0f, 10.0f, 10.0f), glm::mat3(1.0f)),
               "Water molecule should not fit into an occupied spawn point");
        expect(!simulation.spawnMoleculeChecked("h2o", glm::vec3(10.0f, 10.0f, 10.0f), glm::mat3(1.0f), false),
               "Checked water spawn should fail for an occupied point");
        expect(simulation.atoms().size() == atomCountBefore, "Failed checked spawn should not add atoms");
    }

    void testFixedAtomsParticipateInPairPhysics() {
        Lattice::Simulation simulation;
        simulation.createWorld(glm::vec3(20.0f, 20.0f, 20.0f));
        simulation.setBondFormationEnabled(false);
        simulation.setCoulombEnabled(false);
        simulation.setLJEnabled(true);

        simulation.createAtom(glm::vec3(10.0f, 10.0f, 10.0f), glm::vec3(0.0f), AtomData::Type::Ar, false);
        simulation.createAtom(glm::vec3(11.0f, 10.0f, 10.0f), glm::vec3(0.0f), AtomData::Type::Ar, true);

        World& world = simulation.world();
        world.getNeighborList().rebuildPipeline(world.getAtomStorage(), world, 0);
        std::fill(simulation.atoms().fx().begin(), simulation.atoms().fx().end(), 0.0f);
        std::fill(simulation.atoms().fy().begin(), simulation.atoms().fy().end(), 0.0f);
        std::fill(simulation.atoms().fz().begin(), simulation.atoms().fz().end(), 0.0f);
        std::fill(simulation.atoms().energy().begin(), simulation.atoms().energy().end(), 0.0f);

        (void)simulation.forceField().compute(world, false, simulation.getDt());
        expect(std::fabs(simulation.atoms().fx()[1]) > 1e-4f, "Fixed atom should receive non-zero pair force");
        expect(isClose(simulation.atoms().fx()[0], -simulation.atoms().fx()[1], 1e-4f), "Pair force should satisfy Newton's third law");

        const glm::vec3 fixedPosBefore = simulation.atoms().pos(1);
        simulation.update();
        expect(isClose(simulation.atoms().pos(1), fixedPosBefore, 1e-6f), "Fixed atom should stay in place after update");
    }
}

void runPhysicsTests() {
    testSpawnWaterMoleculeCreatesLocalAtoms();
    testSpawnNitrogenMoleculeCreatesStableBond();
    testSpawnAdditionalDiatomicMoleculesCreateStableBond();
    testCheckedMoleculeSpawnRejectsBlockedPoint();
    testFixedAtomsParticipateInPairPhysics();
}
