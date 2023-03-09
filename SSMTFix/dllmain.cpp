#include "pch.h"

#include "Main.h"

static bool Initialize(const SKSEInterface* a_skse)
{
	auto& skse = ISKSE::GetSingleton();

	if (!skse.QueryInterfaces(a_skse))
	{
		gLog.FatalError("Could not query SKSE interfaces");
		return false;
	}

	if (!skse.CreateTrampolines(a_skse))
	{
		gLog.FatalError("Failed to create trampolines");
		return false;
	}

	auto ret = SSMTF::Initialize(a_skse);

	if (ret)
	{
		const auto usageBranch = skse.GetTrampolineUsage(TrampolineID::kBranch);
		const auto usageLocal  = skse.GetTrampolineUsage(TrampolineID::kLocal);

		gLog.Message(
			"Loaded, trampolines: branch:[%zu/%zu] codegen:[%zu/%zu]",
			usageBranch.used,
			usageBranch.total,
			usageLocal.used,
			usageLocal.total);
	}

	return ret;
}

extern "C"
{
	bool SKSEPlugin_Query(const SKSEInterface* a_skse, PluginInfo* a_info)
	{
		return ISKSE::GetSingleton().Query(a_skse, a_info);
	}

	bool SKSEPlugin_Load(const SKSEInterface* a_skse)
	{
		if (IAL::IsAE())
		{
			auto& iskse = ISKSE::GetSingleton();

			iskse.SetPluginHandle(a_skse->GetPluginHandle());
			iskse.OpenLog();
		}

		gLog.Message(
			"Initializing %s version %s (runtime %u.%u.%u.%u)",
			PLUGIN_NAME,
			PLUGIN_VERSION_VERSTRING,
			GET_EXE_VERSION_MAJOR(a_skse->runtimeVersion),
			GET_EXE_VERSION_MINOR(a_skse->runtimeVersion),
			GET_EXE_VERSION_BUILD(a_skse->runtimeVersion),
			GET_EXE_VERSION_SUB(a_skse->runtimeVersion));

		if (!IAL::IsLoaded())
		{
			WinApi::MessageBoxErrorLog(
				PLUGIN_NAME,
				"Could not load the address library");
			return false;
		}

		if (IAL::HasBadQuery())
		{
			WinApi::MessageBoxErrorLog(
				PLUGIN_NAME,
				"One or more addresses could not be retrieved from the address library");
			return false;
		}

		const bool ret = Initialize(a_skse);

		if (!ret)
		{
			WinApi::MessageBoxError(
				PLUGIN_NAME,
				"Plugin initialization failed\n\n"
				"See log for more information");
		}

		IAL::Release();
		gLog.Close();

		return ret;
	}

	SKSEPluginVersionData SKSEPlugin_Version = {
		SKSEPluginVersionData::kVersion,
		MAKE_PLUGIN_VERSION(
			PLUGIN_VERSION_MAJOR,
			PLUGIN_VERSION_MINOR,
			PLUGIN_VERSION_REVISION),
		PLUGIN_NAME,
		PLUGIN_AUTHOR,
		"",
		SKSEPluginVersionData::kVersionIndependentEx_NoStructUse,
		SKSEPluginVersionData::kVersionIndependent_AddressLibraryPostAE
#if defined(SKMP_AE_POST_629)
			| SKSEPluginVersionData::kVersionIndependent_StructsPost629
#endif
		,
		{ RUNTIME_VERSION_1_6_318, RUNTIME_VERSION_1_6_323, 0 },
		0,
	};
};
