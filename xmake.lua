set_project("LatticeLab")
local version = "0.1.4"
set_version(version)
set_languages("c++20")

add_rules("mode.debug", "mode.release")
set_policy("build.compile_commands", true)

add_defines(
    'BUILD_VERSION="' .. version .. '"'
)

if is_mode("release") then
    add_cxxflags(
        "-march=native",
        "-fopt-info-vec-optimized",
        "-fopt-info-vec-missed"
    )
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