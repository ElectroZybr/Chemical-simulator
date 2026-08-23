#include <iomanip>
#include <iostream>
#include <sstream>
#include <filesystem>
#include <string>
#include <vector>

#include <CLI/CLI.hpp>

#include <Lattice/Tools/LogStyle.hpp>
#include "Lattice/Tools/SystemInfo.hpp"
#include "Lattice/Engine/pluginLoader.hpp"
#include "Engine/Simulation.h"
#include "Engine/physics/Atom/AtomData.h"

using namespace Lattice;

namespace {
    const std::filesystem::path pluginsPath = std::filesystem::path("Plugins");
}

static void printBanner() {
std::cout << LogStyle::paint(R"(
    __    ___  ____________________________   ________    ____
   / /   /   |/_  __/_  __/  _/ ____/ ____/  / ____/ /   /  _/
  / /   / /| | / /   / /  / // /   / __/    / /   / /    / /  
 / /___/ ___ |/ /   / / _/ // /___/ /___   / /___/ /____/ /   
/_____/_/  |_/_/   /_/ /___/\____/_____/   \____/_____/___/   

)", LogStyle::Color::brightCyan);
}

static void printHelp() {
    std::cout << "Available commands:\n";
    std::cout << "  step          - execute one simulation step\n";
    std::cout << "  run [count]   - execute multiple simulation steps\n";
    std::cout << "  positions     - print atom positions\n";
    std::cout << "  metrics       - print current metrics\n";
    std::cout << "  sysinfo       - print system information\n";
    std::cout << "  reset         - reset the world to the initial state\n";
    std::cout << "  help          - show this help message\n";
    std::cout << "  quit          - exit the program\n";
}

static void printAtomPositions(const Simulation& simulation) {
    const auto& atoms = simulation.world().getAtomStorage();
    std::cout << "Atom positions:\n";
    for (size_t i = 0; i < atoms.size(); ++i) {
        const glm::vec3 position = atoms.pos(i);
        std::cout << "  [" << i << "] (" << position.x << ", " << position.y << ", " << position.z << ")\n";
    }
}

static void printMetrics(const Simulation& simulation) {
    const auto& metrics = simulation.world().getMetrics();
    std::cout << std::fixed << std::setprecision(4);
    std::cout << "Metrics:\n";
    std::cout << "  Step: " << simulation.getSimStep() << "\n";
    std::cout << "  Time: " << simulation.simTimeNs() << " ns\n";
    std::cout << "  Kinetic Energy: " << metrics.averageKineticEnergyEv << " eV\n";
    std::cout << "  Potential Energy: " << metrics.averagePotentialEnergyEv << " eV\n";
    std::cout << "  Temperature: " << metrics.temperatureK() << " K\n";
}

static void createInitialSimulation(Simulation& simulation, glm::vec3 worldSize, glm::vec3 gravity, float dt, float maxSpeed) {
    simulation.createWorld(worldSize);
    simulation.setDt(dt);
    simulation.setGravity(gravity);
    simulation.setMaxParticleSpeed(maxSpeed);
    simulation.world().setTitle("CLI-driven LatticeEngine");
    simulation.world().setDescription("Interactive CLI demo world.");

    simulation.createAtom(glm::vec3(-2.5f, 2.0f, 0.0f), glm::vec3(0.0f), AtomData::Type::H);
    simulation.createAtom(glm::vec3(2.5f, 2.0f, 0.0f), glm::vec3(0.0f), AtomData::Type::He);
    simulation.createAtom(glm::vec3(0.0f, -2.0f, 0.0f), glm::vec3(0.0f), AtomData::Type::Li);
}

int main(int argc, char** argv) {
    PluginLoader pluginLoader;
    pluginLoader.load(pluginsPath);

    int defaultSteps = 100;
    float dt = 0.01f;
    glm::vec3 gravity{0.0f, 0.0f, 0.0f};
    glm::vec3 worldSize{10.0f, 10.0f, 10.0f};
    float maxSpeed = 100.0f;
    bool verbose = false;

    CLI::App app{"LatticeEngine CLI"};
    app.add_option("--steps", defaultSteps, "Number of simulation steps used by 'run' command")->check(CLI::Range(1, 1000000));
    app.add_option("--dt", dt, "Integration timestep")->check(CLI::Range(0.0, 10.0));
    app.add_option("--gravity", gravity, "Gravity vector x y z")->expected(3);
    app.add_option("--world-size", worldSize, "World size x y z")->expected(3);
    app.add_option("--max-speed", maxSpeed, "Maximum particle speed");
    app.add_flag("-v,--verbose", verbose, "Enable verbose output");
    CLI11_PARSE(app, argc, argv);

    printBanner();
    Lattice::CliSystemInfo::printSystemInfo(std::cout);

    Simulation simulation;
    createInitialSimulation(simulation, worldSize, gravity, dt, maxSpeed);
    std::cout << "\nType 'help' for available commands.\n";

    if (verbose) {
        std::cout << '\n';
        printMetrics(simulation);
        std::cout << "\n";
    }

    std::string line;
    while (true) {
        std::cout << "\n" << LogStyle::paint("❯", LogStyle::Color::prompt) << ' ';
        if (!std::getline(std::cin, line)) {
            break;
        }
        if (line.empty()) {
            continue;
        }

        std::istringstream iss(line);
        std::vector<std::string> tokens;
        std::string token;
        while (iss >> token) {
            tokens.push_back(token);
        }
        if (tokens.empty()) {
            continue;
        }

        const std::string& command = tokens[0];
        if (command == "q" || command == "quit" || command == "exit") {
            break;
        }
        if (command == "help" || command == "h") {
            printHelp();
            continue;
        }
        if (command == "step" || command == "s") {
            simulation.update();
            printMetrics(simulation);
            continue;
        }
        if (command == "run" || command == "r") {
            int count = defaultSteps;
            if (tokens.size() > 1) {
                count = std::stoi(tokens[1]);
                if (count < 1) {
                    count = 1;
                }
            }
            for (int i = 0; i < count; ++i) {
                simulation.update();
            }
            printMetrics(simulation);
            continue;
        }
        if (command == "positions" || command == "p") {
            printAtomPositions(simulation);
            continue;
        }
        if (command == "metrics" || command == "m") {
            printMetrics(simulation);
            continue;
        }
        if (command == "sysinfo" || command == "si") {
            Lattice::CliSystemInfo::printSystemInfo(std::cout);
            continue;
        }
        if (command == "reset") {
            simulation = Simulation();
            createInitialSimulation(simulation, worldSize, gravity, dt, maxSpeed);
            std::cout << "Simulation reset.\n";
            continue;
        }

        std::cout << "Unknown command: " << command << ". Type 'help' for available commands.\n";
    }

    std::cout << "Exiting LatticeEngine CLI. Goodbye!\n";
    return 0;
}
