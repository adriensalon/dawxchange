#include <rtdxc/rtdxc.hpp>

#include <enet/enet.h>

#include <fstream>
#include <thread>

namespace rtdxc {
namespace {
    struct enet_lib {
        enet_lib() { enet_initialize(); }
        ~enet_lib() { enet_deinitialize(); }
    };

    enum struct wire_type : std::uint8_t {
        join = 1, // C->H : request to join
        join_container = 2, // H->C : full project_container blob (on join)
        commit_request = 3, // C->H : client requests to commit (payload = message + optional diff)
        commit_broadcast = 4, // H->* : authoritative commit from host (payload = project_commit or minimal wire form)
        undo_broadcast = 7, // H->* : host performed undo
        redo_broadcast = 8, // H->* : host performed redo
        ping = 9, // keepalive if you want
    };

    struct wire_peer {
        std::string username;
        daw_version version;

        template <typename archive_t>
        void serialize(archive_t& archive)
        {
            archive(username);
            archive(version);
        }
    };

    struct wire_join {
        // wire_peer;

        template <typename archive_t>
        void serialize(archive_t& archive)
        {
            archive(version);
        }
    };

    struct wire_join_container {
        fmtdxc::project_container container;

        template <typename archive_t>
        void serialize(archive_t& archive)
        {
            archive(container);
        }
    };

    struct wire_commit_request {
        fmtdxc::project_commit commit;
        
        template <typename archive_t>
        void serialize(archive_t& archive)
        {
            archive(commit);
        }
    };

}

local_session::local_session(
    const daw_version version,
    const std::filesystem::path& daw_path,
    const std::optional<std::filesystem::path>& container_path,
    const std::function<std::optional<std::filesystem::path>()>& exit_callback)
    : _daw_version(version)
    // , _temp_directory_path(std::filesystem::temp_directory_path())
    , _temp_directory_path("C:\\Users\\adri\\Desktop\\temp") // LOOOL
{
    if (!std::filesystem::exists(daw_path)) {
        throw std::invalid_argument("DAW path provided to session does not exist");
    }
    if (container_path) {
        if (!std::filesystem::exists(container_path.value()) || container_path.value().extension() != ".dxcc") {
            throw std::invalid_argument("Container path provided to session does not exist or is not a dxcc file");
        }
    }
    if (!exit_callback) {
        throw std::invalid_argument("Close callback provided to session is nullptr");
    }

    fmtdxc::version _dxcc_version;
    if (container_path) {
        std::ifstream _dxcc_stream(container_path.value(), std::ios::binary);
        fmtdxc::import_container(_dxcc_stream, _container, _dxcc_version);
    }

    //
    //
    //
    //

    std::filesystem::path _daw_temp_project_path;
    std::visit([&](const auto _version) {
        using daw_type_t = std::decay_t<decltype(_version)>;

        // ableton
        if constexpr (std::is_same_v<daw_type_t, fmtals::version>) {
            _daw_temp_project_path = _temp_directory_path / "dawxchange.als";
        }
    },
        _daw_version);

    //
    //
    //
    //

    if (container_path) {
        std::visit([&](const auto _version) {
            using daw_type_t = std::decay_t<decltype(_version)>;

            // ableton
            if constexpr (std::is_same_v<daw_type_t, fmtals::version>) {
                fmtals::project _als_project = detail::convert_to_als(_container.get_project());
                std::ofstream _als_stream(_daw_temp_project_path, std::ios::binary);

                // TODO export in parent folder Project
                fmtals::export_project(_als_stream, _als_project, _version);
            }
        },
            _daw_version);
    }

    _daw_process = std::make_unique<detail::process>(daw_path);

    if (container_path) {
        _daw_process->load_daw_project(_daw_temp_project_path);
    } else {
        _daw_process->save_daw_project_as(_daw_temp_project_path);
    }

    // _daw_process->on_exit([this] () {
    //     std::optional<std::filesystem::path> _output_container_path = _exit_callback();
    //     if (_output_container_path) {
    //         std::ofstream _output_dxcc_stream(_output_container_path.value(), std::ios::binary);
    //         fmtdxc::export_container(_output_dxcc_stream, _container, fmtdxc::version::alpha);
    //     }
    // });

    // _daw_temp_project_watcher = std::make_unique<detail::file_watcher>(_daw_temp_project_path);
    _daw_temp_project_watcher = std::make_unique<detail::file_watcher>(_temp_directory_path / "dawxchange Project" / "dawxchange.als");
    std::filesystem::path _ok = _temp_directory_path;
    _daw_temp_project_watcher->on_modification([this, _ok](const std::filesystem::path&) {
        std::cout << _ok / "dawxchange Project" / "dawxchange.als" << std::endl;

        std::visit([&](const auto _version) {
            using daw_type_t = std::decay_t<decltype(_version)>;

            // ableton
            if constexpr (std::is_same_v<daw_type_t, fmtals::version>) {
                std::cout << _ok / "dawxchange Project" / "dawxchange.als" << std::endl;
                std::ifstream _als_stream(_ok / "dawxchange Project" / "dawxchange.als", std::ios::binary);
                std::cout << _als_stream.is_open() << std::endl;
                fmtals::version _als_version;
                fmtals::project _als_project;
                fmtals::import_project(_als_stream, _als_project, _als_version);
                // _next_proj = detail::convert_from_als(_als_project);
            }
        },
            _daw_version);

        // fmtdxc::diff(_container.get_project(), _next_proj, _next_diff);
        std::cout << "modified ::::) " << std::endl;
    });
}

bool local_session::can_commit() const
{
    return true; // TODO is next_dif empty
}

bool local_session::can_undo() const
{
    return _container.can_undo();
}

bool local_session::can_redo() const
{
    return _container.can_redo();
}

std::size_t local_session::get_applied_count() const
{
    return _container.get_applied_count();
}

const std::vector<fmtdxc::project_commit>& local_session::get_commits() const
{
    return _container.get_commits();
}

const fmtdxc::sparse_project& local_session::get_diff_from_last_commit() const
{
    return _next_diff;
}

const std::filesystem::path& local_session::get_temp_directory_path() const
{
    return _temp_directory_path;
}

void local_session::commit(const std::string& message)
{
    _container.commit(message, _next_proj);
}

void local_session::undo()
{
    _container.undo();
}

void local_session::redo()
{
    _container.redo();
}
}