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
            RAW = 2
        };

        FileOutput();
        void process(renderer::RadianceBuffer& buffer) override;

        void setFilePath(const std::string& p_filePath);
        void setFileType(FileType fileType);
    private:
        std::string filePath;

        std::uint32_t sequenceNumber;
        FileType fileType;
    };
}
