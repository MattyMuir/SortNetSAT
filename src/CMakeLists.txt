# EXECUTABLE PROJECT - SortNetSAT

# === Create Executable ===
add_executable(SortNetSAT "main.cpp"   "Timer.h"   "prefixes.h" "prefixes.cpp"   "WindowMinimizer.h" "WindowMinimizer.cpp" "FormulaGenerator.h" "FormulaGenerator.cpp" "Expression.h" "Expression.cpp" "clause.h" "clause.cpp")

# === Libraries ===
find_package(sortnetutils REQUIRED)
target_link_libraries(SortNetSAT PUBLIC sortnetutils::sortnetutils)

find_package(kissat REQUIRED)
target_link_libraries(SortNetSAT PUBLIC kissat::kissat)

find_package(minisat REQUIRED)
target_link_libraries(SortNetSAT PUBLIC minisat::minisat)

# === Feature Enables ===
if (SNS_MSVC_STATIC_RUNTIME)
    set_property(TARGET SortNetSAT PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()
target_compile_features(SortNetSAT PRIVATE cxx_std_23)
enable_ipo(SortNetSAT)
set_arch(SortNetSAT)