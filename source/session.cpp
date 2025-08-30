#include <rtdxc/rtdxc.hpp>

#include <fstream>

namespace rtdxc {

session::session(
    const daw_version version,
    const std::filesystem::path& daw_path, 
    const std::filesystem::path& container_path)
    : _process(daw_path)
    , _watcher(container_path)
{
    std::ifstream _dxcc_stream(container_path, std::ios::binary);
    fmtdxc::version _dxcc_version;
    fmtdxc::project_container _container;
    fmtdxc::import_container(_dxcc_stream, _container, _dxcc_version);
    std::filesystem::path _temp_dir_path = "C:/Users/adri/Desktop/temp";
    // std::filesystem::path _temp_dir_path = std::filesystem::temp_directory_path();
    std::filesystem::path _daw_project_path;

    std::visit([&](const auto _version) {
        using daw_type_t = std::decay_t<decltype(_version)>;

        if constexpr (std::is_same_v<daw_type_t, fmtals::version>) {
            fmtals::project _als_project = detail::convert_to_als(_container.get_project());
            _daw_project_path = _temp_dir_path / "dawxchange_session.als";
            std::ofstream _als_stream(_daw_project_path, std::ios::binary);
            fmtals::export_project(_als_stream, _als_project, _version);
        }   
    },
        version);

    _process.load_native_project(_daw_project_path);
}

bool session::can_undo() const
{
    return _project_container.can_undo();
}

bool session::can_redo() const
{
    return _project_container.can_redo();
}

std::size_t session::get_applied_count() const
{
    return _project_container.get_applied_count();
}

const std::vector<fmtdxc::project_commit>& session::get_commits() const
{
    return _project_container.get_commits();
}

const fmtdxc::sparse_project& session::get_diff_from_last_commit() const
{
    return _next_project_diff;
}

void session::commit(const std::string& message)
{
    _project_container.commit(message, _next_project);
}

void session::undo()
{
    _project_container.undo();
}

void session::redo()
{
    _project_container.redo();
}

}