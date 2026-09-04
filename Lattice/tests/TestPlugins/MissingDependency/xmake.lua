-- сборка плагина
target("TestMissingDependency")
    set_kind("shared")
    set_targetdir(".")
    add_files("PluginInit.cpp")
    add_includedirs("..", {public = true})
    add_deps("Lattice")