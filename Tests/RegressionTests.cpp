#include <cmath>
#include <cstdio>
#include <exception>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string_view>

#include "Engine/Simulation.h"
#include "Engine/io/SimulationStateIO.h"
#include "Engine/physics/ForceFields/CoulombForceField.h"

// Исправление бага: этот набор тестов прямо называет исправленные баги, чтобы
// будущие правки сохраняли проверки dt, NeighborList, загрузки charge и устойчивости.

namespace {
constexpr float kForceEpsilon = 1.0e-6f;

[[noreturn]] void fail(std::string_view message) {
    throw std::runtime_error(std::string(message));
}

void require(bool condition, std::string_view message) {
    if (!condition) {
        fail(message);
    }
}

float forceMagnitude(const AtomStorage& atoms, size_t atomIndex) {
    const float fx = atoms.forceX(atomIndex);
    const float fy = atoms.forceY(atomIndex);
    const float fz = atoms.forceZ(atomIndex);
    return std::sqrt(fx * fx + fy * fy + fz * fz);
}

bool atomStateIsFinite(const AtomStorage& atoms, size_t atomIndex) {
    return std::isfinite(atoms.forceX(atomIndex)) && std::isfinite(atoms.forceY(atomIndex)) &&
           std::isfinite(atoms.forceZ(atomIndex)) && std::isfinite(atoms.energy(atomIndex));
}

void rebuildNeighborList(Simulation& simulation) {
    simulation.neighborList().rebuildPipeline(simulation.atoms(), simulation.world(), static_cast<int>(simulation.getSimStep()));
}

void testDtSanitizesInvalidValues() {
    Simulation simulation;
    simulation.createWorld({10.0f, 10.0f, 10.0f});

    simulation.setDt(-1.0f);
    require(std::isfinite(simulation.getDt()), "negative dt must sanitize to a finite value");
    require(simulation.getDt() > 0.0f, "negative dt must sanitize to a positive value");

    simulation.setDt(std::numeric_limits<float>::quiet_NaN());
    require(std::isfinite(simulation.getDt()), "NaN dt must sanitize to a finite value");
    require(simulation.getDt() > 0.0f, "NaN dt must sanitize to a positive value");
}

void testUnsupportedIntegratorsCanonicalizeToVerlet() {
    Simulation simulation;
    simulation.createWorld({10.0f, 10.0f, 10.0f});

    simulation.setIntegrator(Integrator::Scheme::RK4);
    require(simulation.getIntegrator() == Integrator::Scheme::Verlet, "RK4 must canonicalize to Verlet until implemented");

    simulation.setIntegrator(Integrator::Scheme::Langevin);
    require(simulation.getIntegrator() == Integrator::Scheme::Verlet, "Langevin must canonicalize to Verlet until implemented");

    simulation.setIntegrator(Integrator::Scheme::KDK);
    require(simulation.getIntegrator() == Integrator::Scheme::KDK, "implemented KDK scheme must remain selectable");
}

void testSkinDoesNotChangePhysicalCutoff() {
    Simulation simulation;
    simulation.createWorld({20.0f, 20.0f, 20.0f});
    simulation.setLJEnabled(true);
    simulation.setCoulombEnabled(false);
    simulation.setNeighborListCutoff(1.0f);
    simulation.setNeighborListSkin(2.0f);

    simulation.createAtom({5.0f, 5.0f, 5.0f}, {0.0f, 0.0f, 0.0f}, AtomData::Type::H);
    simulation.createAtom({6.5f, 5.0f, 5.0f}, {0.0f, 0.0f, 0.0f}, AtomData::Type::H);
    rebuildNeighborList(simulation);

    require(simulation.neighborList().pairStorageSize() > 0, "pair inside skin must be present in NeighborList");
    simulation.forceField().computePairInteractions(simulation.world());

    require(forceMagnitude(simulation.atoms(), 0) <= kForceEpsilon, "pair outside physical cutoff must not force atom 0");
    require(forceMagnitude(simulation.atoms(), 1) <= kForceEpsilon, "pair outside physical cutoff must not force atom 1");
}

void testMobileFixedPairContributesToNonBondedForces() {
    Simulation simulation;
    simulation.createWorld({20.0f, 20.0f, 20.0f});
    simulation.setLJEnabled(true);
    simulation.setCoulombEnabled(false);
    simulation.setNeighborListCutoff(3.0f);
    simulation.setNeighborListSkin(0.1f);

    simulation.createAtom({5.0f, 5.0f, 5.0f}, {0.0f, 0.0f, 0.0f}, AtomData::Type::H, false);
    simulation.createAtom({6.5f, 5.0f, 5.0f}, {0.0f, 0.0f, 0.0f}, AtomData::Type::H, true);
    rebuildNeighborList(simulation);

    simulation.forceField().computePairInteractions(simulation.world());
    require(forceMagnitude(simulation.atoms(), 0) > kForceEpsilon, "mobile-fixed pair inside cutoff must force mobile atom");
}

void testCoulombOnlyComputeUsesNeighborList() {
    Simulation simulation;
    simulation.createWorld({20.0f, 20.0f, 20.0f});
    simulation.setNeighborListCutoff(3.0f);
    simulation.setNeighborListSkin(0.1f);
    simulation.createAtom({5.0f, 5.0f, 5.0f}, {0.0f, 0.0f, 0.0f}, AtomData::Type::H);
    simulation.createAtom({6.5f, 5.0f, 5.0f}, {0.0f, 0.0f, 0.0f}, AtomData::Type::H);
    simulation.atoms().charge(0) = 1.0f;
    simulation.atoms().charge(1) = -1.0f;
    rebuildNeighborList(simulation);

    CoulombForceField coulomb;
    coulomb.compute(simulation.atoms(), simulation.neighborList());

    require(forceMagnitude(simulation.atoms(), 0) > kForceEpsilon, "Coulomb-only compute must force charged atom 0");
    require(forceMagnitude(simulation.atoms(), 1) > kForceEpsilon, "Coulomb-only compute must force charged atom 1");
}

void testAtomDataCarriesNaClDefaultCharges() {
    require(AtomData::getProps(AtomData::Type::Na).defaultCharge > 0.0f, "Na default charge must be positive in AtomData");
    require(AtomData::getProps(AtomData::Type::Cl).defaultCharge < 0.0f, "Cl default charge must be negative in AtomData");
}

void testCellSizeCoversNeighborListRadius() {
    Simulation simulation;
    simulation.createWorld({40.0f, 40.0f, 40.0f});
    simulation.setNeighborListCutoff(5.0f);
    simulation.setNeighborListSkin(1.0f);
    simulation.setSizeBox({40.0f, 40.0f, 40.0f}, 1);

    require(simulation.world().getGridCellSize() >= simulation.getNeighborListRadius(),
            "grid cell size must cover cutoff + skin for 27-cell traversal");
}

void testFastAtomRefreshesNeighborListBeforeForces() {
    Simulation simulation;
    simulation.createWorld({20.0f, 20.0f, 20.0f});
    simulation.setLJEnabled(true);
    simulation.setCoulombEnabled(false);
    simulation.setNeighborListCutoff(1.0f);
    simulation.setNeighborListSkin(0.1f);
    simulation.setDt(0.01f);

    simulation.createAtom({4.0f, 5.0f, 5.0f}, {110.0f, 0.0f, 0.0f}, AtomData::Type::H);
    simulation.createAtom({6.0f, 5.0f, 5.0f}, {0.0f, 0.0f, 0.0f}, AtomData::Type::H);

    simulation.update();

    require(simulation.neighborList().pairStorageSize() > 0,
            "fast atom crossing into cutoff must refresh NeighborList in the same step");
    require(forceMagnitude(simulation.atoms(), 0) > kForceEpsilon,
            "fast atom crossing into cutoff must receive non-bonded force in the same step");
}

void testTextLoadMissingChargeUsesAtomDefault() {
    const char* path = "lat_missing_charge_regression.lat";
    std::remove(path);

    {
        std::ofstream file(path, std::ios::trunc);
        file << "[meta]\n";
        file << "  format lat\n";
        file << "  version 1\n\n";
        file << "[scene]\n";
        file << "  box 10 10 10\n";
        file << "  dt 0.01\n\n";
        file << "[atoms]\n";
        file << "  count 2\n";
        file << "  atom 1 1 1 0 0 0 " << static_cast<int>(AtomData::Type::Na) << " 0\n";
        file << "  atom 2 1 1 0 0 0 " << static_cast<int>(AtomData::Type::Cl) << " 0 0\n";
    }

    Simulation simulation;
    simulation.createWorld({10.0f, 10.0f, 10.0f});
    SimulationStateIO::load(simulation, path);
    std::remove(path);

    require(simulation.atoms().size() == 2, "text load must create both atoms");
    require(simulation.atoms().charge(0) == AtomData::getProps(AtomData::Type::Na).defaultCharge,
            "missing text charge must preserve atom default charge");
    require(simulation.atoms().charge(1) == 0.0f, "explicit zero text charge must remain zero");
}

void testNearOverlapForcesRemainFinite() {
    Simulation simulation;
    simulation.createWorld({10.0f, 10.0f, 10.0f});
    simulation.setLJEnabled(true);
    simulation.setCoulombEnabled(true);
    simulation.setNeighborListCutoff(3.0f);
    simulation.setNeighborListSkin(0.1f);
    simulation.createAtom({5.0f, 5.0f, 5.0f}, {0.0f, 0.0f, 0.0f}, AtomData::Type::H);
    simulation.createAtom({5.0011f, 5.0f, 5.0f}, {0.0f, 0.0f, 0.0f}, AtomData::Type::H);
    simulation.atoms().charge(0) = 1.0f;
    simulation.atoms().charge(1) = -1.0f;
    rebuildNeighborList(simulation);

    simulation.forceField().computePairInteractions(simulation.world());

    require(atomStateIsFinite(simulation.atoms(), 0), "near-overlap atom 0 force and energy must remain finite");
    require(atomStateIsFinite(simulation.atoms(), 1), "near-overlap atom 1 force and energy must remain finite");
}

void testPairForceRefreshesAfterExternalPositionMutation() {
    Simulation simulation;
    simulation.createWorld({20.0f, 20.0f, 20.0f});
    simulation.setLJEnabled(true);
    simulation.setCoulombEnabled(false);
    simulation.setNeighborListCutoff(2.0f);
    simulation.setNeighborListSkin(0.1f);
    simulation.createAtom({5.0f, 5.0f, 5.0f}, {0.0f, 0.0f, 0.0f}, AtomData::Type::H);
    simulation.createAtom({10.0f, 5.0f, 5.0f}, {0.0f, 0.0f, 0.0f}, AtomData::Type::H);
    rebuildNeighborList(simulation);

    simulation.atoms().setPos(1, {6.0f, 5.0f, 5.0f});
    simulation.forceField().computePairInteractions(simulation.world());

    require(forceMagnitude(simulation.atoms(), 0) > kForceEpsilon,
            "pair force path must refresh NeighborList after external position mutation");
}

using TestFn = void (*)();

struct TestCase {
    std::string_view name;
    TestFn fn;
};

constexpr TestCase kTests[] = {
    {"DtSanitizesInvalidValues", testDtSanitizesInvalidValues},
    {"UnsupportedIntegratorsCanonicalizeToVerlet", testUnsupportedIntegratorsCanonicalizeToVerlet},
    {"SkinDoesNotChangePhysicalCutoff", testSkinDoesNotChangePhysicalCutoff},
    {"MobileFixedPairContributesToNonBondedForces", testMobileFixedPairContributesToNonBondedForces},
    {"CoulombOnlyComputeUsesNeighborList", testCoulombOnlyComputeUsesNeighborList},
    {"AtomDataCarriesNaClDefaultCharges", testAtomDataCarriesNaClDefaultCharges},
    {"CellSizeCoversNeighborListRadius", testCellSizeCoversNeighborListRadius},
    {"FastAtomRefreshesNeighborListBeforeForces", testFastAtomRefreshesNeighborListBeforeForces},
    {"TextLoadMissingChargeUsesAtomDefault", testTextLoadMissingChargeUsesAtomDefault},
    {"NearOverlapForcesRemainFinite", testNearOverlapForcesRemainFinite},
    {"PairForceRefreshesAfterExternalPositionMutation", testPairForceRefreshesAfterExternalPositionMutation},
};
} // namespace

int main() {
    int failures = 0;
    for (const TestCase& test : kTests) {
        try {
            test.fn();
            std::cout << "[PASS] " << test.name << '\n';
        }
        catch (const std::exception& ex) {
            ++failures;
            std::cerr << "[FAIL] " << test.name << ": " << ex.what() << '\n';
        }
    }

    if (failures > 0) {
        std::cerr << failures << " regression test(s) failed\n";
        return 1;
    }

    std::cout << "All regression tests passed\n";
    return 0;
}
