#include <png.h>
#include <cuterf.h>
#include "common.h"

using namespace cuterf;

int wmain(int argc, wchar_t** argv) 
{
    bool show_usage = false;
    int usage_status = EXIT_SUCCESS;
    std::wstring screenshot_path;
    int scale = 1;
    for (size_t argn = 1; argn < (size_t)argc; argn++) {
        if (!wcscmp(argv[argn], L"/?")) {
            show_usage = true;
            break;
        } else if (!wcsncmp(argv[argn], L"/scale:", 7) || !wcsncmp(argv[argn], L"/x", 2)) {
            wchar_t *szScale, *szScaleEnd;
            if (argv[argn][2] == 's')
                szScale = &argv[argn][7];
            else
                szScale = &argv[argn][2];
            scale = wcstol(szScale, &szScaleEnd, 10);
            if (*szScaleEnd != L'\0' || !(scale >= 1 && scale <= 4)) {
                std::wcerr << L"Scale should be 1 to 4 inclusive!" << std::endl;
                show_usage = true;
                usage_status = EXIT_FAILURE;
                break;
            }
        } else if (wcscmp(argv[argn], L"/") && screenshot_path.empty()) {
            screenshot_path = argv[argn];
        } else {
            std::wcerr << L"Unrecognized argument '" << argv[argn] << "'!" << std::endl;
            show_usage = true;
            usage_status = EXIT_FAILURE;
        }
    }
    if (show_usage) {
        std::wcerr << L"Usage: nanovna_screenshot.exe [options] [filename.png]" << std::endl;
        std::wcerr << std::endl;
        std::wcerr << L"Writes a screen capture to a PNG file. The PNG file includes all acquired data" << std::endl;
        std::wcerr << L"in Touchstone format for a 2-port network in the \"Touchstone\" tEXt chunk." << std::endl;
        std::wcerr << std::endl;
        std::wcerr << L"Options:" << std::endl;
        std::wcerr << "\t/?\t\tShow program usage." << std::endl;
        std::wcerr << "\t/scale:N, /xN\tEnlarge image by factor of N (1 <= N <= 4)." << std::endl;
        return usage_status;
    }
    if (screenshot_path.empty())
        screenshot_path = L"NanoVNA_Screenshot_" + current_date_time_for_filename() + L".png";

    std::string screen_raw_data, source, creation_time, touchstone;
    size_t screen_width, screen_height;
    try {
        nanovna::device device;
        if (!device.open()) {
            std::wcerr << L"Cannot find a connected NanoVNA!" << std::endl;
            return EXIT_FAILURE;
        }
        std::wcerr << "Found NanoVNA at '" << device.path() << L"'" << std::endl;
        screen_raw_data = device.capture_screenshot(screen_width, screen_height);
        source = device.board_name() + " (firmware " + device.firmware_info() + ")";
        creation_time = device.timestamp();
        touchstone = device.capture_touchstone(2);
    } catch (const std::runtime_error &e) {
        std::wcerr << L"Failed to read screenshot from NanoVNA: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    rgb565_pixmap pixmap(screen_width, screen_height);
    pixmap.read_from_capture(screen_raw_data);
    creation_time[4] = ':'; // PNG uses YYYY:mm:dd HH:MM
    creation_time[7] = ':';
    if (!pixmap.save_to_png_file(screenshot_path, (unsigned)scale, source, creation_time, "Touchstone", touchstone)) {
        std::wcerr << L"Failed to write screenshot to '" << screenshot_path << L"'!" << std::endl;
        return EXIT_FAILURE;
    }

    std::wcerr << L"Saved screenshot to '" << screenshot_path << L"'" << std::endl;
    return EXIT_SUCCESS;
}
