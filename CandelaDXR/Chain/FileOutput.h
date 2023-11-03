#pragma once

#include "IChain.h"

#include <string>
#include <cstdint>

namespace candela::chain
{
    class FileOutput
        : public IChain
    {
    public:
        enum FileType
        {
            PPM = 0,
            PNG = 1,
            RAW = 2,
            EXR = 3
        };

        enum CompressionType : int
        {
            NONE = 0,
            RLE = 1,
            ZIPS = 2,
            ZIP = 3,
            PIZ = 4
        };

        FileOutput();
        void process(renderer::RadianceBuffer& buffer) override;
        void setCompressionType(CompressionType cType);
        void setFilePath(const std::string& p_filePath);
        void setFileType(FileType fileType);
    private:
        std::string filePath;

        std::uint32_t sequenceNumber;

        // Applies only to EXR images
        CompressionType compressionType;

        FileType fileType;
    };
}
