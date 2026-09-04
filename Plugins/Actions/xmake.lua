-- сборка плагина
target("Actions")
    set_kind("shared")
    set_targetdir(".")

    add_files("PluginInit.cpp")
    add_files("src/**.cpp")

    add_includedirs("include", {public = true})
    add_includedirs("..", {public = true})

    add_deps("Lattice")

-- сборка тестов
target("Actions.tests")
    set_kind("shared")
    set_targetdir(".")

    add_files("tests/*.cpp")

    add_deps("Lattice")