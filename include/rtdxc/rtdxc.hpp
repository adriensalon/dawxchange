#pragma once

#include <fmtals/fmtals.hpp>
#include <fmtdxc/fmtdxc.hpp>

#include <filesystem>
#include <functional>
#include <memory>
#include <unordered_map>

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
        process(const fmtals::version als_version, const std::filesystem::path& als_program_path);
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

/// @brief
struct session {
    session() = delete;
    session(const fmtals::version als_version, const std::filesystem::path& als_program_path, const fmtdxc::project_container& container);
    session(const session& other) = delete;
    session& operator=(const session& other) = delete;
    session(session&& other) = default;
    session& operator=(session&& other) = default;

    [[nodiscard]] bool can_undo() const;
    [[nodiscard]] bool can_redo() const;
    [[nodiscard]] std::size_t get_applied_count() const;
    [[nodiscard]] const std::vector<fmtdxc::project_commit>& get_commits() const;
    [[nodiscard]] const fmtdxc::sparse_project& get_diff_from_last_commit() const;
    void commit(const std::string& message);
    void undo();
    void redo();

private:
    std::filesystem::path _working_directory;
    fmtdxc::project_container _project_container;
    fmtdxc::sparse_project _next_project_diff;
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
    p2p_session(const fmtals::version als_version, const std::filesystem::path& als_program_path, const fmtdxc::project_container& container, const p2p_connexion& connexion);
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
struct collection {

    collection(const std::filesystem::path& root_directory);

    using project_id = std::uint64_t;

    enum struct sort_field {
        name,
        created,
        last_modified,
        ppq,
        commits_applied,
    };

    [[nodiscard]] const fmtdxc::project_info& at(const project_id index);
    void sort(const sort_field& field);
    void sort(const std::initializer_list<sort_field>& fields);
    void foreach (const std::function<void(const project_id, const fmtdxc::project_info&)>& callback);
    void reset(const std::vector<std::filesystem::path>& roots);

private:
    detail::directory_watcher _watcher;
    std::vector<std::filesystem::path> _roots;
    std::map<project_id, fmtdxc::project_info> _infos;
};

/// @brief
struct runtime {
    runtime(const std::filesystem::path& root_directory);

    [[nodiscard]] const collection& get_collection() const;
    [[nodiscard]] std::vector<collection::project_id> get_open_sessions();
    [[nodiscard]] std::vector<collection::project_id> get_open_p2p_sessions();
    [[nodiscard]] const session& open_session_as_als();
    [[nodiscard]] const session& open_session_as_flp();
    [[nodiscard]] const session& open_session_as_als(const collection::project_id id);
    [[nodiscard]] const session& open_session_as_flp(const collection::project_id id);
    [[nodiscard]] const p2p_session& open_p2p_session_as_als(const collection::project_id id, const p2p_connexion connexion);
    [[nodiscard]] const p2p_session& open_p2p_session_as_flp(const collection::project_id id, const p2p_connexion connexion);
    void close_session(const collection::project_id id);
    void close_p2p_session(const collection::project_id id);
    collection::project_id import_project_from_als(const std::filesystem::path& native_project_path, fmtals::version& ver);
    void export_project_to_als(const std::size_t id, const std::filesystem::path& native_project_path, const fmtals::version ver);

private:
    collection _collection;
    std::unordered_map<collection::project_id, session> _sessions;
    std::unordered_map<collection::project_id, p2p_session> _p2p_sessions;
};

}