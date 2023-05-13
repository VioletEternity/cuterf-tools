#include <cuterf.h>
#include "common.h"

using namespace cuterf;

bool save_touchstone_to_file(const std::wstring &path, const std::string &touchstone)
{
    FILE *f = _wfopen(path.c_str(), L"wt");
    if (!f)
        return false;
    fwrite(touchstone.c_str(), 1, touchstone.length(), f);
    fclose(f);
    return true;
}

int wmain(int argc, wchar_t** argv) 
{
    bool show_usage = false;
    int usage_status = EXIT_SUCCESS;
    std::wstring output_path;
    unsigned ports = 0;
    for (size_t argn = 1; argn < (size_t)argc; argn++) {
        if (!wcscmp(argv[argn], L"/?")) {
            show_usage = true;
            break;
        } else if (!wcscmp(argv[argn], L"/s1p")) {
            ports = 1;
            break;
        } else if (!wcscmp(argv[argn], L"/s2p")) {
            ports = 2;
            break;
        } else if (wcscmp(argv[argn], L"/") && output_path.empty()) {
            output_path = argv[argn];
        } else {
            std::wcerr << L"Unrecognized argument '" << argv[argn] << "'!" << std::endl;
            show_usage = true;
            usage_status = EXIT_FAILURE;
        }
    }
    if (show_usage) {
        std::wcerr << L"Usage: nanovna_data.exe [options] [filename.s1p,s2p]" << std::endl;
        std::wcerr << std::endl;
        std::wcerr << L"Writes a capture of the data displayed on screen to a Touchstone format file," << std::endl;
        std::wcerr << L"without initiating a sweep." << std::endl;
        std::wcerr << std::endl;
        std::wcerr << L"Options:" << std::endl;
        std::wcerr << "\t/?\t\tShow program usage." << std::endl;
        std::wcerr << "\t/s1p\t\tSave measurements of 1-port network." << std::endl;
        std::wcerr << "\t/s2p\t\tSave measurements of 2-port network. Default if no filename given." << std::endl;
        return usage_status;
    }
    if (output_path.empty()) {
        output_path = L"NanoVNA_Data_" + current_date_time_for_filename();
        if (ports == 1)
            output_path += L".s1p";
        if (ports == 2 || ports == 0)
            output_path += L".s2p";
    }
    if (ports == 0) {
        if (output_path.length() > 4 && output_path.substr(output_path.length() - 4) == L".s1p")
            ports = 1;
        else if (output_path.length() > 4 && output_path.substr(output_path.length() - 4) == L".s2p")
            ports = 2;
        else {
            std::wcerr << "Cannot determine number of ports for measurement from filename!" << std::endl;
            std::wcerr << "Specify /s1p or /s2p explcitly." << std::endl;
            return EXIT_FAILURE;
        }
    }

    std::string touchstone;
    try {
        nanovna::device device;
        if (!device.open()) {
            std::wcerr << L"Cannot find a connected NanoVNA!" << std::endl;
            return EXIT_FAILURE;
        }
        std::wcerr << "Found NanoVNA at '" << device.path() << L"'" << std::endl;
        touchstone = device.capture_touchstone(ports);
    } catch (const std::runtime_error &e) {
        std::wcerr << L"Failed to read data from NanoVNA: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    if (!save_touchstone_to_file(output_path, touchstone)) {
        std::wcerr << L"Failed to write Touchstone data to '" << output_path << L"'!" << std::endl;
        return EXIT_FAILURE;
    }

    std::wcerr << L"Saved Touchstone data to '" << output_path << L"'" << std::endl;
    return EXIT_SUCCESS;
}
