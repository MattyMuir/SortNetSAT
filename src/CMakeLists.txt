# EXECUTABLE PROJECT - SortNetSAT

# === Create Executable ===
add_executable(SortNetSAT "main.cpp"   "Timer.h"   "Prefixes/prefixes.h" "Prefixes/prefixes.cpp"   "Prefixes/WindowMinimizer.h" "Prefixes/WindowMinimizer.cpp" "FormulaGenerator.h" "FormulaGenerator.cpp" "Expression.h" "Expression.cpp" "clause.h" "clause.cpp" "SimpleExtender.h" "SimpleExtender.cpp" "Extender.h" "Extender.cpp" "minisatutil.h" "minisatutil.cpp" "IncrementalExtender.h" "IncrementalExtender.cpp" "Prefixes/PrefixGenerator.h" "Prefixes/PrefixGenerator.cpp" "Prefixes/PrefixGeneratorV2.h" "Prefixes/PrefixGeneratorV2.cpp" "Prefixes/NetworkGraph.h" "Prefixes/NetworkGraph.cpp"   "Prefixes/LayerDAG.h" "Prefixes/LayerDAG.cpp"   "Prefixes/PrefixGraph.h" "Prefixes/PrefixGraph.cpp" "Prefixes/PrefixGeneratorV3.h" "Prefixes/PrefixGeneratorV3.cpp" "Prefixes/KosarajuSolver.h" "Prefixes/KosarajuSolver.cpp"   "Prefixes/GraphvizSerializer.h" "Prefixes/GraphvizSerializer.cpp"     "Prefixes/IsomorphicOutputSet.h" "Prefixes/IsomorphicOutputSet.cpp" "Prefixes/SubsumptionSolver.h" "Prefixes/SubsumptionSolver.cpp")

# === Libraries ===
find_package(sortnetutils REQUIRED)
target_link_libraries(SortNetSAT PUBLIC sortnetutils::sortnetutils)

find_package(minisat REQUIRED)
target_link_libraries(SortNetSAT PUBLIC minisat::minisat)

find_package(bliss REQUIRED)
target_link_libraries(SortNetSAT PUBLIC bliss::bliss)

find_package(Threads REQUIRED)
target_link_libraries(SortNetSAT PUBLIC Threads::Threads)

# === Feature Enables ===
if (SNS_MSVC_STATIC_RUNTIME)
    set_property(TARGET SortNetSAT PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()
target_compile_features(SortNetSAT PRIVATE cxx_std_23)
enable_ipo(SortNetSAT)
set_arch(SortNetSAT)

set(HAMILTON OFF CACHE BOOL "Link dynamically against the standard library on Hamilton")
if (UNIX)
    set_target_properties(SortNetSAT PROPERTIES
		BUILD_RPATH "/apps/developers/compilers/llvm/20.1.8/1/default/lib/x86_64-unknown-linux-gnu"
	)
endif()