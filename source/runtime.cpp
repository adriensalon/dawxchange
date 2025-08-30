#include <rtdxc/rtdxc.hpp>

namespace rtdxc {

runtime::runtime()
    : _session(nullptr)
    , _p2p_session(nullptr)
{
}

bool runtime::is_session_open() const
{
    return _session.get();
}

session& runtime::open_linked_session(
    const daw_version version,
    const std::filesystem::path& daw_path,
    const std::filesystem::path& container_path)
{
    _session = std::make_unique<session>(version, daw_path, container_path);
    return *(_session.get());
}

// session& runtime::open_temp_session(const std::filesystem::path& native_program_path, const std::function<std::optional<std::filesystem::path>()>& close_callback)
// {
//     _session = std::make_unique<session>(native_program_path, close_callback);
//     return *(_session.get());
// }
//
// session& runtime::open_temp_session_from_template(const std::filesystem::path& native_program_path, const std::filesystem::path& container_path, const std::function<std::optional<std::filesystem::path>()>& close_callback)
// {
//     _session = std::make_unique<session>(native_program_path, container_path, close_callback);
//     return *(_session.get());
// }

void runtime::close_session()
{
    if (!is_session_open()) {
        throw std::runtime_error("Trying to close a session but no session is open");
    }
    if (_session) {
        _session.reset();
    }
}

}