add_executable(LatticeTests
    test.cpp
)

target_link_libraries(LatticeTests PRIVATE Lattice)
add_test(NAME LatticeTests COMMAND LatticeTests)
