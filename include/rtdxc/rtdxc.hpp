#pragma once

#include <fmtals/fmtals.hpp>
#include <fmtdxc/fmtdxc.hpp>
#include <natp2p/natp2p.hpp>

#include <functional>
#include <memory>

namespace rtdxc {

namespace detail {

    /// @brief
    /// @param daw_project
    /// @return
    [[nodiscard]] fmtdxc::project convert_from_als(const fmtals::project& daw_project);

    /// @brief
    /// @param daw_project
    /// @return
    [[nodiscard]] fmtals::project convert_to_als(const fmtdxc::project& proj);

    /// @brief
    struct process {
        process() = delete;
        process(const std::filesystem::path& daw_path);
        process(const process& other) = delete;
        process& operator=(const process& other) = delete;
        process(process&& other) noexcept = default;
        process& operator=(process&& other) noexcept = default;
        ~process() noexcept;

        void on_exit(const std::function<void()>& exit_callback); // TODO
        void load_daw_project(const std::filesystem::path& daw_project_path);
        void save_daw_project();
        void save_daw_project_as(const std::filesystem::path& daw_project_path);

    private:
        std::shared_ptr<struct process_impl> _impl;
    };

    /// @brief
    struct file_watcher {
        file_watcher() = delete;
        file_watcher(const std::filesystem::path& file_path);
        file_watcher(const file_watcher& other) = delete;
        file_watcher& operator=(const file_watcher& other) = delete;
        file_watcher(file_watcher&& other) noexcept = default;
        file_watcher& operator=(file_watcher&& other) noexcept = default;

        void on_modification(const std::function<void(const std::filesystem::path& file_path)>& callback);

    private:
        std::shared_ptr<struct file_watcher_impl> _impl;
    };

    /// @brief
    struct directory_watcher {
        directory_watcher() = delete;
        directory_watcher(const std::filesystem::path& directory_path);
        directory_watcher(const directory_watcher& other) = delete;
        directory_watcher& operator=(const directory_watcher& other) = delete;
        directory_watcher(directory_watcher&& other) noexcept = default;
        directory_watcher& operator=(directory_watcher&& other) noexcept = default;

        void on_modification(const std::function<void(const std::filesystem::path& file_path)>& callback);
        void on_creation(const std::function<void(const std::filesystem::path& file_path)>& callback);
        void on_removal(const std::function<void(const std::filesystem::path& file_path)>& callback);

    private:
        std::shared_ptr<struct directory_watcher_impl> _impl;
    };

    /// @brief
    struct p2p_peer_info {
        std::string remote_ip;
        std::uint16_t remote_port;
        std::chrono::steady_clock::time_point last_seen;
        std::uint64_t received_bytes;
        std::uint64_t sent_bytes;
    };

    /// @brief
    using p2p_client_id = std::uint32_t;

    /// @brief
    struct p2p_host {
        p2p_host() = delete;
        p2p_host(const natp2p::endpoint_lease& host_endpoint);
        p2p_host(const p2p_host& other) = delete;
        p2p_host& operator=(const p2p_host& other) = delete;
        p2p_host(p2p_host&& other) noexcept = default;
        p2p_host& operator=(p2p_host&& other) noexcept = default;
        // ~p2p_host() noexcept;

        [[nodiscard]] std::vector<p2p_client_id> get_clients() const;
        [[nodiscard]] p2p_peer_info get_client_info(const p2p_client_id id) const;
        void add_manual_ipv4_endpoint(const std::string& ipv4, const std::uint16_t port);
        void disconnect(const p2p_client_id id);
        void send(const p2p_client_id id, const std::vector<std::uint8_t>& payload);
        void broadcast(const std::vector<std::uint8_t>& payload);
        void on_connect(const std::function<void(p2p_client_id)>& connect_callback);
        void on_rebind(const std::function<void(p2p_client_id, const p2p_peer_info&)>& rebind_callback);
        void on_disconnect(const std::function<void(p2p_client_id)>& disconnect_callback);
        void on_receive(const std::function<void(p2p_client_id, const std::vector<std::uint8_t>&)>& receive_callback);

    private:
        std::shared_ptr<struct host_session_impl> _impl;
    };

    /// @brief
    struct p2p_client {
        p2p_client() = delete;
        p2p_client(const natp2p::endpoint_data& host_endpoint);
        p2p_client(const p2p_client& other) = delete;
        p2p_client& operator=(const p2p_client& other) = delete;
        p2p_client(p2p_client&& other) noexcept = default;
        p2p_client& operator=(p2p_client&& other) noexcept = default;
        // ~p2p_client() noexcept;

        [[nodiscard]] p2p_peer_info get_host_info() const;
        void send(const std::vector<std::uint8_t>& payload);
        void on_disconnect(const std::function<void()>& disconnect_callback);
        void on_receive(const std::function<void(const std::vector<std::uint8_t>&)>& receive_callback);

    private:
        std::shared_ptr<struct client_session_impl> _impl;
    };

}

/// @brief
using daw_version = std::variant<fmtals::version, int>;

/// @brief launches process on it and on modification updates sparse diff
struct local_session {
    local_session() = delete;
    local_session(
        const daw_version version,
        const std::filesystem::path& daw_path,
        const std::optional<std::filesystem::path>& container_path,
        const std::function<std::optional<std::filesystem::path>()>& exit_callback);
    local_session(const local_session& other) = delete;
    local_session& operator=(const local_session& other) = delete;
    local_session(local_session&& other) = default;
    local_session& operator=(local_session&& other) = default;

    [[nodiscard]] bool can_commit() const;
    [[nodiscard]] bool can_undo() const;
    [[nodiscard]] bool can_redo() const;
    [[nodiscard]] std::size_t get_applied_count() const;
    [[nodiscard]] const std::vector<fmtdxc::project_commit>& get_commits() const;
    [[nodiscard]] const fmtdxc::sparse_project& get_diff_from_last_commit() const;
    [[nodiscard]] const std::filesystem::path& get_temp_directory_path() const;
    void commit(const std::string& message);
    void undo();
    void redo();

private:
    daw_version _daw_version;
    std::filesystem::path _temp_directory_path;
    fmtdxc::project_container _container;
    fmtdxc::sparse_project _next_diff; // for ui
    fmtdxc::project _next_proj;
    std::unique_ptr<detail::process> _daw_process;
    std::unique_ptr<detail::file_watcher> _daw_temp_project_watcher;
};

/// @brief
struct p2p_host_session {
    p2p_host_session() = delete;
    p2p_host_session(
        const daw_version version,
        const std::filesystem::path& daw_path,
        const std::optional<std::filesystem::path>& container_path,
        const std::function<std::optional<std::filesystem::path>()>& exit_callback,
        const natp2p::endpoint_lease& host_endpoint);
    p2p_host_session(const p2p_host_session& other) = delete;
    p2p_host_session& operator=(const p2p_host_session& other) = delete;
    p2p_host_session(p2p_host_session&& other) = default;
    p2p_host_session& operator=(p2p_host_session&& other) = default;

    [[nodiscard]] bool can_commit() const;
    [[nodiscard]] bool can_undo() const;
    [[nodiscard]] bool can_redo() const;
    [[nodiscard]] std::size_t get_applied_count() const;
    [[nodiscard]] const std::vector<fmtdxc::project_commit>& get_commits() const;
    [[nodiscard]] const fmtdxc::sparse_project& get_diff_from_last_commit() const;
    [[nodiscard]] const std::filesystem::path& get_temp_directory_path() const;
    void commit(const std::string& message);
    void undo();
    void redo();

private:
    detail::p2p_host _host;
    local_session _local_session;
};

/// @brief
struct p2p_client_session {
    p2p_client_session() = delete;
    p2p_client_session(
        const daw_version version,
        const std::filesystem::path& daw_path,
        const std::function<std::optional<std::filesystem::path>()>& exit_callback,
        const natp2p::endpoint_data& host_endpoint);
    p2p_client_session(const p2p_client_session& other) = delete;
    p2p_client_session& operator=(const p2p_client_session& other) = delete;
    p2p_client_session(p2p_client_session&& other) = default;
    p2p_client_session& operator=(p2p_client_session&& other) = default;

    [[nodiscard]] std::size_t get_applied_count() const;
    [[nodiscard]] const std::vector<fmtdxc::project_commit>& get_commits() const;
    [[nodiscard]] const fmtdxc::sparse_project& get_diff_from_last_commit() const;
    [[nodiscard]] const std::filesystem::path& get_temp_directory_path() const;
    void commit(const std::string& message);

private:
    detail::p2p_client _client;
    local_session _local_session;
};

/// @brief
using session = std::variant<local_session, p2p_host_session, p2p_client_session>;

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