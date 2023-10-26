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
using std::array;
using std::vector;

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

    const auto width = static_cast<int>(radianceBuffer.getWidth());
    const auto height = static_cast<int>(radianceBuffer.getHeight());

    vector<uint8_t> buffer;
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
            width,
            height,
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
        fwrite(&width, sizeof(int), 1, fp); // Write w
        fwrite(&height, sizeof(int), 1, fp); // Write h
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
        constexpr int numChannels = 3;

        image.num_channels = numChannels;
        auto w = radianceBuffer.getWidth();
        auto h = radianceBuffer.getHeight();
        auto rgb = radianceBuffer.getInternalBuffer().data();

        array<vector<float>, numChannels> images{};
        for (int i = 0; i < numChannels; ++i)
            images[i].resize(w * h);

        for (std::size_t i = 0; i < w * h; i++) 
        {
            for (int j = 0; j < numChannels; ++j)
                images[j][i] = (&rgb[i].x)[j];
        }

        array<float*, numChannels> image_ptr{};
        for (int i = 0; i < numChannels; ++i) // B G R
            image_ptr[i] = &(images[numChannels - (i + 1)].at(0));

        image.images = (unsigned char**)image_ptr.data();
        image.width = width;
        image.height = height;

        header.num_channels = numChannels;
        array<EXRChannelInfo, numChannels> exrChannelInfo{};
        header.channels = exrChannelInfo.data();
        // Must be BGR(A) order, since most of EXR viewers expect this channel order.
        array<const char*, numChannels> bgr = { "B", "G", "R" };
        for (int i = 0; i < numChannels; ++i) // B G R
        {
            strncpy(header.channels[i].name, bgr[i], 255);
            header.channels[i].name[strlen(bgr[i])] = '\0';
        }

        array<int, numChannels> pixelTypes{};
        array<int, numChannels> requestedPixelTypes{};
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