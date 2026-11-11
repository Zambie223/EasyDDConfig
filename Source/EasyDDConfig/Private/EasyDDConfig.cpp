#include "EasyDDConfig.h"
#include "EasyDDConfigLog.h"
#include "EasyDDDisplayRounding.h"
#include "EasyDDNetworkBatcher.h"
#include "EasyDDUploadBooster.h"

DEFINE_LOG_CATEGORY(LogEasyDDConfig);

void FEasyDDConfigModule::StartupModule()
{
	// Depot settings are handled by the root modules and the world subsystem, which SML creates.
	// The network batcher, the upload top-up and the rate display fix hook game functions, so they are installed once per process here.
	EasyDD::NetBatch::Start();
	EasyDD::UploadBoost::Start();
	EasyDD::DisplayRounding::Start();
}

void FEasyDDConfigModule::ShutdownModule()
{
	EasyDD::DisplayRounding::Stop();
	EasyDD::UploadBoost::Stop();
	EasyDD::NetBatch::Stop();
}

IMPLEMENT_MODULE(FEasyDDConfigModule, EasyDDConfig)
