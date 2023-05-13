#include <cstdint>
#include <iostream>
#include <png.h>

bool extract_touchstone_from_png_file(const std::wstring &path, std::string &touchstone)
{
    FILE *file = NULL;
    png_structp png = NULL;
    png_infop info = NULL;
    bool result = false;
    
    file = _wfopen(path.c_str(), L"rb");
    if (!file) 
        goto done;

    png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (png == NULL)
        goto done;

    info = png_create_info_struct(png);
    if (info == NULL)
        goto done;

    if (setjmp(png_jmpbuf(png)))
        goto done;
    
    png_init_io(png, file);
    png_read_png(png, info, PNG_TRANSFORM_IDENTITY, 0);

    png_textp texts;
    size_t text_count = png_get_text(png, info, &texts, NULL);
    for (size_t idx = 0; idx < text_count; idx++) {
        if (!strcmp(texts[idx].key, "Touchstone")) {
            touchstone = std::string(texts[idx].text, texts[idx].text_length);
            result = true;
            break;
        }
    }

done:
    if (png != NULL)
        png_destroy_read_struct(&png, &info, NULL);
    if (file != NULL)
        fclose(file);
    return result;
}

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
    std::wstring image_path, touchstone_path;
    for (size_t argn = 1; argn < (size_t)argc; argn++) {
        if (!wcscmp(argv[argn], L"/?")) {
            show_usage = true;
            break;
        } else if (wcscmp(argv[argn], L"/") && image_path.empty()) {
            image_path = argv[argn];
        } else {
            std::wcerr << L"Unrecognized argument '" << argv[argn] << "'!" << std::endl;
            show_usage = true;
            usage_status = EXIT_FAILURE;
        }
    }
    if (image_path.empty()) {
        show_usage = true;
        usage_status = EXIT_FAILURE;
    }
    if (show_usage) {
        std::wcerr << L"Usage: nanovna_extract.exe [options] [filename.png] [filename.s2p]" << std::endl;
        std::wcerr << std::endl;
        std::wcerr << L"Extracts Touchstone data from a PNG file. The Touchstone file name may be" << std::endl;
        std::wcerr << L"omitted." << std::endl;
        std::wcerr << std::endl;
        std::wcerr << L"Options:" << std::endl;
        std::wcerr << "\t/?\t\tShow program usage." << std::endl;
        return usage_status;
    }
    if (touchstone_path.empty()) {
        size_t pos = image_path.rfind(L'.');
        if (pos == std::string::npos)
            touchstone_path = image_path + L".s2p";
        else
            touchstone_path = image_path.substr(0, pos) + L".s2p";
    }

    std::string touchstone;
    if (!extract_touchstone_from_png_file(image_path, touchstone)) {
        std::wcerr << L"Failed to extract Touchstone data from PNG image '" << image_path << L"'!" << std::endl;
        return EXIT_FAILURE;
    }

    if (!save_touchstone_to_file(touchstone_path, touchstone)) {
        std::wcerr << L"Failed to write Touchstone data to '" << touchstone_path << L"'!" << std::endl;
        return EXIT_FAILURE;
    }

    std::wcerr << L"Extracted Touchstone data to '" << touchstone_path << L"'" << std::endl;
    return EXIT_SUCCESS;
}
