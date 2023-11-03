#include "ChainFactory.h"

#include "VectorFactory.h"

#include "Chain/AlphaCorrection.h"
#include "Chain/FileOutput.h"
#include "Chain/ToneMapping.h"
#include "Exception/Exception.h"

#include <algorithm>

using std::unique_ptr;
using std::make_unique;

using feanor::configuration::ConfigurationNode;

using candela::chain::IChain;
using candela::chain::AlphaCorrection;
using candela::chain::FileOutput;
using candela::chain::ToneMapping;
using candela::chain::CFList;
using candela::chain::factory::ChainFactory;
using candela::chain::factory::AlphaCorrectionChainFactory;
using candela::chain::factory::FileOutputChainFactory;
using candela::chain::factory::ToneMappingChainFactory;
using candela::mathematics::factory::Vector3Factory;
using candela::mathematics::factory::Vector2Factory;
using candela::mathematics::Vector;
using candela::mathematics::Vector3;

unique_ptr<CFList> ChainFactory::create() const
{
	return make_unique<CFList>();
}


unique_ptr<CFList> ChainFactory::create(const ConfigurationNode& config) const
{
	auto chain = create();
	const auto& configObj = config.asObject();
	if (!configObj.keyExists("Chain"))
		return chain;

	CFList& list = *chain;
	for (const auto& chainItemConfig : configObj["Chain"].asList())
	{
		if (!chainItemConfig.asObject().keyExists("Type"))
			continue;
		if (chainItemConfig["Type"].read<std::string>() == "AlphaCorrection")
			list.emplace_back(AlphaCorrectionChainFactory().create(chainItemConfig));
		else if (chainItemConfig["Type"].read<std::string>() == "FileOutput")
			list.emplace_back(FileOutputChainFactory().create(chainItemConfig));
		else if (chainItemConfig["Type"].read<std::string>() == "ToneMapping")
			list.emplace_back(ToneMappingChainFactory().create(chainItemConfig));
		else
			ThrowException("Invalid chain item type");
	}

	return chain;
}

unique_ptr<IChain> AlphaCorrectionChainFactory::create() const
{
	return make_unique<AlphaCorrection>();
}

unique_ptr<IChain> AlphaCorrectionChainFactory::create(const ConfigurationNode& config) const
{
	auto alphaCorrection = make_unique<AlphaCorrection>();
	if (config.asObject().keyExists("Gamma"))
		alphaCorrection->setGamma(config["Gamma"].read<float>());
	return alphaCorrection;
}

unique_ptr<IChain> ToneMappingChainFactory::create() const
{
	return make_unique<ToneMapping>();
}


unique_ptr<IChain> ToneMappingChainFactory::create(const ConfigurationNode& config) const
{
	return create();
}

unique_ptr<IChain> FileOutputChainFactory::create() const
{
	return make_unique<FileOutput>();
}


unique_ptr<IChain> FileOutputChainFactory::create(const ConfigurationNode& config) const
{
	auto fileOutput = make_unique<FileOutput>();
	const auto& configObj = config.asObject();

	if (configObj.keyExists("FilePath"))
		fileOutput->setFilePath(configObj["FilePath"].read<std::string>());

	if (configObj.keyExists("FileType"))
	{
		auto fileTypeStr = configObj["FileType"].read<std::string>();
		std::transform(fileTypeStr.begin(), fileTypeStr.end(), fileTypeStr.begin(),
			[](unsigned char c) { return std::tolower(c); });
		if (fileTypeStr == "ppm")
			fileOutput->setFileType(FileOutput::PPM);
		else if (fileTypeStr == "png")
			fileOutput->setFileType(FileOutput::PNG);
		else if (fileTypeStr == "raw")
			fileOutput->setFileType(FileOutput::RAW);
		else if (fileTypeStr == "exr")
			fileOutput->setFileType(FileOutput::EXR);
		else
			ThrowException("Wrong FileType. Must be PPM/PNG/RAW/EXR");
	}

	if (configObj.keyExists("CompressionType"))
	{
		auto cTypeStr = configObj["CompressionType"].read<std::string>();
		std::transform(cTypeStr.begin(), cTypeStr.end(), cTypeStr.begin(),
			[](unsigned char c) { return std::tolower(c); });
		if (cTypeStr == "none")
			fileOutput->setCompressionType(FileOutput::NONE);
		else if (cTypeStr == "rle")
			fileOutput->setCompressionType(FileOutput::RLE);
		else if (cTypeStr == "zips")
			fileOutput->setCompressionType(FileOutput::ZIPS);
		else if (cTypeStr == "zip")
			fileOutput->setCompressionType(FileOutput::ZIP);
		else if (cTypeStr == "piz")
			fileOutput->setCompressionType(FileOutput::PIZ);
		else
			ThrowException("Wrong Compression Type. Must be NONE/RLE/ZIPS/ZIP/PIZ");
	}
		

	return fileOutput;
}
