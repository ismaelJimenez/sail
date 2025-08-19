cmake_minimum_required(VERSION 3.15...3.23)

project(CmakeConfigPackageTests LANGUAGES CXX)

# ---- Test as standalone project the exported config package ----

if(PROJECT_IS_TOP_LEVEL OR TEST_INSTALLED_VERSION)
  enable_testing()

  find_package(myproject CONFIG REQUIRED) # for sail, project_options, ...

  if(NOT TARGET sail_options)
    message(FATAL_ERROR "Required config package not found!")
    return() # be strictly paranoid for Template Janitor github action! CK
  endif()
endif()

# ---- Dependencies ----

include(${Catch2_SOURCE_DIR}/extras/Catch.cmake)

# Provide a simple smoke test to make sure that the CLI works and can display a --help message
add_test(NAME cli.has_help COMMAND sail --help)

# Provide a test to verify that the version being reported from the application
# matches the version given to CMake. This will be important once you package
# your program. Real world shows that this is the kind of simple mistake that is easy
# to make, but also easy to test for.
add_test(NAME cli.version_matches COMMAND sail --version)
set_tests_properties(cli.version_matches PROPERTIES PASS_REGULAR_EXPRESSION "${PROJECT_VERSION}")

# Test the sail init command creates a Sail.toml file
add_test(NAME cli.init_creates_toml
  COMMAND ${CMAKE_COMMAND} 
  -DSAIL_EXECUTABLE=$<TARGET_FILE:sail>
  -DTEST_WORKING_DIR=${CMAKE_CURRENT_BINARY_DIR}/test_sail_init_temp
  -P ${CMAKE_CURRENT_SOURCE_DIR}/test_sail_init.cmake
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

# Create a test script for sail init
file(WRITE ${CMAKE_CURRENT_SOURCE_DIR}/test_sail_init.cmake "
# Create a temporary directory for testing
file(REMOVE_RECURSE \"\${TEST_WORKING_DIR}\")
file(MAKE_DIRECTORY \"\${TEST_WORKING_DIR}\")

# Run sail init in the test directory
execute_process(
  COMMAND \"\${SAIL_EXECUTABLE}\" init
  WORKING_DIRECTORY \"\${TEST_WORKING_DIR}\"
  RESULT_VARIABLE INIT_RESULT
  OUTPUT_VARIABLE INIT_OUTPUT
  ERROR_VARIABLE INIT_ERROR
  TIMEOUT 30
)

if(NOT INIT_RESULT EQUAL 0)
  message(FATAL_ERROR \"sail init failed with result: \${INIT_RESULT}\\nOutput: \${INIT_OUTPUT}\\nError: \${INIT_ERROR}\")
endif()

# Check if Sail.toml was created
set(TOML_FILE \"\${TEST_WORKING_DIR}/Sail.toml\")
if(NOT EXISTS \"\${TOML_FILE}\")
  message(FATAL_ERROR \"Sail.toml was not created in \${TEST_WORKING_DIR}\")
endif()

# Check content contains expected sections
file(READ \"\${TOML_FILE}\" TOML_CONTENT)
string(FIND \"\${TOML_CONTENT}\" \"[project]\" PROJECT_FOUND)
string(FIND \"\${TOML_CONTENT}\" \"[dependencies]\" DEPS_FOUND)

if(PROJECT_FOUND EQUAL -1)
  message(FATAL_ERROR \"Sail.toml missing [project] section\")
endif()
if(DEPS_FOUND EQUAL -1)
  message(FATAL_ERROR \"Sail.toml missing [dependencies] section\")
endif()

# Clean up
file(REMOVE_RECURSE \"\${TEST_WORKING_DIR}\")
message(STATUS \"sail init test passed - Sail.toml created successfully\")
")

# Test the sail new command creates a project structure
add_test(NAME cli.new_creates_project
  COMMAND ${CMAKE_COMMAND} 
  -DSAIL_EXECUTABLE=$<TARGET_FILE:sail>
  -DTEST_WORKING_DIR=${CMAKE_CURRENT_BINARY_DIR}/test_sail_new_temp
  -DPROJECT_NAME=test_project
  -P ${CMAKE_CURRENT_SOURCE_DIR}/test_sail_new.cmake
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

# Create a test script for sail new
file(WRITE ${CMAKE_CURRENT_SOURCE_DIR}/test_sail_new.cmake "
# Create a temporary directory for testing
file(REMOVE_RECURSE \"\${TEST_WORKING_DIR}\")
file(MAKE_DIRECTORY \"\${TEST_WORKING_DIR}\")

# Run sail new in the test directory
execute_process(
  COMMAND \"\${SAIL_EXECUTABLE}\" new \"\${PROJECT_NAME}\"
  WORKING_DIRECTORY \"\${TEST_WORKING_DIR}\"
  RESULT_VARIABLE NEW_RESULT
  OUTPUT_VARIABLE NEW_OUTPUT
  ERROR_VARIABLE NEW_ERROR
  TIMEOUT 30
)

if(NOT NEW_RESULT EQUAL 0)
  message(FATAL_ERROR \"sail new failed with result: \${NEW_RESULT}\\nOutput: \${NEW_OUTPUT}\\nError: \${NEW_ERROR}\")
endif()

# Check if project directory was created
set(PROJECT_DIR \"\${TEST_WORKING_DIR}/\${PROJECT_NAME}\")
if(NOT EXISTS \"\${PROJECT_DIR}\")
  message(FATAL_ERROR \"Project directory '\${PROJECT_NAME}' was not created\")
endif()

# Check if Sail.toml was created
set(TOML_FILE \"\${PROJECT_DIR}/Sail.toml\")
if(NOT EXISTS \"\${TOML_FILE}\")
  message(FATAL_ERROR \"Sail.toml was not created in project directory\")
endif()

# Check if src directory was created
set(SRC_DIR \"\${PROJECT_DIR}/src\")
if(NOT EXISTS \"\${SRC_DIR}\")
  message(FATAL_ERROR \"src directory was not created\")
endif()

# Check if main.cpp was created
set(MAIN_CPP \"\${SRC_DIR}/main.cpp\")
if(NOT EXISTS \"\${MAIN_CPP}\")
  message(FATAL_ERROR \"src/main.cpp was not created\")
endif()

# Check Sail.toml content
file(READ \"\${TOML_FILE}\" TOML_CONTENT)
string(FIND \"\${TOML_CONTENT}\" \"name = \\\"\${PROJECT_NAME}\\\"\" NAME_FOUND)
if(NAME_FOUND EQUAL -1)
  message(FATAL_ERROR \"Sail.toml does not contain correct project name\")
endif()

# Check main.cpp content
file(READ \"\${MAIN_CPP}\" CPP_CONTENT)
string(FIND \"\${CPP_CONTENT}\" \"Hello, World!\" HELLO_FOUND)
if(HELLO_FOUND EQUAL -1)
  message(FATAL_ERROR \"main.cpp does not contain Hello World message\")
endif()

# Clean up
file(REMOVE_RECURSE \"\${TEST_WORKING_DIR}\")
message(STATUS \"sail new test passed - project structure created successfully\")
")

# Test the sail build command compiles a project
add_test(NAME cli.build_compiles_project
  COMMAND ${CMAKE_COMMAND} 
  -DSAIL_EXECUTABLE=$<TARGET_FILE:sail>
  -DTEST_WORKING_DIR=${CMAKE_CURRENT_BINARY_DIR}/test_sail_build_temp
  -DPROJECT_NAME=build_test
  -P ${CMAKE_CURRENT_SOURCE_DIR}/test_sail_build.cmake
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

# Create a test script for sail build
file(WRITE ${CMAKE_CURRENT_SOURCE_DIR}/test_sail_build.cmake "
# Create a temporary directory for testing
file(REMOVE_RECURSE \"\${TEST_WORKING_DIR}\")
file(MAKE_DIRECTORY \"\${TEST_WORKING_DIR}\")

# First create a new project
execute_process(
  COMMAND \"\${SAIL_EXECUTABLE}\" new \"\${PROJECT_NAME}\"
  WORKING_DIRECTORY \"\${TEST_WORKING_DIR}\"
  RESULT_VARIABLE NEW_RESULT
  OUTPUT_VARIABLE NEW_OUTPUT
  ERROR_VARIABLE NEW_ERROR
  TIMEOUT 30
)

if(NOT NEW_RESULT EQUAL 0)
  message(FATAL_ERROR \"sail new failed: \${NEW_OUTPUT} \${NEW_ERROR}\")
endif()

set(PROJECT_DIR \"\${TEST_WORKING_DIR}/\${PROJECT_NAME}\")

# Now try to build the project (debug mode)
execute_process(
  COMMAND \"\${SAIL_EXECUTABLE}\" build
  WORKING_DIRECTORY \"\${PROJECT_DIR}\"
  RESULT_VARIABLE BUILD_RESULT
  OUTPUT_VARIABLE BUILD_OUTPUT
  ERROR_VARIABLE BUILD_ERROR
  TIMEOUT 60
)

if(NOT BUILD_RESULT EQUAL 0)
  message(FATAL_ERROR \"sail build failed: \${BUILD_OUTPUT} \${BUILD_ERROR}\")
endif()

# Check if target directory was created
set(TARGET_DEBUG_DIR \"\${PROJECT_DIR}/target/debug\")
if(NOT EXISTS \"\${TARGET_DEBUG_DIR}\")
  message(FATAL_ERROR \"target/debug directory was not created\")
endif()

# Check if executable was created (handle Windows .exe extension)
if(WIN32)
  set(EXECUTABLE_PATH \"\${TARGET_DEBUG_DIR}/\${PROJECT_NAME}.exe\")
else()
  set(EXECUTABLE_PATH \"\${TARGET_DEBUG_DIR}/\${PROJECT_NAME}\")
endif()
if(NOT EXISTS \"\${EXECUTABLE_PATH}\")
  message(FATAL_ERROR \"Executable was not created at \${EXECUTABLE_PATH}\")
endif()

# Test build from subdirectory (src)
execute_process(
  COMMAND \"\${SAIL_EXECUTABLE}\" build --release
  WORKING_DIRECTORY \"\${PROJECT_DIR}/src\"
  RESULT_VARIABLE BUILD_RELEASE_RESULT
  OUTPUT_VARIABLE BUILD_RELEASE_OUTPUT
  ERROR_VARIABLE BUILD_RELEASE_ERROR
  TIMEOUT 60
)

if(NOT BUILD_RELEASE_RESULT EQUAL 0)
  message(FATAL_ERROR \"sail build --release from subdirectory failed: \${BUILD_RELEASE_OUTPUT} \${BUILD_RELEASE_ERROR}\")
endif()

# Check if release executable was created (handle Windows .exe extension)
set(TARGET_RELEASE_DIR \"\${PROJECT_DIR}/target/release\")
if(WIN32)
  set(RELEASE_EXECUTABLE_PATH \"\${TARGET_RELEASE_DIR}/\${PROJECT_NAME}.exe\")
else()
  set(RELEASE_EXECUTABLE_PATH \"\${TARGET_RELEASE_DIR}/\${PROJECT_NAME}\")
endif()
if(NOT EXISTS \"\${RELEASE_EXECUTABLE_PATH}\")
  message(FATAL_ERROR \"Release executable was not created at \${RELEASE_EXECUTABLE_PATH}\")
endif()

# Clean up
file(REMOVE_RECURSE \"\${TEST_WORKING_DIR}\")
message(STATUS \"sail build test passed - project compiled successfully\")
")

# Test the sail run command builds and executes a project
add_test(NAME cli.run_executes_project
  COMMAND ${CMAKE_COMMAND} 
  -DSAIL_EXECUTABLE=$<TARGET_FILE:sail>
  -DTEST_WORKING_DIR=${CMAKE_CURRENT_BINARY_DIR}/test_sail_run_temp
  -DPROJECT_NAME=run_test
  -P ${CMAKE_CURRENT_SOURCE_DIR}/test_sail_run.cmake
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

# Create a test script for sail run
file(WRITE ${CMAKE_CURRENT_SOURCE_DIR}/test_sail_run.cmake "
# Create a temporary directory for testing
file(REMOVE_RECURSE \"\${TEST_WORKING_DIR}\")
file(MAKE_DIRECTORY \"\${TEST_WORKING_DIR}\")

# First create a new project
execute_process(
  COMMAND \"\${SAIL_EXECUTABLE}\" new \"\${PROJECT_NAME}\"
  WORKING_DIRECTORY \"\${TEST_WORKING_DIR}\"
  RESULT_VARIABLE NEW_RESULT
  OUTPUT_VARIABLE NEW_OUTPUT
  ERROR_VARIABLE NEW_ERROR
  TIMEOUT 30
)

if(NOT NEW_RESULT EQUAL 0)
  message(FATAL_ERROR \"sail new failed: \${NEW_OUTPUT} \${NEW_ERROR}\")
endif()

set(PROJECT_DIR \"\${TEST_WORKING_DIR}/\${PROJECT_NAME}\")

# Now try to run the project (debug mode)
execute_process(
  COMMAND \"\${SAIL_EXECUTABLE}\" run
  WORKING_DIRECTORY \"\${PROJECT_DIR}\"
  RESULT_VARIABLE RUN_RESULT
  OUTPUT_VARIABLE RUN_OUTPUT
  ERROR_VARIABLE RUN_ERROR
  TIMEOUT 60
)

if(NOT RUN_RESULT EQUAL 0)
  message(FATAL_ERROR \"sail run failed: \${RUN_OUTPUT} \${RUN_ERROR}\")
endif()

# Check that the output contains 'Hello, World!'
string(FIND \"\${RUN_OUTPUT}\" \"Hello, World!\" HELLO_FOUND)
if(HELLO_FOUND EQUAL -1)
  message(FATAL_ERROR \"Expected 'Hello, World!' in output, got: \${RUN_OUTPUT}\")
endif()

# Check if target directory was created
set(TARGET_DEBUG_DIR \"\${PROJECT_DIR}/target/debug\")
if(NOT EXISTS \"\${TARGET_DEBUG_DIR}\")
  message(FATAL_ERROR \"target/debug directory was not created\")
endif()

# Check if executable was created (handle Windows .exe extension)
if(WIN32)
  set(EXECUTABLE_PATH \"\${TARGET_DEBUG_DIR}/\${PROJECT_NAME}.exe\")
else()
  set(EXECUTABLE_PATH \"\${TARGET_DEBUG_DIR}/\${PROJECT_NAME}\")
endif()
if(NOT EXISTS \"\${EXECUTABLE_PATH}\")
  message(FATAL_ERROR \"Executable was not created at \${EXECUTABLE_PATH}\")
endif()

# Test run with --release flag from subdirectory
execute_process(
  COMMAND \"\${SAIL_EXECUTABLE}\" run --release
  WORKING_DIRECTORY \"\${PROJECT_DIR}/src\"
  RESULT_VARIABLE RUN_RELEASE_RESULT
  OUTPUT_VARIABLE RUN_RELEASE_OUTPUT
  ERROR_VARIABLE RUN_RELEASE_ERROR
  TIMEOUT 60
)

if(NOT RUN_RELEASE_RESULT EQUAL 0)
  message(FATAL_ERROR \"sail run --release from subdirectory failed: \${RUN_RELEASE_OUTPUT} \${RUN_RELEASE_ERROR}\")
endif()

# Check that the release run also produced Hello, World!
string(FIND \"\${RUN_RELEASE_OUTPUT}\" \"Hello, World!\" HELLO_RELEASE_FOUND)
if(HELLO_RELEASE_FOUND EQUAL -1)
  message(FATAL_ERROR \"Expected 'Hello, World!' in release output, got: \${RUN_RELEASE_OUTPUT}\")
endif()

# Check if release executable was created (handle Windows .exe extension)
set(TARGET_RELEASE_DIR \"\${PROJECT_DIR}/target/release\")
if(WIN32)
  set(RELEASE_EXECUTABLE_PATH \"\${TARGET_RELEASE_DIR}/\${PROJECT_NAME}.exe\")
else()
  set(RELEASE_EXECUTABLE_PATH \"\${TARGET_RELEASE_DIR}/\${PROJECT_NAME}\")
endif()
if(NOT EXISTS \"\${RELEASE_EXECUTABLE_PATH}\")
  message(FATAL_ERROR \"Release executable was not created at \${RELEASE_EXECUTABLE_PATH}\")
endif()

# Clean up
file(REMOVE_RECURSE \"\${TEST_WORKING_DIR}\")
message(STATUS \"sail run test passed - project ran successfully\")
")

add_executable(tests tests.cpp)
target_link_libraries(
  tests
  PRIVATE sail::sail_warnings
          sail::sail_options
          sail::sample_library
          Catch2::Catch2WithMain)

if(WIN32 AND BUILD_SHARED_LIBS)
  add_custom_command(
    TARGET tests
    PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:tests> $<TARGET_FILE_DIR:tests>
    COMMAND_EXPAND_LISTS)
endif()

# automatically discover tests that are defined in catch based test files you can modify the unittests. Set TEST_PREFIX
# to whatever you want, or use different for different binaries
catch_discover_tests(
  tests
  TEST_PREFIX
  "unittests."
  REPORTER
  XML
  OUTPUT_DIR
  .
  OUTPUT_PREFIX
  "unittests."
  OUTPUT_SUFFIX
  .xml)

# Add a file containing a set of constexpr tests
add_executable(constexpr_tests constexpr_tests.cpp)
target_link_libraries(
  constexpr_tests
  PRIVATE sail::sail_warnings
          sail::sail_options
          sail::sample_library
          Catch2::Catch2WithMain)

catch_discover_tests(
  constexpr_tests
  TEST_PREFIX
  "constexpr."
  REPORTER
  XML
  OUTPUT_DIR
  .
  OUTPUT_PREFIX
  "constexpr."
  OUTPUT_SUFFIX
  .xml)

# Disable the constexpr portion of the test, and build again this allows us to have an executable that we can debug when
# things go wrong with the constexpr testing
add_executable(relaxed_constexpr_tests constexpr_tests.cpp)
target_link_libraries(
  relaxed_constexpr_tests
  PRIVATE sail::sail_warnings
          sail::sail_options
          sail::sample_library
          Catch2::Catch2WithMain)
target_compile_definitions(relaxed_constexpr_tests PRIVATE -DCATCH_CONFIG_RUNTIME_STATIC_REQUIRE)

catch_discover_tests(
  relaxed_constexpr_tests
  TEST_PREFIX
  "relaxed_constexpr."
  REPORTER
  XML
  OUTPUT_DIR
  .
  OUTPUT_PREFIX
  "relaxed_constexpr."
  OUTPUT_SUFFIX
  .xml)
