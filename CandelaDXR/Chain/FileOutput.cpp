#define _CRT_SECURE_NO_WARNINGS

#include "FileOutput.h"

#include <cstdint>
#include <stdexcept>
#include <algorithm>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"

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
    if (fileType != RAW)
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
        // stbi_write_hdr(
        //     fileName.c_str(), 
        //     static_cast<int>(radianceBuffer.getWidth()),
        //     static_cast<int>(radianceBuffer.getHeight()), 
        //     3,
        //     &radianceBuffer.getInternalBuffer().data()->x
        // );
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
    default:
        break;
    }

}