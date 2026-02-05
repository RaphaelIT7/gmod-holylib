// HOLYLIB_PRIORITY_MODULE
#include "LuaInterface.h"
#include "detours.h"
#include "module.h"
#include "lua.h"

#if SYSTEM_LINUX
#include <dlfcn.h>
#include <link.h>
#include <signal.h>
#include <time.h>
#include <stdio.h>
#include <fcntl.h>
#include <elf.h>
#include <execinfo.h>
#endif
#include <atomic>
#include "iluashared.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

/*
	HolyLib's crash handler to hopefully create a useful crash dump without needing gdb setup
*/

class CCrashHandlerModule : public IModule
{
public:
#if SYSTEM_LINUX
	void Init(CreateInterfaceFn* appfn, CreateInterfaceFn* gamefn) override;
	void Shutdown() override;
#endif
	void Think(bool bSimulating) override;
	const char* Name() override { return "crashhandler"; };
	int Compatibility() override { return LINUX32 | LINUX64; };
};

static CCrashHandlerModule g_pCrashHandlerModule;
IModule* pCrashHandlerModule = &g_pCrashHandlerModule;

class LuaAlloc
{
public:
	static void* alloc(void* ud, void* ptr, size_t osize, size_t nsize)
	{
		if (nsize == 0) // No we don't free!
			return nullptr;

		LuaAlloc* luaAlloc = (LuaAlloc*)ud;

		void* newPtr = (void*)((uintptr_t)luaAlloc->buffer + (uintptr_t)luaAlloc->offset);
		luaAlloc->offset += nsize;
		return newPtr;
	}

private:
	char buffer[64 * 1024];
    size_t offset;
};

struct SignalData
{
	const char* crashOrigin;
	LuaAlloc luaAlloc;
};

static std::atomic<SignalData*> g_pSignalData;

#if SYSTEM_LINUX
/*
	Helper class since I am lazy
*/
class ScopedFileDescriptor
{
public:
	ScopedFileDescriptor(int fileDescriptor) : m_nFileDescriptor(fileDescriptor) {};
	~ScopedFileDescriptor() { if (m_nFileDescriptor < 0 && m_nFileDescriptor != STDERR_FILENO) { close(m_nFileDescriptor); } }
	ScopedFileDescriptor(const ScopedFileDescriptor&) = delete;
	ScopedFileDescriptor& operator=(const ScopedFileDescriptor&) = delete;
	operator int() const { return m_nFileDescriptor; }

private:
	int m_nFileDescriptor;
};

/*
	EFL stuff
	We load it as I wanna get the function names instead of just showing the offsets.
	The issue is that not really much is signal safe and your quite limited.

	Useful:
		https://en.wikipedia.org/wiki/Executable_and_Linkable_Format
*/
struct ElfSection
{
	off_t offset;
	size_t size;
};

struct ElfSections
{
	ElfSection debug_info;
	ElfSection debug_line;
	ElfSection debug_abbrev;
};

// Opens & Reads the file to find the debug sections
static void ReadElfSections(char* filePath, ElfSections& sections)
{
	if (!filePath || !filePath[0])
		return;

	// dprintf(STDERR_FILENO, "File: \"%s\"\n", filePath);
	int fd = open(filePath, O_RDONLY);
	if (fd < 0)
		return;

	ScopedFileDescriptor scopedFD(fd);
	ElfW(Ehdr) eh;
	if (read(fd, &eh, sizeof(eh)) != sizeof(eh))
		return;

	if (memcmp(eh.e_ident, ELFMAG, SELFMAG) != 0)
		return;

	if (eh.e_shoff == 0 || eh.e_shnum == 0)
		return;

	ElfW(Shdr) shdrs[128];
	if (eh.e_shnum > 128)
		return;

	if (lseek(fd, eh.e_shoff, SEEK_SET) < 0)
		return;

	if (read(fd, shdrs, eh.e_shnum * sizeof(ElfW(Shdr))) != (ssize_t)(eh.e_shnum * sizeof(ElfW(Shdr))))
		return;

	if (eh.e_shstrndx == SHN_UNDEF)
		return;

	ElfW(Shdr)& shstr = shdrs[eh.e_shstrndx];
	if (lseek(fd, shstr.sh_offset, SEEK_SET) < 0)
		return;

	// What the heck is this file???
	if (shstr.sh_size > 100000)
		return;

	char* shstrtab = (char*)alloca(shstr.sh_size);
	if (read(fd, shstrtab, shstr.sh_size) != (ssize_t)shstr.sh_size)
		return;

	for (int i = 0; i < eh.e_shnum; ++i)
	{
		const char* name = shstrtab + shdrs[i].sh_name;
		// dprintf(STDERR_FILENO, "%s - %s\n", filePath, name);
		if (!strcmp(name, ".debug_info"))
		{
			sections.debug_info.offset = shdrs[i].sh_offset;
			sections.debug_info.size = shdrs[i].sh_size;
		} else if (!strcmp(name, ".debug_abbrev"))
		{
			sections.debug_abbrev.offset = shdrs[i].sh_offset;
			sections.debug_abbrev.size = shdrs[i].sh_size;
		} else if (!strcmp(name, ".debug_line"))
		{
			sections.debug_line.offset = shdrs[i].sh_offset;
			sections.debug_line.size = shdrs[i].sh_size;
		}
	}
}

// Intended to live on the stack to be unaffected in case of memory corruption
class ModuleInfo
{
public:
	struct Entry
	{
		uintptr_t base;
		char path[270];
		ElfSections sections;
	};

	ModuleInfo()
	{
		memset(this, 0, sizeof(ModuleInfo));
		dl_iterate_phdr(ModuleInfo::PHDRCallback, this);
	}

	Entry* AddModule(uintptr_t baseAddress, const char* pFileName)
	{
		Entry& pEntry = m_pModules[m_nModules++];
		if (m_nModules >= MAX_MODULES)
			return nullptr;

		pEntry.base = baseAddress;
		if (pFileName && pFileName[0])
		{
			strncpy(pEntry.path, pFileName, sizeof(pEntry.path));
		} else
		{
			// Hacky workaround as the executable may be pFileName = ""
			Dl_info dlInfo;
			if (dladdr((void*)pEntry.base, &dlInfo) && dlInfo.dli_fname)
				strncpy(pEntry.path, dlInfo.dli_fname, sizeof(pEntry.path) - 1);
		}

		ReadElfSections(pEntry.path, pEntry.sections);

		return &pEntry;
	}

	static int PHDRCallback(struct dl_phdr_info *info, size_t size, void *data)
	{
		ModuleInfo* pModuleInfo = (ModuleInfo*)data;
		pModuleInfo->AddModule(info->dlpi_addr, info->dlpi_name);

		return 0;
	}

	const Entry* FindModule(uintptr_t nAddress)
	{
		Entry* foundEntry = nullptr;
		uintptr_t foundAddress = -1;
		for (int nModule = 0; nModule < m_nModules; ++nModule)
		{
			Entry& pEntry = m_pModules[nModule];
			// Find the closest base to our address
			if (nAddress >= pEntry.base && pEntry.base > foundAddress) 
			{
				foundAddress = pEntry.base;
				foundEntry = &pEntry;
			}
		}

		if (foundEntry)
			return foundEntry;

		Dl_info dlInfo;
		if (dladdr((void*)nAddress, &dlInfo) && dlInfo.dli_fname)
			foundEntry = AddModule((uintptr_t)dlInfo.dli_fbase, dlInfo.dli_fname);

		return foundEntry;
	}

	// pEntry can be a nullptr!
	void FindFunctionInfo(const Entry* pEntry, uintptr_t nAddress, const char** outName, uintptr_t* outOffset)
	{
		Dl_info dlInfo;
		if (dladdr((void*)nAddress, &dlInfo) && dlInfo.dli_sname)
		{
			strncpy(m_strReturnBuffer, dlInfo.dli_sname, sizeof(m_strReturnBuffer));
			*outName = m_strReturnBuffer;
			*outOffset = nAddress - (uintptr_t)dlInfo.dli_saddr;
			return;
		}

		uintptr_t offset = pEntry ? (nAddress - pEntry->base) : nAddress;

		m_strReturnBuffer[0] = '?';
		m_strReturnBuffer[1] = '?';
		m_strReturnBuffer[2] = '\0';

		*outName = m_strReturnBuffer;
		*outOffset = offset;
	}

private:
	static constexpr int MAX_MODULES = 64;
	Entry m_pModules[MAX_MODULES];
	int m_nModules = 0;

	// For FindFunctionName as when we return we still want it to be valid
	char m_strReturnBuffer[270];
};

static void RestoreDefaultHandler()
{
	struct sigaction sa{};
	sa.sa_handler = SIG_DFL;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGSEGV, &sa, nullptr);
}

/*
	Restores the original handler and returns to the code that threw the signal.
	This is to allow the server to properly crash and to create the default debug.log file
*/
static void ReturnSignal()
{
	RestoreDefaultHandler();

	sigreturn(nullptr);
}

static void GenerateCrashFileName(char* buffer, int bufferSize)
{
	time_t t = time(nullptr);

	struct tm timeInfo;
	localtime_r(&t, &timeInfo);

	strftime(buffer, bufferSize, "garrysmod/holylib/crashes/crash_%Y-%m-%d_%H:%M:%S.log", &timeInfo);
}

static void DumpRegister(int fd, gregset_t& registers, const char* name, int idx)
{
	int32_t sval = (int32_t)registers[idx];
	uint32_t uval = (uint32_t)registers[idx];

	dprintf(fd,
		"  %-8s 0x%08x  %d\n",
		name, uval, sval
	);
};

extern void AttemptLuaCallback();
static thread_local bool g_bIsInSignalHandler = false;
static void CrashHandler(int signal, siginfo_t* signalInfo, void* ucontext)
{
	if (g_bIsInSignalHandler)
	{
		ReturnSignal();
		return;
	}

	g_bIsInSignalHandler = true;

	char crashFileName[64];
	GenerateCrashFileName(crashFileName, sizeof(crashFileName));

	int fileDescriptor = ::open(crashFileName, O_CREAT | O_WRONLY | O_APPEND, 0644);
	if (fileDescriptor < 0)
		fileDescriptor = STDERR_FILENO;

	dprintf(STDERR_FILENO, "%s: %s crash log \"%s\"\n", fileDescriptor == STDERR_FILENO ? "Dumping crash log to console - Failed to write" : "Wrote", PROJECT_NAME, crashFileName);

	ModuleInfo pModuleInfo;

	dprintf(fileDescriptor, "===== CRASH REPORT =====\n");
	dprintf(fileDescriptor, "Signal: %d (%s)\n", signal, strsignal(signal));
	dprintf(fileDescriptor, "Fault address: %p\n", signalInfo->si_addr);
	dprintf(fileDescriptor, "Fault reason: ");
	switch(signalInfo->si_code)
	{
		case SEGV_MAPERR:
			if (signalInfo->si_addr == nullptr)
				dprintf(fileDescriptor, "Null pointer access\n");
			else
				dprintf(fileDescriptor, "Invalid pointer access\n");
			break;
		case SEGV_ACCERR:
			dprintf(fileDescriptor, "Invalid permissions memory access\n");
			break;
		default:
			dprintf(fileDescriptor, "Unknown\n");
			break;
	}

	ucontext_t* uc = (ucontext_t*)ucontext;
#if defined(__x86_64__)
	void* ip = (void*)uc->uc_mcontext.gregs[REG_RIP];
#elif defined(__i386__)
	void* ip = (void*)uc->uc_mcontext.gregs[REG_EIP];
#else
#error "Unsupported architecture"
#endif
	dprintf(fileDescriptor, "Instruction pointer: %p\n", ip);

	// Later goal is to figure out if we can safely call to Lua.
	// bool in_vphysics = strstr(module_name, "vphysics") != nullptr;

	const ModuleInfo::Entry* mod = pModuleInfo.FindModule((uintptr_t)ip);
	const char* moduleName = mod ? mod->path : "unknown";
	dprintf(fileDescriptor, "Crashed in module: %s\n", moduleName);

	dprintf(fileDescriptor, "Registers:\n");
#if defined(__x86_64__)
	DumpRegister(fileDescriptor, uc->uc_mcontext.gregs, "rax", REG_RAX);
	DumpRegister(fileDescriptor, uc->uc_mcontext.gregs, "rbx", REG_RBX);
	DumpRegister(fileDescriptor, uc->uc_mcontext.gregs, "rcx", REG_RCX);
	DumpRegister(fileDescriptor, uc->uc_mcontext.gregs, "rdx", REG_RDX);
	DumpRegister(fileDescriptor, uc->uc_mcontext.gregs, "rsi", REG_RSI);
	DumpRegister(fileDescriptor, uc->uc_mcontext.gregs, "rdi", REG_RDI);
	DumpRegister(fileDescriptor, uc->uc_mcontext.gregs, "rbp", REG_RBP);
	DumpRegister(fileDescriptor, uc->uc_mcontext.gregs, "rsp", REG_RSP);
	DumpRegister(fileDescriptor, uc->uc_mcontext.gregs, "r8",  REG_R8);
	DumpRegister(fileDescriptor, uc->uc_mcontext.gregs, "r9",  REG_R9);
	DumpRegister(fileDescriptor, uc->uc_mcontext.gregs, "r10", REG_R10);
	DumpRegister(fileDescriptor, uc->uc_mcontext.gregs, "r11", REG_R11);
	DumpRegister(fileDescriptor, uc->uc_mcontext.gregs, "r12", REG_R12);
	DumpRegister(fileDescriptor, uc->uc_mcontext.gregs, "r13", REG_R13);
	DumpRegister(fileDescriptor, uc->uc_mcontext.gregs, "r14", REG_R14);
	DumpRegister(fileDescriptor, uc->uc_mcontext.gregs, "r15", REG_R15);
	DumpRegister(fileDescriptor, uc->uc_mcontext.gregs, "rip", REG_RIP);
#elif defined(__i386__)
	DumpRegister(fileDescriptor, uc->uc_mcontext.gregs, "eax", REG_EAX);
	DumpRegister(fileDescriptor, uc->uc_mcontext.gregs, "ebx", REG_EBX);
	DumpRegister(fileDescriptor, uc->uc_mcontext.gregs, "ecx", REG_ECX);
	DumpRegister(fileDescriptor, uc->uc_mcontext.gregs, "edx", REG_EDX);
	DumpRegister(fileDescriptor, uc->uc_mcontext.gregs, "esi", REG_ESI);
	DumpRegister(fileDescriptor, uc->uc_mcontext.gregs, "edi", REG_EDI);
	DumpRegister(fileDescriptor, uc->uc_mcontext.gregs, "ebp", REG_EBP);
	DumpRegister(fileDescriptor, uc->uc_mcontext.gregs, "esp", REG_ESP);
	DumpRegister(fileDescriptor, uc->uc_mcontext.gregs, "eip", REG_EIP);
#else
	dprintf(fileDescriptor, "  GG\n");
#endif

	// ToDo: backtrace is not really signal safe! It could deadlock!
	constexpr int bufferSize = 64;
	void* buffer[bufferSize];
	int nptrs = backtrace(buffer, bufferSize);
	dprintf(fileDescriptor, "Backtrace (%d frames):\n", nptrs);
	for (int i = 0; i < nptrs; ++i)
	{
		uintptr_t addr = (uintptr_t)buffer[i];
		const ModuleInfo::Entry* fmod = pModuleInfo.FindModule(addr);
		uintptr_t offset = 0;
		const char* fileName;
		pModuleInfo.FindFunctionInfo(fmod, addr, &fileName, &offset);

		dprintf(fileDescriptor, "  %p: %s + 0x%lx [%s]\n", (void*)addr, fileName, offset, fmod ? fmod->path : "UNKNOWN");
	}

	GarrysMod::Lua::ILuaShared* pLuaShared = Lua::GetShared();
	if (pLuaShared)
		dprintf(fileDescriptor, pLuaShared->GetStackTraces());


	SignalData signalData;
	memset(&signalData, 0, sizeof(SignalData));
	signalData.crashOrigin = moduleName;

	// Safe since we know 100% that the stack remains.
	g_pSignalData.store(&signalData);

	AttemptLuaCallback();
	ReturnSignal();

	g_bIsInSignalHandler = false;
}

static void SetupCrashHandler()
{
	static uint8_t signalStackBuffer[200000 + sizeof(ModuleInfo) + sizeof(SignalData)];
	stack_t signalStack{};
	signalStack.ss_sp = signalStackBuffer;
	signalStack.ss_size = sizeof(signalStackBuffer);
	signalStack.ss_flags = 0;
	sigaltstack(&signalStack, nullptr);

	struct sigaction signalAction{};
	signalAction.sa_sigaction = CrashHandler;
	sigemptyset(&signalAction.sa_mask);
	signalAction.sa_flags = SA_SIGINFO | SA_ONSTACK;
	sigaction(SIGSEGV, &signalAction, nullptr);
}

void CCrashHandlerModule::Init(CreateInterfaceFn* appfn, CreateInterfaceFn* gamefn)
{
	(void)appfn;
	(void)gamefn;

	SetupCrashHandler();
}

void CCrashHandlerModule::Shutdown()
{
	RestoreDefaultHandler();
}
#endif

/*
	Lua callback part

	At this point we wrote the crash long.
	Now we want to attempt to call a Lua hook to let scripts shut down properly.
	If we crash here, it'll be fine.
*/
static int g_nHookRegistry = -1;
static std::atomic<bool> g_bExpectingCrash = false; // Something crashed and Lua was called / attempted. Now we allow to rest in piece
static std::atomic<bool> g_bThreadAwaitingLua = false; // A thread crashed and is waiting for the main thread
static void DoLuaCallback()
{
	if (!g_Lua)
		return;

	// We stop the GC in case a crash is related to an memory allocator or some bs
	Util::func_lua_gc(g_Lua->GetState(), LUA_GCSTOP, 0);

	SignalData* signalData = g_pSignalData.load();
	if (!signalData) // No signal data GG
		return;

	Util::func_lua_setallocf(g_Lua->GetState(), LuaAlloc::alloc, &signalData->luaAlloc);

	if (Lua::PushHook("HolyLib:OnServerCrash"))
	{
		g_Lua->PushString(signalData->crashOrigin);
		g_Lua->CallFunctionProtected(2, 0, true);
	}
}

void AttemptLuaCallback()
{
	if (g_bExpectingCrash.load())
		return;

	if (ThreadInMainThread())
	{
		DoLuaCallback();
		g_bThreadAwaitingLua.store(false);
		g_bExpectingCrash.store(true);
	} else {
		// We crashed on another thread - so Lua should be good to use
		g_bThreadAwaitingLua.store(true);
		while (g_bThreadAwaitingLua.load())
			ThreadSleep(10);
	}
}

void CCrashHandlerModule::Think(bool bSimulating)
{
	(void)bSimulating;

	if (g_bThreadAwaitingLua.load())
		AttemptLuaCallback();
}