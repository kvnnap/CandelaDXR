#include "ChainFactory.h"

#include "VectorFactory.h"

#include "Chain/AlphaCorrection.h"
#include "Chain/Exposure.h"
#include "Chain/FileOutput.h"
#include "Chain/ToneMapping.h"
#include "Exception/Exception.h"

#include "Util/StringUtil.h"

#include "Environment/Environment.h"

using std::unique_ptr;
using std::make_unique;

using feanor::configuration::ConfigurationNode;

using candela::chain::IChain;
using candela::chain::AlphaCorrection;
using candela::chain::Exposure;
using candela::chain::FileOutput;
using candela::chain::ToneMapping;
using candela::chain::CFList;
using candela::chain::factory::ChainFactory;
using candela::chain::factory::AlphaCorrectionChainFactory;
using candela::chain::factory::ExposureChainFactory;
using candela::chain::factory::FileOutputChainFactory;
using candela::chain::factory::ToneMappingChainFactory;
using candela::mathematics::factory::Vector3Factory;
using candela::mathematics::factory::Vector2Factory;
using candela::mathematics::Vector;
using candela::mathematics::Vector3;
using candela::util::ToLower;

ChainFactory::ChainFactory(candela::environment::Environment& env)
	: env(env)
{}

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

	auto& chainFactory = env.getChainManager().getFactoryManager();
	for (const auto& chainItemConfig : configObj["Chain"].asList())
	{
		if (chainItemConfig.asObject().keyExists("Type"))
			chain->push_back(chainFactory.get(chainItemConfig["Type"]).create(chainItemConfig));
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

unique_ptr<IChain> ExposureChainFactory::create() const
{
	return make_unique<Exposure>();
}

unique_ptr<IChain> ExposureChainFactory::create(const ConfigurationNode& config) const
{
	auto exposure = make_unique<Exposure>();
	if (config.asObject().keyExists("Level"))
		exposure->setExposure(config["Level"].read<float>());
	return exposure;
}

unique_ptr<IChain> ToneMappingChainFactory::create() const
{
	return make_unique<ToneMapping>();
}


unique_ptr<IChain> ToneMappingChainFactory::create(const ConfigurationNode& config) const
{
	ToneMapping::ToneMappingType type = ToneMapping::ToneMappingType::Reinhard;
	if (config.asObject().keyExists("ToneMapper"))
	{
		const auto strType = config["ToneMapper"].read<std::string>();
		if (strType == "Reinhard")
			type = ToneMapping::ToneMappingType::Reinhard;
		else if (strType == "ACES")
			type = ToneMapping::ToneMappingType::ACES;
		else
			ThrowException("Wrong ToneMapper. Must be Reinhard or ACES (Case Sensitive)");
	}

	return make_unique<ToneMapping>(type);
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
		auto fileTypeStr = ToLower(configObj["FileType"].read<std::string>());
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
		auto cTypeStr = ToLower(configObj["CompressionType"].read<std::string>());
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
