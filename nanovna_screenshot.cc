#include <cstdint>
#include <iostream>
#include <iomanip>
#include <png.h>
#include "libnanovna.h"

static const std::string SOFTWARE_NAME = "https://github.com/VioletEternity/nanovna-tools";

struct screenshot 
{
    static constexpr size_t display_width = 480;
    static constexpr size_t display_height = 320;
    typedef uint16_t pixel;

    pixel data[display_width * display_height];

    std::vector<uint8_t> to_rgb24(unsigned scale) const 
    {
        static const uint8_t lut6to8[] = {
            0,   4,   8,   12,  16,  20,  24,  28,  32,  36,  40,  45,  49,  53,  57,  61,
            65,  69,  73,  77,  81,  85,  89,  93,  97,  101, 105, 109, 113, 117, 121, 125,
            130, 134, 138, 142, 146, 150, 154, 158, 162, 166, 170, 174, 178, 182, 186, 190,
            194, 198, 202, 206, 210, 215, 219, 223, 227, 231, 235, 239, 243, 247, 251, 255
        };
        std::vector<uint8_t> rgb24_data;
        for (size_t row = 0; row < display_height; row++) {
            for (unsigned yrepeat = 0; yrepeat < scale; yrepeat++) {
                for (size_t column = 0; column < display_width; column++) {
                    pixel pixel = data[column + display_width * row];
                    for (unsigned xrepeat = 0; xrepeat < scale; xrepeat++) {
                        rgb24_data.push_back(lut6to8[(pixel & 0xf800) >> 10]);
                        rgb24_data.push_back(lut6to8[(pixel & 0x07e0) >> 5]);
                        rgb24_data.push_back(lut6to8[(pixel & 0x001f) << 1]);
                    }
                }
            }
        }
        return rgb24_data;
    }

    void read_from_nanovna(libnanovna::nanovna &device)
    {
        std::string raw_data = device.capture_screenshot();
        for (size_t i = 0; i < raw_data.length(); i += 2)
            data[i / 2] = (raw_data[i] << 8) | (raw_data[i + 1] & 0xff);
    }

    bool save_to_png_file(const std::wstring &path, unsigned scale, const std::string &source, const std::string &creation_time, const std::string &touchstone)
    {
        FILE *file = NULL;
        png_structp png = NULL;
        png_infop info = NULL;
        std::vector<uint8_t> rgb24_data;
        std::vector<png_bytep> rows;
        bool result = false;
        
        file = _wfopen(path.c_str(), L"wb");
        if (file == NULL) 
            goto done;

        png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
        if (png == NULL)
            goto done;

        info = png_create_info_struct(png);
        if (info == NULL)
            goto done;

        if (setjmp(png_jmpbuf(png)))
            goto done;

        png_set_IHDR(png, info,
            display_width * scale,
            display_height * scale,
            8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
            PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

        rgb24_data = to_rgb24(scale);
        for (size_t row = 0; row < display_height * scale; row++)
            rows.push_back(&rgb24_data[3 * display_width * scale * row]);
        png_set_rows(png, info, &rows[0]);

        png_text texts[4];
        texts[0].key = "Software";
        texts[0].compression = PNG_TEXT_COMPRESSION_NONE;
        texts[0].text = const_cast<png_charp>(SOFTWARE_NAME.c_str());
        texts[0].text_length = SOFTWARE_NAME.length();
        texts[1].key = "Source";
        texts[1].compression = PNG_TEXT_COMPRESSION_NONE;
        texts[1].text = const_cast<png_charp>(source.c_str());
        texts[1].text_length = source.length();
        texts[2].key = "Creation Time";
        texts[2].compression = PNG_TEXT_COMPRESSION_NONE;
        texts[2].text = const_cast<png_charp>(creation_time.c_str());
        texts[2].text_length = creation_time.length();
        texts[3].key = "Touchstone";
        texts[3].compression = PNG_TEXT_COMPRESSION_zTXt;
        texts[3].text = const_cast<png_charp>(touchstone.c_str());
        texts[3].text_length = touchstone.length();
        png_set_text(png, info, texts, sizeof(texts) / sizeof(texts[0]));

        png_init_io(png, file);
        png_write_png(png, info, PNG_TRANSFORM_IDENTITY, 0);
        result = true;

    done:
        if (png != NULL)
            png_destroy_write_struct(&png, &info);
        if (file != NULL)
            fclose(file);
        return result;
    }
};

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

    screenshot screenshot;
    std::string source, creation_time, touchstone;
    try {
        libnanovna::nanovna device;
        if (!device.open()) {
            std::wcerr << L"Cannot find a connected NanoVNA!" << std::endl;
            return EXIT_FAILURE;
        }
        std::wcerr << "Found NanoVNA at '" << device.path() << L"'" << std::endl;
        screenshot.read_from_nanovna(device);
        source = device.board_name() + " (firmware " + device.firmware_info() + ")";
        creation_time = device.timestamp();
        touchstone = device.capture_touchstone(2);
    } catch (const std::runtime_error &e) {
        std::wcerr << L"Failed to read screenshot from NanoVNA: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    if (!screenshot.save_to_png_file(screenshot_path, (unsigned)scale, source, creation_time, touchstone)) {
        std::wcerr << L"Failed to write screenshot to '" << screenshot_path << L"'!" << std::endl;
        return EXIT_FAILURE;
    }

    std::wcerr << L"Saved screenshot to '" << screenshot_path << L"'" << std::endl;
    return EXIT_SUCCESS;
}
