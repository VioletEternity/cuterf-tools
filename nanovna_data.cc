#include <iostream>
#include <iomanip>

#include "libnanovna.h"

std::string current_date_time()
{
    time_t now = time(NULL);
    struct tm *timeinfo = localtime(&now);

    std::stringstream ss;
    ss << 1900 + timeinfo->tm_year;
    ss << '-' << std::setw(2) << std::setfill('0') << timeinfo->tm_mon + 1;
    ss << '-' << std::setw(2) << std::setfill('0') << timeinfo->tm_mday;
    ss << ' ' << std::setw(2) << std::setfill('0') << timeinfo->tm_hour;
    ss << ':' << std::setw(2) << std::setfill('0') << timeinfo->tm_min;
    ss << ':' << std::setw(2) << std::setfill('0') << timeinfo->tm_sec;
    return ss.str();
}

std::wstring current_date_time_for_filename()
{
    time_t now = time(NULL);
    struct tm *timeinfo = localtime(&now);

    std::wstringstream ss;
    ss << std::setw(2) << std::setfill(L'0') << timeinfo->tm_year % 100;
    ss << std::setw(2) << std::setfill(L'0') << timeinfo->tm_mon + 1;
    ss << std::setw(2) << std::setfill(L'0') << timeinfo->tm_mday;
    ss << '_';
    ss << std::setw(2) << std::setfill(L'0') << timeinfo->tm_hour;
    ss << std::setw(2) << std::setfill(L'0') << timeinfo->tm_min;
    ss << std::setw(2) << std::setfill(L'0') << timeinfo->tm_sec;
    return ss.str();
}

bool save_snp_to_file(const std::wstring &path, const std::string &board, const std::string &version, const std::vector<libnanovna::point> &data, unsigned ports)
{
    FILE *f = _wfopen(path.c_str(), L"wt");
    if (!f)
        return false;
    
    fprintf(f, "! Measured with %s (Version: %s)\n", board.c_str(), version.c_str());
    fprintf(f, "! Date: %s\n", current_date_time().c_str());
    fprintf(f, "# HZ S RI R 50\n");
    for (size_t idx = 0; idx < data.size(); idx++) {
        if (ports == 1)
            fprintf(f, "%10d %12.9f %12.9f\n",
                data[idx].freq,
                data[idx].s11.real(), data[idx].s11.imag());
        if (ports == 2)
            fprintf(f, "%10d %12.9f %12.9f %12.9f %12.9f %12.9f %12.9f %12.9f %12.9f\n",
                data[idx].freq,
                data[idx].s11.real(), data[idx].s11.imag(),
                data[idx].s21.real(), data[idx].s21.imag(),
                0.0f, 0.0f,
                0.0f, 0.0f);
    }
    
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

    std::string board, version;
    std::vector<libnanovna::point> data;
    try {
        libnanovna::nanovna device;
        if (!device.open()) {
            std::wcerr << L"Cannot find a connected NanoVNA!" << std::endl;
            return EXIT_FAILURE;
        }
        std::wcerr << "Found NanoVNA at '" << device.path() << L"'" << std::endl;
        board = device.board();
        version = device.version();
        data = device.capture_data(ports);
    } catch (const std::runtime_error &e) {
        std::wcerr << L"Failed to read data from NanoVNA: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    if (!save_snp_to_file(output_path, board, version, data, ports)) {
        std::wcerr << L"Failed to write data to '" << output_path << L"'!" << std::endl;
        return EXIT_FAILURE;
    }

    std::wcerr << L"Saved data to '" << output_path << L"'" << std::endl;
    return EXIT_SUCCESS;
}