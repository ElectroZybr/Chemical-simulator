target("Lattice")
    set_kind("shared")

    add_files("Kernel/*.cpp")
    add_files("Tools/*.cpp")
    add_files("Tests/*.cpp")

    add_includedirs("..", {public = true})

    add_packages("glm", "toml++", {public = true})