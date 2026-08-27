// #include <Lattice/Tools/Tests.hpp>
// #include <Lattice/Tools/Logger.hpp>

// TEST2(LoggerScope_InitialState, Lattice::TestFixture) {
//     REQUIRE(Logger::indent() == 0);
//     REQUIRE(Logger::scopeDepth() == 0);
// }

// TEST2(LoggerScope_Enter, Lattice::TestFixture) {
//     REQUIRE(Logger::indent() == 0);
//     REQUIRE(Logger::scopeDepth() == 0);

//     {
//         Logger::Scope scope("Test", "test");

//         REQUIRE(Logger::indent() == 1);
//         REQUIRE(Logger::scopeDepth() == 1);
//     }

//     REQUIRE(Logger::indent() == 0);
//     REQUIRE(Logger::scopeDepth() == 0);
// }

// TEST2(LoggerScope_Nested, Lattice::TestFixture) {
//     REQUIRE(Logger::indent() == 0);
//     REQUIRE(Logger::scopeDepth() == 0);

//     {
//         Logger::Scope outer("Test", "outer");

//         REQUIRE(Logger::indent() == 1);
//         REQUIRE(Logger::scopeDepth() == 1);

//         {
//             Logger::Scope inner("Test", "inner");

//             REQUIRE(Logger::indent() == 2);
//             REQUIRE(Logger::scopeDepth() == 2);

//             {
//                 Logger::Scope deep("Test", "deep");

//                 REQUIRE(Logger::indent() == 3);
//                 REQUIRE(Logger::scopeDepth() == 3);
//             }

//             REQUIRE(Logger::indent() == 2);
//             REQUIRE(Logger::scopeDepth() == 2);
//         }

//         REQUIRE(Logger::indent() == 1);
//         REQUIRE(Logger::scopeDepth() == 1);
//     }

//     REQUIRE(Logger::indent() == 0);
//     REQUIRE(Logger::scopeDepth() == 0);
// }

// TEST2(LoggerScope_Finish, Lattice::TestFixture) {
//     {
//         Logger::Scope scope("Test", "test");

//         REQUIRE(Logger::indent() == 1);
//         REQUIRE(Logger::scopeDepth() == 1);

//         scope.finish();

//         REQUIRE(Logger::indent() == 0);
//         REQUIRE(Logger::scopeDepth() == 0);
//     }

//     REQUIRE(Logger::indent() == 0);
//     REQUIRE(Logger::scopeDepth() == 0);
// }

// TEST2(LoggerScope_FinishOnlyOnce, Lattice::TestFixture) {
//     Logger::Scope scope("Test", "test");

//     REQUIRE(Logger::indent() == 1);
//     REQUIRE(Logger::scopeDepth() == 1);

//     scope.finish();

//     REQUIRE(Logger::indent() == 0);
//     REQUIRE(Logger::scopeDepth() == 0);

//     scope.finish();

//     REQUIRE(Logger::indent() == 0);
//     REQUIRE(Logger::scopeDepth() == 0);

//     scope.finishError("error");

//     REQUIRE(Logger::indent() == 0);
//     REQUIRE(Logger::scopeDepth() == 0);
// }

// TEST2(LoggerScope_FinishError, Lattice::TestFixture) {
//     {
//         Logger::Scope scope("Test", "test");

//         REQUIRE(Logger::indent() == 1);
//         REQUIRE(Logger::scopeDepth() == 1);

//         scope.finishError("failed");

//         REQUIRE(Logger::indent() == 0);
//         REQUIRE(Logger::scopeDepth() == 0);
//     }

//     REQUIRE(Logger::indent() == 0);
//     REQUIRE(Logger::scopeDepth() == 0);
// }

// TEST2(LoggerScope_FinishErrorOnlyOnce, Lattice::TestFixture) {
//     Logger::Scope scope("Test", "test");

//     scope.finishError("first");

//     REQUIRE(Logger::indent() == 0);
//     REQUIRE(Logger::scopeDepth() == 0);

//     scope.finishError("second");
//     scope.finish();

//     REQUIRE(Logger::indent() == 0);
//     REQUIRE(Logger::scopeDepth() == 0);
// }

// TEST2(LoggerScope_Cancel, Lattice::TestFixture) {
//     {
//         Logger::Scope scope("Test", "test");

//         REQUIRE(Logger::indent() == 1);
//         REQUIRE(Logger::scopeDepth() == 1);

//         scope.cancel();

//         REQUIRE(Logger::indent() == 0);
//         REQUIRE(Logger::scopeDepth() == 0);
//     }

//     REQUIRE(Logger::indent() == 0);
//     REQUIRE(Logger::scopeDepth() == 0);
// }

// TEST2(LoggerScope_CancelOnlyOnce, Lattice::TestFixture) {
//     Logger::Scope scope("Test", "test");

//     scope.cancel();

//     REQUIRE(Logger::indent() == 0);
//     REQUIRE(Logger::scopeDepth() == 0);

//     scope.cancel();
//     scope.finish();
//     scope.finishError("error");

//     REQUIRE(Logger::indent() == 0);
//     REQUIRE(Logger::scopeDepth() == 0);
// }

// TEST2(LoggerScope_Destructor, Lattice::TestFixture) {
//     REQUIRE(Logger::indent() == 0);
//     REQUIRE(Logger::scopeDepth() == 0);

//     {
//         Logger::Scope scope("Test", "test");

//         REQUIRE(Logger::indent() == 1);
//         REQUIRE(Logger::scopeDepth() == 1);
//     }

//     REQUIRE(Logger::indent() == 0);
//     REQUIRE(Logger::scopeDepth() == 0);
// }

// TEST2(LoggerScope_NestedFinishRestoresParent, Lattice::TestFixture) {
//     {
//         Logger::Scope outer("Test", "outer");

//         {
//             Logger::Scope inner("Test", "inner");

//             REQUIRE(Logger::indent() == 2);
//             REQUIRE(Logger::scopeDepth() == 2);

//             inner.finish();

//             REQUIRE(Logger::indent() == 1);
//             REQUIRE(Logger::scopeDepth() == 1);
//         }

//         REQUIRE(Logger::indent() == 1);
//         REQUIRE(Logger::scopeDepth() == 1);
//     }

//     REQUIRE(Logger::indent() == 0);
//     REQUIRE(Logger::scopeDepth() == 0);
// }

// TEST2(LoggerScope_SiblingScopes, Lattice::TestFixture) {
//     {
//         Logger::Scope first("Test", "first");

//         REQUIRE(Logger::indent() == 1);
//         REQUIRE(Logger::scopeDepth() == 1);

//         first.finish();

//         REQUIRE(Logger::indent() == 0);
//         REQUIRE(Logger::scopeDepth() == 0);
//     }

//     {
//         Logger::Scope second("Test", "second");

//         REQUIRE(Logger::indent() == 1);
//         REQUIRE(Logger::scopeDepth() == 1);

//         second.finish();

//         REQUIRE(Logger::indent() == 0);
//         REQUIRE(Logger::scopeDepth() == 0);
//     }
// }

// TEST2(LoggerScope_DeepNesting, Lattice::TestFixture) {
//     {
//         Logger::Scope a("Test", "a");
//         Logger::Scope b("Test", "b");
//         Logger::Scope c("Test", "c");
//         Logger::Scope d("Test", "d");
//         Logger::Scope e("Test", "e");

//         REQUIRE(Logger::indent() == 5);
//         REQUIRE(Logger::scopeDepth() == 5);

//         e.finish();

//         REQUIRE(Logger::indent() == 4);
//         REQUIRE(Logger::scopeDepth() == 4);

//         d.finish();

//         REQUIRE(Logger::indent() == 3);
//         REQUIRE(Logger::scopeDepth() == 3);
//     }

//     REQUIRE(Logger::indent() == 0);
//     REQUIRE(Logger::scopeDepth() == 0);
// }

// TEST2(LoggerScope_Invariant, Lattice::TestFixture) {
//     REQUIRE(Logger::indent() == Logger::scopeDepth());

//     {
//         Logger::Scope a("Test", "a");
//         REQUIRE(Logger::indent() == Logger::scopeDepth());

//         {
//             Logger::Scope b("Test", "b");
//             REQUIRE(Logger::indent() == Logger::scopeDepth());

//             {
//                 Logger::Scope c("Test", "c");
//                 REQUIRE(Logger::indent() == Logger::scopeDepth());
//             }

//             REQUIRE(Logger::indent() == Logger::scopeDepth());
//         }

//         REQUIRE(Logger::indent() == Logger::scopeDepth());
//     }

//     REQUIRE(Logger::indent() == Logger::scopeDepth());
// }