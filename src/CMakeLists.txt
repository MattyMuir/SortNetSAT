# EXECUTABLE PROJECT - SortNetSAT

# === Create Executable ===
add_executable(SortNetSAT "main.cpp"   "Timer.h"   "prefixes.h" "prefixes.cpp"   "WindowMinimizer.h" "WindowMinimizer.cpp" "FormulaGenerator.h" "FormulaGenerator.cpp" "Expression.h" "Expression.cpp" "clause.h" "clause.cpp" "SimpleExtender.h" "SimpleExtender.cpp" "Extender.h" "Extender.cpp" "minisatutil.h" "minisatutil.cpp" "IncrementalExtender.h" "IncrementalExtender.cpp" "PrefixGenerator.h" "PrefixGenerator.cpp" "PrefixGeneratorV2.h" "PrefixGeneratorV2.cpp" "NetworkGraph.h" "NetworkGraph.cpp"   "LayerDAG.h" "LayerDAG.cpp")

# === Libraries ===
find_package(sortnetutils REQUIRED)
target_link_libraries(SortNetSAT PUBLIC sortnetutils::sortnetutils)

find_package(minisat REQUIRED)
target_link_libraries(SortNetSAT PUBLIC minisat::minisat)

find_package(bliss REQUIRED)
target_link_libraries(SortNetSAT PUBLIC bliss::bliss)

# === Feature Enables ===
if (SNS_MSVC_STATIC_RUNTIME)
    set_property(TARGET SortNetSAT PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()
target_compile_features(SortNetSAT PRIVATE cxx_std_23)
enable_ipo(SortNetSAT)
set_arch(SortNetSAT)