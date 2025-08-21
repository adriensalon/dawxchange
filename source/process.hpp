#pragma once

#include <filesystem>
#include <memory>

namespace dxc {

struct process {
    process() = delete;
    process(const std::filesystem::path& program);
    process(const process& other) = delete;
    process& operator=(const process& other) = delete;
    process(process&& other) = default;
    process& operator=(process&& other) = default;
    ~process() noexcept;
    
    void load_project(const std::filesystem::path& project);
    void save_project();
    void save_project_as(const std::filesystem::path& project);

private:
    std::shared_ptr<struct session_impl> _impl;
};

struct file_watcher {


};

struct session {
    session(const std::filesystem::path& program_path, const std::filesystem::path& working_directory);
    session(const std::filesystem::path& program_path, const std::filesystem::path& working_directory, const std::filesystem::path& project_path);

    // ne fait rien, gere le processus tout seul

    void on_save(const std::function<void(const dxc::project&)>& save_callback)

private:
    process _process;
    file_watcher _file_watcher;
};

}