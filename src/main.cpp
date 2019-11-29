#include "skse64/PluginAPI.h"
#include "skse64_common/skse_version.h"
#include "skse64_common/Relocation.h"
#include "skse64_common/SafeWrite.h"
#include "skse64_common/BranchTrampoline.h"
#include "skse64/GameMenus.h"
#include <shlobj.h>
#include "xbyak/xbyak.h"
#include "version.h"

PluginHandle g_pluginHandle = kPluginHandle_Invalid;

// 83 79 30 00 76 10
RelocPtr<uintptr_t> RememberCurrentTabIdxAddr(0x008F46F0);  // 1_5_97
// 48 8B C4 55 57 41 54 41 56 41 57 48 8D 68 A1 48 81 EC D0 00 00 00 48 C7 45 27 FE FF FF FF
RelocPtr<uintptr_t> GetCurrentTabIndexAddr(0x008F3AD0 + 0x213);  // 1_5_97


// 83 79 30 00 76 10 + 10
RelocPtr<uintptr_t> SavedTabIndexAddr(0x02F4F1C0);  // 1_5_97
static UInt32 &SavedTabIndex = *(UInt32*)SavedTabIndexAddr.GetUIntPtr();

static UInt32 GetSavedTabIndex(UInt8 rcx, UInt8 rdx)
{
	UInt32 retn = 2;
	if (rcx != rdx) {
		MenuManager * mm = MenuManager::GetSingleton();
		if (mm) {
			if (mm->IsMenuOpen(&UIStringHolder::GetSingleton()->mapMenu)) {
				retn = 0;
			}
		} else {
			retn = rcx;
		}
	} else {
		retn = rcx;
	}

	return retn;
}

extern "C"
{
	bool SKSEPlugin_Query(const SKSEInterface * skse, PluginInfo * info)
	{
		gLog.OpenRelative(CSIDL_MYDOCUMENTS, "\\My Games\\Skyrim Special Edition\\SKSE\\StayAtSystemPage.log");

		_MESSAGE("StayAtSystemPage v%s", SYSP_VERSION_VERSTRING);
		info->infoVersion = PluginInfo::kInfoVersion;
		info->name = "StayAtSystemPage";
		info->version = SYSP_VERSION_MAJOR;

		g_pluginHandle = skse->GetPluginHandle();

		if (skse->isEditor) {
			_MESSAGE("loaded in editor, marking as incompatible");
			return false;
		}

		switch (skse->skseVersion) {
		case PACKED_SKSE_VERSION:
			break;
		default:
			_ERROR("This plugin is out of date with the installed version of SKSE.");
			return false;
		}

		return true;
	}

	bool SKSEPlugin_Load(const SKSEInterface * skse)
	{
		if (!g_branchTrampoline.Create(1024 * 64)) {
			_ERROR("couldn't create branch trampoline. this is fatal. skipping remainder of init process.");
			return false;
		}

		if (!g_localTrampoline.Create(1024 * 64)) {
			_ERROR("couldn't create codegen buffer. this is fatal. skipping remainder of init process.");
			return false;
		}

		// 0 = quest, 1 = player info, 2 = system
		SavedTabIndex = 2;

		// disable the remembercurrenttabindex callback
		SafeWrite32(RememberCurrentTabIdxAddr.GetUIntPtr(), 0x909090C3);

		// fix for map menu
		struct InstallGetSavedTabIndex_Code : Xbyak::CodeGenerator
		{
			InstallGetSavedTabIndex_Code(void * buf, uintptr_t funcAddr) : Xbyak::CodeGenerator(4096, buf)
			{
				Xbyak::Label retnLabel;
				Xbyak::Label funcLabel;

				sub(rsp, 0x20);

				mov(dl, byte[rax + 0x20]);
				mov(cl, cl);
				call(ptr[rip + funcLabel]);

				add(rsp, 0x20);

				jmp(ptr[rip + retnLabel]);

				L(funcLabel);
				dq(funcAddr);

				L(retnLabel);
				dq(GetCurrentTabIndexAddr.GetUIntPtr() + 0x5);
			}
		};

		void * codeBuf = g_localTrampoline.StartAlloc();
		InstallGetSavedTabIndex_Code code(codeBuf, GetFnAddr(&GetSavedTabIndex));
		g_localTrampoline.EndAlloc(code.getCurr());

		if (!g_branchTrampoline.Write5Branch(GetCurrentTabIndexAddr.GetUIntPtr(), uintptr_t(code.getCode())))
			return false;
		SafeWrite32(GetCurrentTabIndexAddr.GetUIntPtr() + 5, 0x90909090);

		return true;
	}
}
