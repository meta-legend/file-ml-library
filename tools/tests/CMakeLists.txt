# Smoke test target, driven by CTest. Built only when FILEML_BUILD_TESTS=ON;
# consumers of the library never see this.
add_executable(fileml_smoke main.cpp)
target_link_libraries(fileml_smoke PRIVATE fileml)

add_test(NAME smoke COMMAND fileml_smoke)
set_tests_properties(smoke PROPERTIES TIMEOUT 60)
