# EXECUTABLE PROJECT - SortNetSAT

# === Create Executable ===
add_executable(SortNetSAT "main.cpp" "Assignment.h" "Expression.h" "Timer.h" "Assignment.cpp" "Expression.cpp" "prefixes.h" "prefixes.cpp" "Network.h" "Network.cpp" "WindowMinimizer.h" "WindowMinimizer.cpp")

# === Feature Enables ===
if (SNS_MSVC_STATIC_RUNTIME)
    set_property(TARGET SortNetSAT PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()
target_compile_features(SortNetSAT PRIVATE cxx_std_23)
enable_ipo(SortNetSAT)
set_arch(SortNetSAT)