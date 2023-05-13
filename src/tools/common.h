#ifndef UTILS_H
#define UTILS_H

#include <time.h>
#include <string>
#include <iostream>
#include <iomanip>
#include <png.h>

static const std::string SOFTWARE_NAME = "https://github.com/VioletEternity/cuterf-tools";

std::wstring current_date_time_for_filename()
{
    time_t now = time(NULL);
    struct tm *timeinfo = localtime(&now);

    std::wstringstream ss;
    ss << std::setw(2) << std::setfill(L'0') << timeinfo->tm_year % 100;
    ss << std::setw(2) << std::setfill(L'0') << timeinfo->tm_mon + 1;
    ss << std::setw(2) << std::setfill(L'0') << timeinfo->tm_mday;
    ss << L'_';
    ss << std::setw(2) << std::setfill(L'0') << timeinfo->tm_hour;
    ss << std::setw(2) << std::setfill(L'0') << timeinfo->tm_min;
    ss << std::setw(2) << std::setfill(L'0') << timeinfo->tm_sec;
    return ss.str();
}

std::string current_date_time_for_metadata()
{
    time_t now = time(NULL);
    struct tm *timeinfo = localtime(&now);

    std::stringstream ss;
    ss << std::setw(4) << std::setfill('0') << (1900 + timeinfo->tm_year);
    ss << ':';
    ss << std::setw(2) << std::setfill('0') << timeinfo->tm_mon + 1;
    ss << ':';
    ss << std::setw(2) << std::setfill('0') << timeinfo->tm_mday;
    ss << ' ';
    ss << std::setw(2) << std::setfill('0') << timeinfo->tm_hour;
    ss << ':';
    ss << std::setw(2) << std::setfill('0') << timeinfo->tm_min;
    ss << ':';
    ss << std::setw(2) << std::setfill('0') << timeinfo->tm_sec;
    return ss.str();
}

struct rgb565_pixmap
{
    typedef uint16_t pixel;

    const size_t width, height;
    std::unique_ptr<pixel[]> data;

    rgb565_pixmap(size_t width, size_t height) :
        width(width), height(height), data(new pixel[width * height])
    {}

    std::vector<uint8_t> to_rgb24(unsigned scale) const 
    {
        static const uint8_t lut6to8[] = {
            0,   4,   8,   12,  16,  20,  24,  28,  32,  36,  40,  45,  49,  53,  57,  61,
            65,  69,  73,  77,  81,  85,  89,  93,  97,  101, 105, 109, 113, 117, 121, 125,
            130, 134, 138, 142, 146, 150, 154, 158, 162, 166, 170, 174, 178, 182, 186, 190,
            194, 198, 202, 206, 210, 215, 219, 223, 227, 231, 235, 239, 243, 247, 251, 255
        };
        std::vector<uint8_t> rgb24_data;
        for (size_t row = 0; row < height; row++) {
            for (unsigned yrepeat = 0; yrepeat < scale; yrepeat++) {
                for (size_t column = 0; column < width; column++) {
                    pixel pixel = data[column + width * row];
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

    void read_from_capture(const std::string &raw_data)
    {
        if (raw_data.size() != width * height * sizeof(pixel))
            throw std::runtime_error("raw capture data has wrong size!");
        for (size_t i = 0; i < raw_data.length(); i += 2)
            data[i / 2] = (raw_data[i] << 8) | (raw_data[i + 1] & 0xff);
    }

    bool save_to_png_file(const std::wstring &path, unsigned scale, const std::string &source, const std::string &creation_time, const std::string &extra_name = "", const std::string &extra_value = "")
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
            width * scale,
            height * scale,
            8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
            PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

        rgb24_data = to_rgb24(scale);
        for (size_t row = 0; row < height * scale; row++)
            rows.push_back(&rgb24_data[3 * width * scale * row]);
        png_set_rows(png, info, &rows[0]);

        png_text texts[4];
        size_t text_count = 0;
        texts[text_count].key = "Software";
        texts[text_count].compression = PNG_TEXT_COMPRESSION_NONE;
        texts[text_count].text = const_cast<png_charp>(SOFTWARE_NAME.c_str());
        texts[text_count++].text_length = SOFTWARE_NAME.length();
        if (!source.empty()) {
            texts[text_count].key = "Source";
            texts[text_count].compression = PNG_TEXT_COMPRESSION_NONE;
            texts[text_count].text = const_cast<png_charp>(source.c_str());
            texts[text_count++].text_length = source.length();
        }
        if (!creation_time.empty()) {
            texts[text_count].key = "Creation Time";
            texts[text_count].compression = PNG_TEXT_COMPRESSION_NONE;
            texts[text_count].text = const_cast<png_charp>(creation_time.c_str());
            texts[text_count++].text_length = creation_time.length();
        }
        if (!extra_name.empty()) {
            texts[text_count].key = const_cast<png_charp>(extra_name.c_str());
            texts[text_count].compression = PNG_TEXT_COMPRESSION_zTXt;
            texts[text_count].text = const_cast<png_charp>(extra_value.c_str());
            texts[text_count++].text_length = extra_value.length();
        }
        png_set_text(png, info, texts, text_count);

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


#endif // UTILS_H