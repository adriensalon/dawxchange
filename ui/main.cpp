#include <rtdxc/rtdxc.hpp>

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
    void reset(const std::filesystem::path& root_directory);

    collection::project_id import_project(const std::filesystem::path& native_project_path);
    void export_project(const collection::project_id id, const std::filesystem::path& native_project_path);

private:
    rtdxc::detail::directory_watcher _watcher;
    std::vector<std::filesystem::path> _roots;
    std::map<project_id, fmtdxc::project_info> _infos;
    std::unordered_set<project_id> _new_projects;
};

static rtdxc::runtime _runtime;

int main()
{

    while (true) {


        if (false) {
            _runtime.open_temp_session("ableton.exe", [] () {
                return "new container path.dxcc";
            });
        }


    }
}