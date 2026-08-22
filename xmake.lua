set_project("LatticeLab")
local version = "0.1.4"
set_version(version)
add_defines('BUILD_VERSION="' .. version .. '"')
set_languages("c++20")

add_rules("mode.debug", "mode.release")

add_rules("plugin.compile_commands.autoupdate", {
    outputdir = "."
})

if is_mode("release") then
    add_cxxflags("-march=native")
end

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