set_project("LatticeLab")
set_version("0.1.4")
set_languages("c++20")

add_rules("mode.debug", "mode.release")

add_requires(
    "glm",
    "toml++"
)

includes("Lattice")

for _, dir in ipairs(os.dirs("Plugins/*")) do
    includes(dir)
end

target("LatticeLab")
    set_kind("binary")
    set_targetdir(".")
    
    add_files("main.cpp")

    add_deps("Lattice")