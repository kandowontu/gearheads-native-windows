#pragma once

#include <windows.h>
#include <wincodec.h>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace gh {

constexpr int kCanvasWidth = 640;
constexpr int kCanvasHeight = 480;

struct Image {
    int width = 0;
    int height = 0;
    int origin_x = 0;
    int origin_y = 0;
    std::vector<std::uint32_t> pixels;
};

class ImageCache {
public:
    explicit ImageCache(std::filesystem::path root);
    ~ImageCache();

    ImageCache(const ImageCache&) = delete;
    ImageCache& operator=(const ImageCache&) = delete;

    const Image& load(const std::filesystem::path& relative_path);
    const std::filesystem::path& root() const { return root_; }

private:
    Image decode(const std::filesystem::path& path) const;

    std::filesystem::path root_;
    IWICImagingFactory* factory_ = nullptr;
    std::unordered_map<std::wstring, std::unique_ptr<Image>> images_;
    std::unordered_map<std::wstring, std::pair<int, int>> origins_;
};

class Canvas {
public:
    Canvas();
    ~Canvas();

    Canvas(const Canvas&) = delete;
    Canvas& operator=(const Canvas&) = delete;

    void clear(COLORREF color);
    void fill_rect(int x, int y, int width, int height, COLORREF color);
    void frame_rect(int x, int y, int width, int height, COLORREF color, int thickness = 1);
    void image(const Image& source, int x, int y, bool mirror_x = false);
    void image_scaled(
        const Image& source,
        int x,
        int y,
        int width,
        int height,
        bool mirror_x = false
    );
    void image_region(
        const Image& source,
        int x,
        int y,
        int source_x,
        int source_y,
        int width,
        int height
    );
    void text(
        const std::wstring& value,
        int x,
        int y,
        int pixel_height,
        COLORREF color,
        int alignment = 0,
        bool selected = false
    );
    void present(HDC target, const RECT& client);

    HDC dc() const { return memory_dc_; }

private:
    static std::uint32_t color_pixel(COLORREF color);
    void synchronize_gdi();
    void resize_present_surface(HDC target, int width, int height);

    HDC memory_dc_ = nullptr;
    HBITMAP bitmap_ = nullptr;
    HGDIOBJ previous_bitmap_ = nullptr;
    std::uint32_t* pixels_ = nullptr;
    bool gdi_pending_ = false;
    std::unordered_map<int, HFONT> fonts_;

    // The logical canvas is already a DIB back buffer.  A second, client-sized
    // surface keeps letterboxing and scaling off the visible window so a frame
    // reaches DWM in one BitBlt rather than as several independently visible
    // GDI operations.
    HDC present_dc_ = nullptr;
    HBITMAP present_bitmap_ = nullptr;
    HGDIOBJ present_previous_bitmap_ = nullptr;
    int present_width_ = 0;
    int present_height_ = 0;
};

std::filesystem::path executable_directory();
std::filesystem::path locate_asset_root();

}  // namespace gh
