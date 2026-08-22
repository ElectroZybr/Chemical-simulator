add_requires("wgpu-native")

target("GPU")
    set_kind("shared")
    set_targetdir(".")

    add_files("PluginInit.cpp")
    add_files("src/**.cpp")

    add_packages("wgpu-native")

    add_includedirs("include", {public = true})
    add_includedirs("..", {public = true})

    add_deps("Lattice")