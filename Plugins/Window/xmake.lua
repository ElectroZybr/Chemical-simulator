add_requires("stb")
add_requires("glfw")

target("Window")
    set_kind("shared")
    set_targetdir(".")

    add_files("PluginInit.cpp")
    add_files("src/**.cpp")

    add_packages("stb")
    add_packages("glfw")

    add_includedirs("include", {public = true})
    add_includedirs("..", {public = true})
    add_includedirs("src")

    add_includedirs("../Render/include")
    add_includedirs("../GPU/include")

    add_deps("Lattice")