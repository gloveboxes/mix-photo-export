#include <fpxlib.h>
#include <turbojpeg.h>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <vector>

namespace fs = std::filesystem;
void makeTestDocument(const std::string& input, const std::string& output);

void check(FPXStatus result) {
    if (result != FPX_OK) throw std::runtime_error("Fixture FlashPix error " + std::to_string(result));
}

void makeImage(const fs::path& output) {
    FPXColorspace colorspace{};
    colorspace.numberOfComponents = 4;
    const FPXComponentColor components[] = {NIFRGB_R, NIFRGB_G, NIFRGB_B, ALPHA};
    for (unsigned int channel = 0; channel < 4; ++channel)
        colorspace.theComponents[channel] = {components[channel], DATA_TYPE_UNSIGNED_BYTE};
    FPXImageHandle* rawImage = nullptr;
    FPXBackground background{};
    check(FPX_CreateImageByFilename(output.string().c_str(), 128, 96, 64, 64,
        colorspace, background, NONE, &rawImage));
    std::unique_ptr<FPXImageHandle, decltype(&FPX_CloseImage)> image(rawImage, FPX_CloseImage);
    std::vector<unsigned char> pixels(128 * 96 * 4);
    for (size_t offset = 0; offset < pixels.size(); offset += 4) {
        pixels[offset] = 64;
        pixels[offset + 1] = 32;
        pixels[offset + 2] = 16;
        pixels[offset + 3] = 128;
    }
    FPXImageDesc description{};
    description.numberOfComponents = colorspace.numberOfComponents;
    for (unsigned int channel = 0; channel < 4; ++channel) {
        auto& component = description.components[channel];
        component.myColorType = colorspace.theComponents[channel];
        component.horzSubSampFactor = component.vertSubSampFactor = 1;
        component.columnStride = 4;
        component.lineStride = 128 * 4;
        component.theData = pixels.data() + channel;
    }
    check(FPX_WriteImageRectangle(image.get(), 0, 0, 127, 95, &description));
    check(FPX_CloseImage(image.release()));
}

void verifyJpeg(const fs::path& input, bool fixture) {
    std::ifstream file(input, std::ios::binary);
    if (!file) throw std::runtime_error("Cannot open JPEG");
    std::vector<unsigned char> encoded((std::istreambuf_iterator<char>(file)), {});
    std::unique_ptr<void, decltype(&tjDestroy)> decoder(tjInitDecompress(), tjDestroy);
    if (!decoder) throw std::runtime_error("Cannot initialize JPEG decoder");
    int width = 0, height = 0, subsampling = 0, colorspace = 0;
    if (tjDecompressHeader3(decoder.get(), encoded.data(), encoded.size(), &width, &height,
        &subsampling, &colorspace) != 0) throw std::runtime_error("Invalid JPEG header");
    if (width <= 0 || height <= 0 || static_cast<uint64_t>(width) * height > 100000000)
        throw std::runtime_error("Invalid JPEG dimensions");
    std::vector<unsigned char> pixels(static_cast<size_t>(width) * height * 3);
    if (tjDecompress2(decoder.get(), encoded.data(), encoded.size(), pixels.data(), width, 0,
        height, TJPF_RGB, TJFLAG_STOPONWARNING) != 0) throw std::runtime_error("JPEG decode failed");
    if (fixture) {
        if (width != 128 || height != 96) throw std::runtime_error("Fixture was not exported at full resolution");
        const size_t center = (static_cast<size_t>(height / 2) * width + width / 2) * 3;
        const int expected[] = {191, 159, 143};
        for (size_t channel = 0; channel < 3; ++channel)
            if (std::abs(pixels[center + channel] - expected[channel]) > 8)
                throw std::runtime_error("Fixture RGB/alpha mismatch");
    }
}

int main(int argc, char** argv) {
    try {
        if (argc < 3) throw std::runtime_error("Expected create DIRECTORY or verify JPEG [fixture]");
        const std::string command = argv[1];
        if (command == "create") {
            const fs::path directory = argv[2];
            fs::create_directories(directory);
            check(FPX_InitSystem());
            try {
                makeImage(directory / "plain.fpx");
                makeTestDocument((directory / "plain.fpx").string(), (directory / "layers.mix").string());
            } catch (...) {
                FPX_ClearSystem();
                throw;
            }
            check(FPX_ClearSystem());
        } else if (command == "verify") {
            verifyJpeg(argv[2], argc > 3);
        } else {
            throw std::runtime_error("Unknown fixture command");
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}