#pragma once

#include <fmtals/fmtals.hpp>
#include <fmtdxc/fmtdxc.hpp>

#include <filesystem>
#include <memory>

namespace dxc {
namespace detail {

    /// @brief
    struct process {
        process() = delete;
        process(const std::filesystem::path& native_program_path);
        process(const process& other) = delete;
        process& operator=(const process& other) = delete;
        process(process&& other) = default;
        process& operator=(process&& other) = default;
        ~process() noexcept;

        void load_native_project(const std::filesystem::path& native_project_path);
        void save_native_project();
        void save_native_project_as(const std::filesystem::path& native_project_path);

    private:
        std::shared_ptr<struct session_impl> _impl;
    };

    /// @brief
    struct watcher {
        watcher() = delete;
        watcher(const std::filesystem::path& native_project_path);
        watcher(const watcher& other) = delete;
        watcher& operator=(const watcher& other) = delete;
        watcher(watcher&& other) = default;
        watcher& operator=(watcher&& other) = default;

        [[nodiscard]] const fmtdxc::project& get_next_project() const;

    private:
        std::shared_ptr<struct watcher_impl> _impl;
    };
}

/// @brief
struct session {
    session() = delete;
    session(const std::filesystem::path& native_program_path, const fmtdxc::project_info& info);
    session(const session& other) = delete;
    session& operator=(const session& other) = delete;
    session(session&& other) = default;
    session& operator=(session&& other) = default;

    void commit(const std::string& message);
    void undo();
    void redo();
    [[nodiscard]] bool can_undo() const;
    [[nodiscard]] bool can_redo() const;
    [[nodiscard]] std::size_t get_applied_count() const;
    [[nodiscard]] const std::vector<fmtdxc::project_commit>& get_commits() const;
    [[nodiscard]] const fmtdxc::sparse_project& get_diff_from_last_commit() const;

private:
    std::filesystem::path _working_directory;
    fmtdxc::project_container _project_container;
    fmtdxc::sparse_project _next_project_diff;
    detail::process _process;
    detail::watcher _watcher;
};

/// @brief
struct configuration {
    std::uint32_t max_open_sessions = 1;
    std::vector<std::filesystem::path> collection_roots;
};

/// @brief
struct runtime {
    runtime(const configuration& config);

    [[nodiscard]] const std::vector<fmtdxc::project_info>& get_collection() const;
    [[nodiscard]] bool get_sessions_open_count();
    [[nodiscard]] const session& open_session(const std::size_t index);
    void close_session();

private:
    std::vector<fmtdxc::project_info> _collection;
    std::unique_ptr<session> _session;
};

}