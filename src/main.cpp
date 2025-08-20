#include <cstdlib>
#include <exception>
#include <fmt/format.h>
#include <fmt/base.h>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <fstream>
#include <filesystem>
#include <utility>
#include <spdlog/spdlog.h>

#include <CLI/CLI.hpp>

// This file will be generated automatically when you run the CMake
// configuration step. It creates a namespace called `sail`. You can modify
// the source template at `configured_files/config.hpp.in`.
#include <internal_use_only/config.hpp>

#ifdef _WIN32
    constexpr std::string_view EXECUTABLE_EXTENSION{".exe"};
#else
    constexpr std::string_view EXECUTABLE_EXTENSION{};
#endif

// Helper function to get executable path with correct extension
std::filesystem::path get_executable_path(const std::filesystem::path& target_dir, const std::string& project_name) {
  return target_dir / (project_name + std::string(EXECUTABLE_EXTENSION));
}

// Helper function to quote paths for system commands
std::string quote_path(const std::string& path) {
#ifdef _WIN32
  return "\"" + path + "\"";
#else
  // Escape spaces and special characters on Unix-like systems
  std::string result = path;
  size_t pos = 0;
  while ((pos = result.find(' ', pos)) != std::string::npos) {
    result.insert(pos, "\\");
    pos += 2;
  }
  return result;
#endif
}

// Helper function to build project (used by both build and run commands)
// cppcheck-suppress normalCheckLevelMaxBranches
std::pair<int, std::filesystem::path> build_project(bool release_mode) {
  // Find project root by looking for Sail.toml
  std::filesystem::path project_root;
  std::filesystem::path current_path = std::filesystem::current_path();
  
  while (current_path != current_path.root_path()) {
    const std::filesystem::path sail_toml_path = current_path / "Sail.toml";
    if (std::filesystem::exists(sail_toml_path)) {
      project_root = current_path;
      break;
    }
    current_path = current_path.parent_path();
  }
  
  if (project_root.empty()) {
    fmt::print("Error: Sail.toml not found in current directory or any parent directory. Run 'sail init' first.\\n");
    return {EXIT_FAILURE, {}};
  }
  
  // Read project name from Sail.toml
  const std::filesystem::path sail_toml_path = project_root / "Sail.toml";
  std::ifstream toml_file(sail_toml_path);
  if (!toml_file) {
    fmt::print("Error: Failed to read Sail.toml\\n");
    return {EXIT_FAILURE, {}};
  }
  
  std::string project_name;
  std::string line;
  while (std::getline(toml_file, line)) {
    if (line.find("name = \"") != std::string::npos) {
      const size_t start = line.find('\"') + 1;
      const size_t end = line.find('\"', start);
      if (start != std::string::npos && end != std::string::npos) {
        project_name = line.substr(start, end - start);
        break;
      }
    }
  }
  
  if (project_name.empty()) {
    fmt::print("Error: Could not find project name in Sail.toml\\n");
    return {EXIT_FAILURE, {}};
  }
  
  // Check if src directory exists
  const std::filesystem::path src_dir = project_root / "src";
  if (!std::filesystem::exists(src_dir)) {
    fmt::print("Error: src directory not found\\n");
    return {EXIT_FAILURE, {}};
  }
  
  // Determine build mode and target directory
  const std::string build_mode = release_mode ? "Release" : "Debug";
  const std::string target_subdir = release_mode ? "release" : "debug";
  const std::filesystem::path target_dir = project_root / "target" / target_subdir;
  
  try {
    // Create target directories
    std::filesystem::create_directories(target_dir);
    
    // Generate CMakeLists.txt if it doesn't exist
    const std::filesystem::path cmake_path = project_root / "CMakeLists.txt";
    if (!std::filesystem::exists(cmake_path)) {
      const std::string cmake_content = fmt::format(R"(cmake_minimum_required(VERSION 3.21)

project({} VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Collect all source files
file(GLOB_RECURSE SOURCES src/*.cpp src/*.c)

# Create executable
add_executable({} ${{SOURCES}})

# Set output directory based on build type
set_target_properties({} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${{CMAKE_SOURCE_DIR}}/target/debug"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${{CMAKE_SOURCE_DIR}}/target/release"
)

# Ensure consistent output name across platforms
set_target_properties({} PROPERTIES OUTPUT_NAME "{}")
)", project_name, project_name, project_name, project_name, project_name);
      
      std::ofstream cmake_file(cmake_path);
      if (!cmake_file) {
        fmt::print("Error: Failed to create CMakeLists.txt\\n");
        return {EXIT_FAILURE, {}};
      }
      cmake_file << cmake_content;
    }
    
    // Create build directory inside target
    const std::filesystem::path build_dir = target_dir / "build";
    std::filesystem::create_directories(build_dir);
    
    // Run CMake configure
    const std::string cmake_configure_cmd = fmt::format(
      "cmake -DCMAKE_BUILD_TYPE={} -S {} -B {}",
      build_mode,
      quote_path(project_root.string()),
      quote_path(build_dir.string())
    );
    
    const int configure_result = std::system(cmake_configure_cmd.c_str());  // NOLINT(cert-env33-c,concurrency-mt-unsafe)
    if (configure_result != 0) {
      fmt::print("Error: CMake configuration failed\\n");
      return {EXIT_FAILURE, {}};
    }
    
    // Run CMake build
    const std::string cmake_build_cmd = fmt::format(
      "cmake --build {} --config {}",
      quote_path(build_dir.string()),
      build_mode
    );
    
    const int build_result = std::system(cmake_build_cmd.c_str());  // NOLINT(cert-env33-c,concurrency-mt-unsafe)
    if (build_result != 0) {
      fmt::print("Error: Build failed\\n");
      return {EXIT_FAILURE, {}};
    }
    
    const std::filesystem::path executable_path = get_executable_path(target_dir, project_name);
    return {EXIT_SUCCESS, executable_path};
    
  } catch (const std::filesystem::filesystem_error& e) {
    fmt::print("Error: {}\\n", e.what());
    return {EXIT_FAILURE, {}};
  } catch (const std::exception& e) {
    fmt::print("Error: {}\\n", e.what());
    return {EXIT_FAILURE, {}};
  }
}

// Handler for run subcommand
int handle_run_command(bool run_release, const std::vector<std::string>& run_args) {
  fmt::print("Compiling {}...\\n", run_release ? "release" : "debug");
  
  const auto [build_result, executable_path] = build_project(run_release);
  if (build_result != EXIT_SUCCESS) {
    return build_result;
  }
  
  if (!std::filesystem::exists(executable_path)) {
    fmt::print("Error: Executable not found at {}\\n", executable_path.string());
    return EXIT_FAILURE;
  }
  
  fmt::print("Running `{}`\\n", executable_path.filename().string());
  
  // Build command to execute
  std::string run_command = quote_path(executable_path.string());
  for (const auto& arg : run_args) {
    run_command += " " + quote_path(arg);
  }
  
  // Execute the program
  const int run_result = std::system(run_command.c_str());  // NOLINT(cert-env33-c,concurrency-mt-unsafe)
  return run_result;
}

// Handler for build subcommand
int handle_build_command(bool build_release) {
  fmt::print("Configuring project...\\n");
  fmt::print("Compiling...\\n");
  
  const auto [build_result, executable_path] = build_project(build_release);
  if (build_result != EXIT_SUCCESS) {
    return build_result;
  }
  
  const std::string target_subdir = build_release ? "release" : "debug";
  const std::string build_mode = build_release ? "Release" : "Debug";
  
  if (std::filesystem::exists(executable_path)) {
    fmt::print("Finished {} [{}] target(s) in target/{}/\\n", 
              build_release ? "release" : "debug",
              build_mode, 
              target_subdir);
  } else {
    fmt::print("Warning: Executable not found at expected location\\n");
  }
  
  return EXIT_SUCCESS;
}

// Handler for new subcommand
int handle_new_command(const std::string& new_project_name) {
  const std::filesystem::path project_dir = std::filesystem::current_path() / new_project_name;
  
  // Check if directory already exists
  if (std::filesystem::exists(project_dir)) {
    fmt::print("Directory '{}' already exists\\n", new_project_name);
    return EXIT_FAILURE;
  }
  
  try {
    // Create project directory
    std::filesystem::create_directory(project_dir);
    
    // Create src directory
    const std::filesystem::path src_dir = project_dir / "src";
    std::filesystem::create_directory(src_dir);
    
    // Create Sail.toml
    const std::filesystem::path sail_toml_path = project_dir / "Sail.toml";
    const std::string sail_toml_content = fmt::format(R"([project]
name = "{}"
version = "0.1.0"

[dependencies]
)", new_project_name);
    
    std::ofstream toml_file(sail_toml_path);
    if (!toml_file) {
      fmt::print("Failed to create Sail.toml\\n");
      return EXIT_FAILURE;
    }
    toml_file << sail_toml_content;
    
    // Create main.cpp
    const std::filesystem::path main_cpp_path = src_dir / "main.cpp";
    constexpr std::string_view main_cpp_content = R"(#include <iostream>

int main() {
    std::cout << "Hello, World!" << std::endl;
    return 0;
}
)";
    
    std::ofstream cpp_file(main_cpp_path);
    if (!cpp_file) {
      fmt::print("Failed to create src/main.cpp\\n");
      return EXIT_FAILURE;
    }
    cpp_file << main_cpp_content;
    
    fmt::print("Created project '{}'\\n", new_project_name);
    return EXIT_SUCCESS;
    
  } catch (const std::filesystem::filesystem_error& e) {
    fmt::print("Failed to create project: {}\\n", e.what());
    return EXIT_FAILURE;
  }
}

// Handler for init subcommand
int handle_init_command() {
  // Get current folder name as project name
  const std::string project_name = std::filesystem::current_path().filename().string();
  
  const std::string sail_toml_content = fmt::format(R"([project]
name = "{}"
version = "0.1.0"

[dependencies]
)", project_name);
  
  const std::filesystem::path sail_toml_path = std::filesystem::current_path() / "Sail.toml";
  
  if (std::filesystem::exists(sail_toml_path)) {
    fmt::print("Sail.toml already exists in current directory\\n");
    return EXIT_FAILURE;
  }
  
  std::ofstream file(sail_toml_path);
  if (!file) {
    fmt::print("Failed to create Sail.toml\\n");
    return EXIT_FAILURE;
  }
  
  file << sail_toml_content;
  fmt::print("Created Sail.toml\\n");
  return EXIT_SUCCESS;
}

// NOLINTNEXTLINE(bugprone-exception-escape)
int main(int argc, const char **argv)
{
  try {
    CLI::App app{ fmt::format("{} version {}", sail::cmake::project_name, sail::cmake::project_version) };

    std::optional<std::string> message;
    app.add_option("-m,--message", message, "A message to print back out");
    bool show_version = false;
    app.add_flag("--version", show_version, "Show version information");

    bool is_turn_based = false;
    auto *turn_based = app.add_flag("--turn_based", is_turn_based);

    bool is_loop_based = false;
    auto *loop_based = app.add_flag("--loop_based", is_loop_based);

    turn_based->excludes(loop_based);
    loop_based->excludes(turn_based);

    auto* init_subcommand = app.add_subcommand("init", "Initialize a new Sail project");
    auto* new_subcommand = app.add_subcommand("new", "Create a new Sail project");
    auto* build_subcommand = app.add_subcommand("build", "Compile the current project");
    auto* run_subcommand = app.add_subcommand("run", "Run the current project");
    
    std::string new_project_name;
    new_subcommand->add_option("name", new_project_name, "Project name")->required();
    
    bool build_release = false;
    build_subcommand->add_flag("--release", build_release, "Build in release mode");
    
    bool run_release = false;
    std::vector<std::string> run_args;
    run_subcommand->add_flag("--release", run_release, "Run in release mode");
    
    // Set up command handlers using the extracted functions
    init_subcommand->callback([&]() { return handle_init_command(); });
    new_subcommand->callback([&]() { return handle_new_command(new_project_name); });
    build_subcommand->callback([&]() { return handle_build_command(build_release); });
    run_subcommand->callback([&]() { return handle_run_command(run_release, run_args); });

    CLI11_PARSE(app, argc, argv);

    if (show_version) {
      fmt::print("{}\n", sail::cmake::project_version);
      return EXIT_SUCCESS;
    }
    
    if (init_subcommand->parsed() || new_subcommand->parsed() || build_subcommand->parsed() || run_subcommand->parsed()) {
      return EXIT_SUCCESS;
    }

    return EXIT_SUCCESS;
  } catch (const std::exception &e) {
    spdlog::error("Unhandled exception in main: {}", e.what());
    return EXIT_FAILURE;
  }
}
