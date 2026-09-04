target("Lattice")
    set_kind("shared")
    set_targetdir(".")

    add_files("Kernel/*.cpp")
    add_files("Tools/*.cpp")

    add_includedirs("..", {public = true})

    add_packages("glm", "toml++", {public = true})

-- сборка тестов
target("Lattice.tests")
    set_kind("shared")
    set_targetdir(".")

    add_files("tests/*.cpp")
    
    for _, dir in ipairs(os.dirs("tests/TestPlugins/*")) do
        includes(dir)
    end

    add_deps("Lattice")