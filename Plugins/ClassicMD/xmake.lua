target("ClassicMD")
    set_kind("shared")
    set_targetdir(".")

    add_files("PluginInit.cpp")
    -- add_files("src/**.cpp")

    add_includedirs("..", {public = true})
    add_includedirs("src")
    
    add_includedirs("../ParticleDynamics/include", {public = true})
    add_includedirs("../StdData/include", {public = true})

    add_deps("Lattice")