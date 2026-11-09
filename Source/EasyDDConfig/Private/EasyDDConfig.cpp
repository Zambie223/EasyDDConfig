#include "EasyDDConfig.h"
#include "EasyDDConfigLog.h"

DEFINE_LOG_CATEGORY(LogEasyDDConfig);

void FEasyDDConfigModule::StartupModule()
{
	// All real work happens in the root modules and the world subsystem, which SML creates.
}

void FEasyDDConfigModule::ShutdownModule()
{
}

IMPLEMENT_MODULE(FEasyDDConfigModule, EasyDDConfig)
