#include "render.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <system_error>

namespace gh {
namespace {

void check_hresult(HRESULT result, const char* operation) {
    if (FAILED(result)) {
        throw std::runtime_error(std::string(operation) + " failed (HRESULT " +
                                 std::to_string(static_cast<unsigned long>(result)) + ")");
    }
}

std::wstring cache_key(const std::filesystem::path& path) {
    std::wstring result = path.lexically_normal().wstring();
    std::transform(result.begin(), result.end(), result.begin(), towlower);
    return result;
}

}  // namespace

ImageCache::ImageCache(std::filesystem::path root) : root_(std::move(root)) {
    check_hresult(
        CoCreateInstance(
            CLSID_WICImagingFactory,
            nullptr,
            CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(&factory_)
        ),
        "Creating the Windows Imaging Component factory"
    );

    std::ifstream origins_file(root_ / "data/sprite-origins.ini");
    if (!origins_file) {
        throw std::runtime_error("Could not open data/sprite-origins.ini");
    }
    std::string line;
    while (std::getline(origins_file, line)) {
        const std::size_t equals = line.find('=');
        if (equals == std::string::npos) continue;
        std::istringstream coordinates(line.substr(equals + 1));
        int origin_x = 0;
        int origin_y = 0;
        if (!(coordinates >> origin_x >> origin_y)) continue;
        origins_.emplace(
            cache_key(std::filesystem::path(line.substr(0, equals))),
            std::pair{origin_x, origin_y}
        );
    }
}

ImageCache::~ImageCache() {
    if (factory_ != nullptr) {
        factory_->Release();
    }
}

const Image& ImageCache::load(const std::filesystem::path& relative_path) {
    const std::wstring key = cache_key(relative_path);
    const auto found = images_.find(key);
    if (found != images_.end()) {
        return *found->second;
    }
    auto decoded = std::make_unique<Image>(decode(root_ / relative_path));
    if (const auto origin = origins_.find(key); origin != origins_.end()) {
        decoded->origin_x = origin->second.first;
        decoded->origin_y = origin->second.second;
    }
    const Image& result = *decoded;
    images_.emplace(key, std::move(decoded));
    return result;
}

Image ImageCache::decode(const std::filesystem::path& path) const {
    IWICBitmapDecoder* decoder = nullptr;
    IWICBitmapFrameDecode* frame = nullptr;
    IWICFormatConverter* converter = nullptr;
    try {
        check_hresult(
            factory_->CreateDecoderFromFilename(
                path.c_str(),
                nullptr,
                GENERIC_READ,
                WICDecodeMetadataCacheOnLoad,
                &decoder
            ),
            "Opening image"
        );
        check_hresult(decoder->GetFrame(0, &frame), "Reading image frame");
        check_hresult(factory_->CreateFormatConverter(&converter), "Creating image converter");
        check_hresult(
            converter->Initialize(
                frame,
                GUID_WICPixelFormat32bppBGRA,
                WICBitmapDitherTypeNone,
                nullptr,
                0.0,
                WICBitmapPaletteTypeCustom
            ),
            "Converting image to BGRA"
        );
        UINT width = 0;
        UINT height = 0;
        check_hresult(converter->GetSize(&width, &height), "Reading image dimensions");
        Image result;
        result.width = static_cast<int>(width);
        result.height = static_cast<int>(height);
        result.pixels.resize(static_cast<std::size_t>(width) * height);
        check_hresult(
            converter->CopyPixels(
                nullptr,
                width * 4,
                static_cast<UINT>(result.pixels.size() * sizeof(std::uint32_t)),
                reinterpret_cast<BYTE*>(result.pixels.data())
            ),
            "Reading image pixels"
        );
        converter->Release();
        frame->Release();
        decoder->Release();
        return result;
    } catch (...) {
        if (converter != nullptr) converter->Release();
        if (frame != nullptr) frame->Release();
        if (decoder != nullptr) decoder->Release();
        throw;
    }
}

Canvas::Canvas() {
    memory_dc_ = CreateCompatibleDC(nullptr);
    if (memory_dc_ == nullptr) {
        throw std::runtime_error("Could not create the canvas device context");
    }
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = kCanvasWidth;
    info.bmiHeader.biHeight = -kCanvasHeight;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    bitmap_ = CreateDIBSection(memory_dc_, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (bitmap_ == nullptr || bits == nullptr) {
        DeleteDC(memory_dc_);
        throw std::runtime_error("Could not create the 640x480 canvas bitmap");
    }
    pixels_ = static_cast<std::uint32_t*>(bits);
    previous_bitmap_ = SelectObject(memory_dc_, bitmap_);
    SetBkMode(memory_dc_, TRANSPARENT);
}

Canvas::~Canvas() {
    for (const auto& [size, font] : fonts_) {
        (void)size;
        DeleteObject(font);
    }
    if (memory_dc_ != nullptr && previous_bitmap_ != nullptr) {
        SelectObject(memory_dc_, previous_bitmap_);
    }
    if (bitmap_ != nullptr) DeleteObject(bitmap_);
    if (memory_dc_ != nullptr) DeleteDC(memory_dc_);
    if (present_dc_ != nullptr && present_previous_bitmap_ != nullptr) {
        SelectObject(present_dc_, present_previous_bitmap_);
    }
    if (present_bitmap_ != nullptr) DeleteObject(present_bitmap_);
    if (present_dc_ != nullptr) DeleteDC(present_dc_);
}

std::uint32_t Canvas::color_pixel(COLORREF color) {
    return static_cast<std::uint32_t>(GetRValue(color)) << 16U |
           static_cast<std::uint32_t>(GetGValue(color)) << 8U |
           static_cast<std::uint32_t>(GetBValue(color));
}

void Canvas::synchronize_gdi() {
    if (!gdi_pending_) return;
    GdiFlush();
    gdi_pending_ = false;
}

void Canvas::clear(COLORREF color) {
    synchronize_gdi();
    std::fill(pixels_, pixels_ + kCanvasWidth * kCanvasHeight, color_pixel(color));
}

void Canvas::fill_rect(int x, int y, int width, int height, COLORREF color) {
    synchronize_gdi();
    const int left = std::clamp(x, 0, kCanvasWidth);
    const int top = std::clamp(y, 0, kCanvasHeight);
    const int right = std::clamp(x + width, 0, kCanvasWidth);
    const int bottom = std::clamp(y + height, 0, kCanvasHeight);
    const std::uint32_t pixel = color_pixel(color);
    for (int row = top; row < bottom; ++row) {
        std::fill(pixels_ + row * kCanvasWidth + left, pixels_ + row * kCanvasWidth + right, pixel);
    }
}

void Canvas::frame_rect(int x, int y, int width, int height, COLORREF color, int thickness) {
    fill_rect(x, y, width, thickness, color);
    fill_rect(x, y + height - thickness, width, thickness, color);
    fill_rect(x, y, thickness, height, color);
    fill_rect(x + width - thickness, y, thickness, height, color);
}

void Canvas::image(const Image& source, int x, int y, bool mirror_x) {
    synchronize_gdi();
    for (int sy = 0; sy < source.height; ++sy) {
        const int dy = y + sy;
        if (dy < 0 || dy >= kCanvasHeight) continue;
        for (int sx = 0; sx < source.width; ++sx) {
            const int dx = x + sx;
            if (dx < 0 || dx >= kCanvasWidth) continue;
            const int source_x = mirror_x ? source.width - 1 - sx : sx;
            const std::uint32_t pixel = source.pixels[static_cast<std::size_t>(sy) * source.width + source_x];
            const unsigned alpha = pixel >> 24U;
            if (alpha == 0) continue;
            std::uint32_t& destination = pixels_[dy * kCanvasWidth + dx];
            if (alpha == 255) {
                destination = pixel & 0x00ffffffU;
                continue;
            }
            const unsigned inverse = 255U - alpha;
            const unsigned source_blue = pixel & 0xffU;
            const unsigned source_green = pixel >> 8U & 0xffU;
            const unsigned source_red = pixel >> 16U & 0xffU;
            const unsigned dest_blue = destination & 0xffU;
            const unsigned dest_green = destination >> 8U & 0xffU;
            const unsigned dest_red = destination >> 16U & 0xffU;
            destination = ((source_red * alpha + dest_red * inverse + 127U) / 255U) << 16U |
                          ((source_green * alpha + dest_green * inverse + 127U) / 255U) << 8U |
                          ((source_blue * alpha + dest_blue * inverse + 127U) / 255U);
        }
    }
}

void Canvas::image_region(
    const Image& source,
    int x,
    int y,
    int source_x,
    int source_y,
    int width,
    int height
) {
    synchronize_gdi();
    for (int row = 0; row < height; ++row) {
        if (source_y + row < 0 || source_y + row >= source.height) continue;
        for (int column = 0; column < width; ++column) {
            if (source_x + column < 0 || source_x + column >= source.width) continue;
            const std::uint32_t pixel = source.pixels[
                static_cast<std::size_t>(source_y + row) * source.width + source_x + column
            ];
            const int dx = x + column;
            const int dy = y + row;
            if (dx < 0 || dx >= kCanvasWidth || dy < 0 || dy >= kCanvasHeight) continue;
            const unsigned alpha = pixel >> 24U;
            if (alpha == 0) continue;
            if (alpha == 255) {
                pixels_[dy * kCanvasWidth + dx] = pixel & 0x00ffffffU;
                continue;
            }
            std::uint32_t& destination = pixels_[dy * kCanvasWidth + dx];
            const unsigned inverse = 255U - alpha;
            const unsigned source_blue = pixel & 0xffU;
            const unsigned source_green = pixel >> 8U & 0xffU;
            const unsigned source_red = pixel >> 16U & 0xffU;
            const unsigned dest_blue = destination & 0xffU;
            const unsigned dest_green = destination >> 8U & 0xffU;
            const unsigned dest_red = destination >> 16U & 0xffU;
            destination = ((source_red * alpha + dest_red * inverse + 127U) / 255U) << 16U |
                          ((source_green * alpha + dest_green * inverse + 127U) / 255U) << 8U |
                          ((source_blue * alpha + dest_blue * inverse + 127U) / 255U);
        }
    }
}

void Canvas::text(
    const std::wstring& value,
    int x,
    int y,
    int pixel_height,
    COLORREF color,
    int alignment,
    bool selected
) {
    HFONT font = nullptr;
    const auto found = fonts_.find(pixel_height);
    if (found == fonts_.end()) {
        font = CreateFontW(
            -pixel_height,
            0,
            0,
            0,
            FW_NORMAL,
            FALSE,
            FALSE,
            FALSE,
            ANSI_CHARSET,
            OUT_TT_PRECIS,
            CLIP_DEFAULT_PRECIS,
            ANTIALIASED_QUALITY,
            DEFAULT_PITCH,
            L"SBS"
        );
        fonts_.emplace(pixel_height, font);
    } else {
        font = found->second;
    }
    const HGDIOBJ old_font = SelectObject(memory_dc_, font);
    SetTextColor(memory_dc_, selected ? RGB(102, 255, 102) : color);
    SIZE extent{};
    GetTextExtentPoint32W(memory_dc_, value.c_str(), static_cast<int>(value.size()), &extent);
    int draw_x = x;
    if (alignment == 1) draw_x -= extent.cx / 2;
    if (alignment == 2) draw_x -= extent.cx;
    TextOutW(memory_dc_, draw_x, y - extent.cy / 2, value.c_str(), static_cast<int>(value.size()));
    gdi_pending_ = true;
    SelectObject(memory_dc_, old_font);
}

void Canvas::resize_present_surface(HDC target, int width, int height) {
    if (present_dc_ != nullptr && width == present_width_ && height == present_height_) return;

    if (present_dc_ == nullptr) {
        present_dc_ = CreateCompatibleDC(target);
        if (present_dc_ == nullptr) {
            throw std::runtime_error("Could not create the presentation device context");
        }
    }

    HBITMAP replacement = CreateCompatibleBitmap(target, width, height);
    if (replacement == nullptr) {
        throw std::runtime_error("Could not create the client-sized presentation bitmap");
    }

    if (present_bitmap_ != nullptr) {
        SelectObject(present_dc_, present_previous_bitmap_);
        DeleteObject(present_bitmap_);
    }
    present_bitmap_ = replacement;
    present_previous_bitmap_ = SelectObject(present_dc_, present_bitmap_);
    present_width_ = width;
    present_height_ = height;
    SetBkMode(present_dc_, TRANSPARENT);
}

void Canvas::present(HDC target, const RECT& client) {
    const int client_width = client.right - client.left;
    const int client_height = client.bottom - client.top;
    if (client_width <= 0 || client_height <= 0) return;
    // TextOut is batched by GDI while the rest of the renderer writes the DIB
    // pixels directly.  Finish those queued writes before StretchBlt reads the
    // logical frame.
    synchronize_gdi();
    resize_present_surface(target, client_width, client_height);

    const double scale = std::min(
        static_cast<double>(client_width) / kCanvasWidth,
        static_cast<double>(client_height) / kCanvasHeight
    );
    const int width = static_cast<int>(kCanvasWidth * scale);
    const int height = static_cast<int>(kCanvasHeight * scale);
    const int x = (client_width - width) / 2;
    const int y = (client_height - height) / 2;
    const HBRUSH black = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    RECT entire_client{0, 0, client_width, client_height};
    FillRect(present_dc_, &entire_client, black);
    SetStretchBltMode(present_dc_, COLORONCOLOR);
    StretchBlt(
        present_dc_,
        x,
        y,
        width,
        height,
        memory_dc_,
        0,
        0,
        kCanvasWidth,
        kCanvasHeight,
        SRCCOPY
    );
    BitBlt(target, 0, 0, client_width, client_height, present_dc_, 0, 0, SRCCOPY);

    // A following game tick starts by clearing the DIB through its raw pixel
    // pointer.  Ensure this presentation has consumed that DIB first; otherwise
    // GDI batching can occasionally turn the queued frame into the next tick's
    // momentary black clear.
    GdiFlush();
}

std::filesystem::path executable_directory() {
    std::array<wchar_t, 32768> buffer{};
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0 || length == buffer.size()) {
        throw std::runtime_error("Could not determine the executable path");
    }
    return std::filesystem::path(std::wstring(buffer.data(), length)).parent_path();
}

std::filesystem::path locate_asset_root() {
    std::filesystem::path directory = executable_directory();
    for (int depth = 0; depth < 5; ++depth) {
        const auto candidate = directory / "assets";
        if (std::filesystem::is_regular_file(candidate / "manifest.json")) {
            return candidate;
        }
        if (!directory.has_parent_path()) break;
        directory = directory.parent_path();
    }
    throw std::runtime_error("Could not find assets/manifest.json beside the native port");
}

}  // namespace gh
