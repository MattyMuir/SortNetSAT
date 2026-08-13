# EXECUTABLE PROJECT - SortNetSAT

# === Create Executable ===
add_executable(SortNetSAT "main.cpp"   "Timer.h"   "Prefixes/prefixes.h" "Prefixes/prefixes.cpp"   "Prefixes/WindowMinimizer.h" "Prefixes/WindowMinimizer.cpp" "FormulaGenerator.h" "FormulaGenerator.cpp" "Expression.h" "Expression.cpp" "clause.h" "clause.cpp" "SimpleExtender.h" "SimpleExtender.cpp" "Extender.h" "Extender.cpp" "minisatutil.h" "minisatutil.cpp" "IncrementalExtender.h" "IncrementalExtender.cpp"   "Prefixes/SubsumptionSolver.h" "Prefixes/SubsumptionSolver.cpp"    "Prefixes/PrefixGenerator.h" "Prefixes/PrefixGenerator.cpp" "Prefixes/NetworkSignature.h" "Prefixes/NetworkSignature.cpp"  "Prefixes/PrefixDescriptor.h" "Prefixes/PrefixDescriptor.cpp" "Prefixes/IsomorphicOutputSet.cpp" "Prefixes/IsomorphicOutputSet.h" "Prefixes/fastcanonize.cpp" "Prefixes/fastcanonize.h" "Prefixes/gicanonize.cpp" "Prefixes/gicanonize.h" "BulkChecker.h" "BulkChecker.cpp")

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
target_include_directories(SortNetSAT PRIVATE ".")
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