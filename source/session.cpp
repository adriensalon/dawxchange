#include <rtdxc/rtdxc.hpp>

namespace rtdxc {

session::session(const std::filesystem::path& native_program_path, const std::filesystem::path& container_path)
    : _process(native_program_path)
    , _watcher(container_path)
{
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