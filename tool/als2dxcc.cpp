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

    std::filesystem::path _als_path("C:/Users/adri/Desktop/Untitled.als");
    std::filesystem::path _dxcc_path("C:/Users/adri/Desktop/untitled.dxcc");

    std::ifstream _als_stream(_als_path, std::ios::binary);
    std::ofstream _dxcc_stream(_dxcc_path, std::ios::binary);
    bool _as_json = _dxcc_path.extension() == ".json";
    if (!_as_json) {
        _dxcc_path.replace_extension(".dxcc");
    }

    try {
        fmtals::project _als_project;
        fmtals::version _als_version;
        fmtals::import_project(_als_stream, _als_project, _als_version);
        fmtdxc::project _dxc_project = rtdxc::detail::convert_from_als(_als_project);
        fmtdxc::project_container _container(_dxc_project);
        fmtdxc::export_container(_dxcc_stream, _container, fmtdxc::version::alpha, _as_json);
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
    }
}