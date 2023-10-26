#define _CRT_SECURE_NO_WARNINGS

#include "FileOutput.h"

#include <cstdint>
#include <stdexcept>
#include <algorithm>
#include <array>

#include "Exception/Exception.h"

#include "stb/stb_image_write.h"
#include "tinyexr/tinyexr.h"

using candela::chain::FileOutput;
using candela::renderer::RgbSpectrum;
using candela::renderer::RadianceBuffer;
using std::runtime_error;
using std::uint8_t;
using std::clamp;

FileOutput::FileOutput()
    : filePath("output"), sequenceNumber(), fileType()
{

}

void FileOutput::setFilePath(const std::string& p_filePath)
{
    filePath = p_filePath;
}

void FileOutput::setFileType(FileType p_fileType)
{
    fileType = p_fileType;
}

void FileOutput::process(RadianceBuffer& radianceBuffer)
{
    auto fileName = filePath + "_" + std::to_string(sequenceNumber++);

    std::vector<uint8_t> buffer;
    if (fileType == PPM || fileType == PNG)
    {
        buffer.reserve(radianceBuffer.getWidth() * radianceBuffer.getHeight() * 3);

        for (size_t y = 0; y < radianceBuffer.getHeight(); ++y)
        {
            for (size_t x = 0; x < radianceBuffer.getWidth(); ++x)
            {
                auto rgb = radianceBuffer.get(x, y);
                buffer.push_back(static_cast<uint8_t>(clamp(rgb.x, 0.f, 1.f) * 255));
                buffer.push_back(static_cast<uint8_t>(clamp(rgb.y, 0.f, 1.f) * 255));
                buffer.push_back(static_cast<uint8_t>(clamp(rgb.z, 0.f, 1.f) * 255));
            }
        }
    }

    switch (fileType)
    {
    case PPM:
    {
        fileName += ".ppm";

        FILE* fp = fopen(fileName.c_str(), "wb");
        if (fp == nullptr)
            throw runtime_error("Could not open file: " + fileName);

        // Write ppm header
        fprintf(fp, "P6\n%zu %zu\n255\n", radianceBuffer.getWidth(), radianceBuffer.getHeight());
        fwrite(buffer.data(), 1, buffer.size(), fp);
        fclose(fp);
    }
    break;
    case PNG:
        fileName += ".png";
        stbi_write_png(
            fileName.c_str(),
            static_cast<int>(radianceBuffer.getWidth()),
            static_cast<int>(radianceBuffer.getHeight()),
            3,
            buffer.data(),
            static_cast<int>(radianceBuffer.getWidth() * 3)
        );
        break;
    case RAW:
    {
        fileName += ".raw";
        FILE* fp = fopen(fileName.c_str(), "wb");
        if (fp == nullptr)
            throw runtime_error("Could not open file: " + fileName);
        int w = static_cast<int>(radianceBuffer.getWidth());
        int h = static_cast<int>(radianceBuffer.getHeight());
        fwrite(&w, sizeof(int), 1, fp); // Write width
        fwrite(&h, sizeof(int), 1, fp); // Write height
        fwrite(radianceBuffer.getInternalBuffer().data(), sizeof(RgbSpectrum), radianceBuffer.getInternalBuffer().size(), fp);
        fclose(fp);
    }
    break;
    case EXR:
    {
        fileName += ".exr";
        EXRHeader header;
        InitEXRHeader(&header);

        EXRImage image;
        InitEXRImage(&image);

        image.num_channels = 3;
        auto width = radianceBuffer.getWidth();
        auto height = radianceBuffer.getHeight();
        auto rgb = radianceBuffer.getInternalBuffer().data();

        std::vector<float> images[3];
        images[0].resize(width * height);
        images[1].resize(width * height);
        images[2].resize(width * height);

        for (int i = 0; i < width * height; i++) 
        {
            images[0][i] = rgb[i].x;
            images[1][i] = rgb[i].y;
            images[2][i] = rgb[i].z;
        }

        float* image_ptr[3];
        image_ptr[0] = &(images[2].at(0)); // B
        image_ptr[1] = &(images[1].at(0)); // G
        image_ptr[2] = &(images[0].at(0)); // R

        image.images = (unsigned char**)image_ptr;
        image.width = width;
        image.height = height;

        constexpr int numChannels = 3;
        header.num_channels = numChannels;
        std::array<EXRChannelInfo, numChannels> exrChannelInfo{};
        header.channels = exrChannelInfo.data();
        // Must be BGR(A) order, since most of EXR viewers expect this channel order.
        strncpy(header.channels[0].name, "B", 255); header.channels[0].name[strlen("B")] = '\0';
        strncpy(header.channels[1].name, "G", 255); header.channels[1].name[strlen("G")] = '\0';
        strncpy(header.channels[2].name, "R", 255); header.channels[2].name[strlen("R")] = '\0';

        std::array<int, numChannels> pixelTypes{};
        std::array<int, numChannels> requestedPixelTypes{};
        header.pixel_types = pixelTypes.data();
        header.requested_pixel_types = requestedPixelTypes.data();
        for (int i = 0; i < header.num_channels; i++) 
        {
            header.pixel_types[i] = TINYEXR_PIXELTYPE_FLOAT; // pixel type of input image
            header.requested_pixel_types[i] = TINYEXR_PIXELTYPE_FLOAT; // pixel type of output image to be stored in .EXR
        }

        const char* err;
        int ret = SaveEXRImageToFile(&image, &header, fileName.c_str(), &err);
        if (ret != TINYEXR_SUCCESS)
            ThrowException("Saving EXR failed: err = " + std::string(err));
    }
    break;
    default:
        break;
    }

}