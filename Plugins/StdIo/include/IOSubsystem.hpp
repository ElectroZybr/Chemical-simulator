#pragma once

#include <string>
#include <vector>

// Kernel dependences
#include <Lattice/Kernel/PluginAPI.hpp>
#include "Lattice/Kernel/SubsystemAPI.hpp"
#include "Lattice/Tools/Logger.hpp"
#include <Lattice/Kernel/Components.hpp>
#include <Lattice/Kernel/Settings.hpp>

// Plugin dependences


// Source
#include "Document.hpp"
#include "LoaderAPI.hpp"
#include "ParserAPI.hpp"


class IOSubsystem final : public SubsystemAPI {
public:
    explicit IOSubsystem(Lattice::Components& ioBranch) {
        components = &ioBranch;
        loaders = ioBranch.addImpls<LoaderAPI>();
        parsers = ioBranch.addImpls<ParserAPI>();
        Lattice::Settings* settings = ioBranch.require<Lattice::Settings>(); 
        settings->on("io", "load", [&]() { load("Lattice.toml"); } );
    }

    void load(const std::filesystem::path& path) {
        Logger::ok("IOSubsystem", "загрузка отсюда: {}", std::string(path));
        ParserAPI* parser = findParser(path);
        Document doc;
        if (parser)
            doc = parser->parseFile(path);

        for (LoaderAPI* loader : loaders) {
            loader->load(doc);
        }
    }

    void save() {

    }

    ~IOSubsystem() {

    }

private:
    Lattice::Components* components = nullptr;
    ParserAPI* findParser(const std::filesystem::path& path) {
        for (ParserAPI* parser : parsers)
            if (parser && parser->extension() == path.extension())
                return parser;
        Logger::warning("IOSubsystem", "Parser for extension {} not found", std::string(path.extension()));
        return nullptr;
    }
    std::vector<LoaderAPI*> loaders;
    std::vector<ParserAPI*> parsers;
};
