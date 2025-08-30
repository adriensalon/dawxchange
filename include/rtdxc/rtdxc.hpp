#pragma once

#include <fmtals/fmtals.hpp>
#include <fmtdxc/fmtdxc.hpp>

#include <functional>
#include <memory>
#include <unordered_map>
#include <unordered_set>

namespace rtdxc {

namespace detail {

    /// @brief
    /// @param native_project
    /// @return
    [[nodiscard]] fmtdxc::project convert_from_als(const fmtals::project& native_project);

    /// @brief
    /// @param native_project
    /// @return
    [[nodiscard]] fmtals::project convert_to_als(const fmtdxc::project& proj);

    /// @brief
    struct process {
        process() = delete;
        process(const std::filesystem::path& native_program_path);
        process(const process& other) = delete;
        process& operator=(const process& other) = delete;
        process(process&& other) = default;
        process& operator=(process&& other) = default;
        ~process() noexcept;

        [[nodiscard]] bool is_terminated() const;
        void load_native_project(const std::filesystem::path& native_project_path);
        void save_native_project();
        void save_native_project_as(const std::filesystem::path& native_project_path);

    private:
        std::shared_ptr<struct process_impl> _impl;
    };

    /// @brief
    struct file_watcher {
        file_watcher() = delete;
        file_watcher(const std::filesystem::path& file_path);
        file_watcher(const file_watcher& other) = delete;
        file_watcher& operator=(const file_watcher& other) = delete;
        file_watcher(file_watcher&& other) = default;
        file_watcher& operator=(file_watcher&& other) = default;

        void on_modification(const std::function<void(const std::filesystem::path& file_path)>& callback);

    private:
        std::shared_ptr<struct file_watcher_impl> _impl;
    };

    struct directory_watcher {
        directory_watcher() = delete;
        directory_watcher(const std::filesystem::path& directory_path);
        directory_watcher(const directory_watcher& other) = delete;
        directory_watcher& operator=(const directory_watcher& other) = delete;
        directory_watcher(directory_watcher&& other) = default;
        directory_watcher& operator=(directory_watcher&& other) = default;

        void on_modification(const std::function<void(const std::filesystem::path& file_path)>& callback);
        void on_creation(const std::function<void(const std::filesystem::path& file_path)>& callback);
        void on_removal(const std::function<void(const std::filesystem::path& file_path)>& callback);

    private:
        std::shared_ptr<struct directory_watcher_impl> _impl;
    };
}

/// @brief launches process on it and on modification updates sparse diff
struct session {
    session() = delete;
    session(const std::filesystem::path& native_program_path, const std::filesystem::path& container_path);
    session(const session& other) = delete;
    session& operator=(const session& other) = delete;
    session(session&& other) = default;
    session& operator=(session&& other) = default;
    ~session();

    [[nodiscard]] bool is_terminated() const;
    [[nodiscard]] bool can_undo() const;
    [[nodiscard]] bool can_redo() const;
    [[nodiscard]] std::size_t get_applied_count() const;
    [[nodiscard]] const std::vector<fmtdxc::project_commit>& get_commits() const;
    [[nodiscard]] const fmtdxc::sparse_project& get_diff_from_last_commit() const;
    [[nodiscard]] const fmtdxc::project_info& get_info() const;
    void commit(const std::string& message);
    void undo();
    void redo();
    
private:
    bool _is_terminated;
    std::function<std::filesystem::path()> _close_callback;
    fmtdxc::project_container _project_container;
    std::variant<fmtals::version> _native_version;
    std::filesystem::path _working_directory;
    fmtdxc::sparse_project _next_project_diff; // for ui
    fmtdxc::project _next_project;
    detail::process _process;
    detail::file_watcher _watcher;
};

struct p2p_connexion {
    bool is_host;
};

/// @brief
struct p2p_session {
    p2p_session() = delete;
    p2p_session(const std::filesystem::path& native_program_path, const fmtdxc::project_container& container, const p2p_connexion& connexion);
    p2p_session(const p2p_session& other) = delete;
    p2p_session& operator=(const p2p_session& other) = delete;
    p2p_session(p2p_session&& other) = default;
    p2p_session& operator=(p2p_session&& other) = default;

    [[nodiscard]] const std::vector<fmtdxc::project_commit>& get_commits() const;
    [[nodiscard]] const fmtdxc::sparse_project& get_diff_from_last_commit() const;
    [[nodiscard]] const p2p_connexion& get_connexion() const;
    void commit(const std::string& message);

private:
    session _session;
    p2p_connexion _connexion;
};

/// @brief
struct runtime {
    runtime();
    runtime(const runtime& other) = delete;
    runtime& operator=(const runtime& other) = delete;
    runtime(runtime&& other) = default;
    runtime& operator=(runtime&& other) = default;

    [[nodiscard]] bool is_session_open() const;
    session& open_linked_session(const std::filesystem::path& native_program_path, const std::filesystem::path& container_path); // open (REQ)
    session& open_temp_session(const std::filesystem::path& native_program_path, const std::function<std::optional<std::filesystem::path>()>& close_callback); // new
    session& open_temp_session_from_template(const std::filesystem::path& native_program_path, const std::filesystem::path& container_path, const std::function<std::optional<std::filesystem::path>()>& close_callback); // new from template (REQ)
    // void open_linked_p2p_host_session(const std::filesystem::path& native_program_path, const std::filesystem::path& container_path); // open as p2p host (REQ)
    // void open_temp_p2p_host_session(const std::filesystem::path& native_program_path, const std::function<std::optional<std::filesystem::path>()>& close_callback); // new as p2p host
    // void open_temp_p2p_client_session(const std::filesystem::path& native_program_path, const std::function<std::optional<std::filesystem::path>()>& close_callback); // join as p2p client
    void close_session();
    
private:
    std::unique_ptr<session> _session;
    std::unique_ptr<p2p_session> _p2p_session;
};

}

// import
// export (REQ)
// new
//      new
//      new from template (REQ)
//      new as p2p host
//      join as p2p client
// open
//      open (REQ)
//      open as p2p host (REQ)