#include <fpxlib.h>
#include <turbojpeg.h>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <memory>
#include <random>
#include <stdexcept>
#include <vector>

namespace fs = std::filesystem;

void extractStorage(const std::string& input, const std::string& name, const std::string& output);
std::vector<std::string> imageStorages(const std::string& input);

void check(FPXStatus result, const char* operation) {
    if (result != FPX_OK) {
        throw std::runtime_error(std::string(operation) + ": FlashPix error " +
                                 std::to_string(result));
    }
}

struct TemporaryCopy {
    fs::path directory;
    fs::path file;

    explicit TemporaryCopy(const fs::path& input) {
        std::random_device random;
        for (int attempt = 0; attempt < 100; ++attempt) {
            const auto candidate = fs::temp_directory_path() /
                ("mix-photo-export-" + std::to_string(random()) + "-" + std::to_string(random()));
            if (fs::create_directory(candidate)) {
                directory = candidate;
                break;
            }
        }
        if (directory.empty()) throw std::runtime_error("Cannot create temporary directory");
        file = directory / "input.mix";
        try {
            fs::permissions(directory, fs::perms::owner_all, fs::perm_options::replace);
            fs::copy_file(input, file);
        } catch (...) {
            fs::remove_all(directory);
            throw;
        }
    }

    ~TemporaryCopy() {
        std::error_code error;
        fs::remove_all(directory, error);
    }
};

struct Toolkit {
    Toolkit() { check(FPX_InitSystem(), "Initialize decoder"); }
    ~Toolkit() { FPX_ClearSystem(); }
};

struct ImageHandle {
    FPXImageHandle* image = nullptr;
    ~ImageHandle() { if (image) FPX_CloseImage(image); }
};

void convert(const fs::path& input, const std::string& storage, const fs::path& output, int jpegQuality) {
    if (fs::exists(output)) throw std::runtime_error("Output already exists: " + output.string());
    TemporaryCopy temporary(input);
    Toolkit toolkit;
    if (!storage.empty()) {
        const auto extracted = temporary.directory / "image.fpx";
        extractStorage(temporary.file.string(), storage, extracted.string());
        temporary.file = extracted;
    }
    ImageHandle handle;
    unsigned int width = 0, height = 0, tileWidth = 64, tileHeight = 64;
    FPXColorspace colorspace{};
    const auto decoderPath = temporary.file.string();
    check(FPX_OpenImageByFilename(decoderPath.c_str(), nullptr,
          &width, &height, &tileWidth, &tileHeight, &colorspace, &handle.image), "Open image");
    if (!width || !height || width > 65500 || height > 65500 ||
        static_cast<uint64_t>(width) * height > 100000000) {
        throw std::runtime_error("Unsupported image dimensions");
    }
    FPXResolution resolution{};
    check(FPX_GetResolutionInfo(handle.image, &resolution), "Read resolution pyramid");
    if (resolution.numberOfResolutions < 1 || resolution.numberOfResolutions > FPXMAXRESOLUTIONS)
        throw std::runtime_error("Invalid resolution pyramid");
    std::vector<unsigned char> pixels(static_cast<size_t>(width) * height * 4, 0);
    FPXImageDesc description{};
    description.numberOfComponents = 4;
    const FPXComponentColor components[] = {NIFRGB_R, NIFRGB_G, NIFRGB_B, ALPHA};
    for (unsigned int channel = 0; channel < 4; ++channel) {
        auto& component = description.components[channel];
        component.myColorType = {components[channel], DATA_TYPE_UNSIGNED_BYTE};
        component.horzSubSampFactor = component.vertSubSampFactor = 1;
        component.columnStride = 4;
        component.lineStride = static_cast<int>(width * 4);
        component.theData = pixels.data() + channel;
    }
    check(FPX_ReadImageRectangle(handle.image, 0, 0, width - 1, height - 1,
          resolution.numberOfResolutions - 1, &description), "Decode pixels");
    for (size_t offset = 0; offset < pixels.size(); offset += 4) {
        for (size_t channel = 0; channel < 3; ++channel)
            pixels[offset + channel] = std::min(255, pixels[offset + channel] + 255 - pixels[offset + 3]);
        pixels[offset + 3] = 255;
    }
    std::unique_ptr<void, decltype(&tjDestroy)> compressor(tjInitCompress(), tjDestroy);
    if (!compressor) throw std::runtime_error("Cannot initialize JPEG encoder");
    unsigned char* encoded = nullptr;
    unsigned long size = 0;
    const int result = tjCompress2(compressor.get(), pixels.data(), static_cast<int>(width), 0,
        static_cast<int>(height), TJPF_RGBA, &encoded, &size, TJSAMP_444, jpegQuality, TJFLAG_ACCURATEDCT);
    std::unique_ptr<unsigned char, decltype(&tjFree)> buffer(encoded, tjFree);
    if (result != 0) throw std::runtime_error(std::string("Encode JPEG: ") + tjGetErrorStr2(compressor.get()));
#ifdef _WIN32
    FILE* file = _wfopen(output.c_str(), L"wbx");
#else
    FILE* file = fopen(output.c_str(), "wbx");
#endif
    if (!file) throw std::runtime_error("Cannot create JPEG: " + output.string());
    bool success = fwrite(buffer.get(), 1, size, file) == size;
    if (fclose(file) != 0) success = false;
    if (!success) fs::remove(output);
    if (!success) throw std::runtime_error("Cannot write JPEG: " + output.string());
    std::cout << output.filename().string() << " (" << width << 'x' << height << ")\n";
}

int main(int argc, char** argv) {
    try {
        fs::path input;
        fs::path output;
        int quality = 95;
        for (int argument = 1; argument < argc; ++argument) {
            const std::string value = argv[argument];
            if (value == "--help" || value == "-h") {
                std::cout << "MIX Photo Export\n"
                             "Usage: mix-photo-export -input INPUT -output DIRECTORY [--quality 1..100]\n"
                             "Also accepts positional INPUT, --input, --output, and -o.\n"
                             "Convert a .mix/.fpx file or a folder of files to JPEG.\n"
                             "Default output: a jpg folder beside the input files.\n"
                             "Exports every embedded image at its highest stored resolution.\n"
                             "Existing JPEGs are skipped. Picture It! layer layouts are not rendered.\n";
                return 0;
            } else if (value == "-input" || value == "--input") {
                if (!input.empty()) throw std::runtime_error("Input specified more than once");
                if (++argument == argc || std::string(argv[argument]).empty() || argv[argument][0] == '-')
                    throw std::runtime_error("Missing input file or directory");
                input = argv[argument];
            } else if (value == "-output" || value == "--output" || value == "-o") {
                if (++argument == argc || std::string(argv[argument]).empty() || argv[argument][0] == '-')
                    throw std::runtime_error("Missing output directory");
                output = argv[argument];
            } else if (value == "--quality") {
                if (++argument == argc) throw std::runtime_error("Missing JPEG quality");
                const std::string number = argv[argument];
                size_t consumed = 0;
                quality = std::stoi(number, &consumed);
                if (consumed != number.size() || quality < 1 || quality > 100)
                    throw std::runtime_error("JPEG quality must be between 1 and 100");
            } else if (input.empty() && value.rfind("-", 0) != 0) {
                input = value;
            } else {
                throw std::runtime_error("Unexpected argument: " + value);
            }
        }
        if (input.empty()) throw std::runtime_error("Usage: mix-photo-export -input INPUT -output DIRECTORY [--quality 1..100]");
        input = fs::absolute(input);
        const bool directory = fs::is_directory(input);
        if (output.empty()) output = (directory ? input : input.parent_path()) / "jpg";
        output = fs::absolute(output);
        std::vector<fs::path> files;
        auto supported = [](const fs::path& file) {
            auto extension = file.extension().string();
            std::transform(extension.begin(), extension.end(), extension.begin(),
                [](unsigned char letter) { return static_cast<char>(std::tolower(letter)); });
            return extension == ".mix" || extension == ".fpx";
        };
        if (directory) {
            for (const auto& entry : fs::directory_iterator(input))
                if (entry.is_regular_file() && supported(entry.path())) files.push_back(entry.path());
        } else if (fs::is_regular_file(input) && supported(input)) {
            files.push_back(input);
        }
        if (files.empty()) throw std::runtime_error("No .mix or .fpx files found");
        std::sort(files.begin(), files.end());
        fs::create_directories(output);
        size_t converted = 0, skipped = 0, failed = 0;
        for (const auto& file : files) {
            try {
                std::vector<std::string> storages;
                {
                    TemporaryCopy temporary(file);
                    Toolkit toolkit;
                    storages = imageStorages(temporary.file.string());
                }
                if (storages.empty()) storages.push_back("");
                for (size_t index = 0; index < storages.size(); ++index) {
                    const auto& storage = storages[index];
                    const std::string suffix = index == 0 ? "" : "-image-" + storage.substr(18);
                    const auto destination = output / (file.stem().string() + suffix + ".jpg");
                    if (fs::exists(destination)) {
                        ++skipped;
                        std::cout << "SKIP " << destination.filename().string() << '\n';
                        continue;
                    }
                    try {
                        convert(file, storage, destination, quality);
                        ++converted;
                    } catch (const std::exception& error) {
                        ++failed;
                        std::cerr << "FAIL " << file.filename().string() << " [" << storage << "]: " << error.what() << '\n';
                    }
                }
            } catch (const std::exception& error) {
                ++failed;
                std::cerr << "FAIL " << file.filename().string() << ": " << error.what() << '\n';
            }
        }
        std::cout << converted << " converted, " << skipped << " skipped, " << failed
                  << " failed. Output: " << output.string() << '\n';
        return failed ? 1 : 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}