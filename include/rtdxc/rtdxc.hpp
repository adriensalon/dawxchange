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

struct watcher {


};

struct collection {



};

struct session {
    session(const std::filesystem::path& program_path, const std::filesystem::path& working_directory);
    session(const std::filesystem::path& program_path, const std::filesystem::path& working_directory, const std::filesystem::path& project_path);


private:
    process _process;
    watcher _watcher;
};

struct runtime {
    

private:
    std::shared_ptr<session> _session;
    collection _collection;
};

struct client {

    void exit();

private:
    std::shared_ptr<struct client_impl> _impl;
};


}