#include <rtdxc/rtdxc.hpp>

#include <fstream>

int main(int argc, char* argv[])
{
    // if (argc > 3 || argc < 2) {
    //     std::cout << "Usage: als2dxcc <input.als> [output.dxcc]" << std::endl;
    //     return 1;
    // }

    // std::filesystem::path _als_path(argv[1]);
    // if (!std::filesystem::exists(_als_path)) {
    //     std::cerr << "Error: Ableton Live Set " << _als_path << " does not exist" << std::endl;
    //     return 2;
    // }

    // std::filesystem::path _dxcc_path;
    // if (argc == 2) {
    //     _dxcc_path = _als_path;
    //     _dxcc_path.replace_extension(".dxcc");
    // } else {
    //     _dxcc_path = argv[2];
    // }

    std::filesystem::path _dxcc_path("C:/Users/adri/Desktop/untitled.json");
    std::filesystem::path _als_path("C:/Users/adri/Desktop/untitled2.als");

    std::ifstream _dxcc_stream(_dxcc_path, std::ios::binary);
    std::ofstream _als_stream(_als_path, std::ios::binary);
    bool _as_json = _dxcc_path.extension() == ".json";
    if (!_as_json) {
        _dxcc_path.replace_extension(".dxcc");
    }

    // try {
        fmtdxc::version _dxcc_version;
        fmtdxc::project_container _container;
        fmtdxc::import_container(_dxcc_stream, _container, _dxcc_version, _as_json);
        fmtals::project _als_project = rtdxc::detail::convert_to_als(_container.get_project());
        fmtals::version _als_version = fmtals::version::v_9_0_0;
        fmtals::export_project(_als_stream, _als_project, _als_version);
    // } catch (const std::exception& e) {
    //     std::cerr << e.what() << '\n';
    // }
}