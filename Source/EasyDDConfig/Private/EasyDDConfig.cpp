#include "EasyDDConfig.h"
#include "EasyDDConfigLog.h"
#include "EasyDDNetworkBatcher.h"

DEFINE_LOG_CATEGORY(LogEasyDDConfig);

void FEasyDDConfigModule::StartupModule()
{
	// Depot settings are handled by the root modules and the world subsystem, which SML creates.
	// The network batcher hooks a game function, so it is installed once per process here.
	EasyDD::NetBatch::Start();
}

void FEasyDDConfigModule::ShutdownModule()
{
	EasyDD::NetBatch::Stop();
}

IMPLEMENT_MODULE(FEasyDDConfigModule, EasyDDConfig)
