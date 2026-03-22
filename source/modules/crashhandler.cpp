// HOLYLIB_PRIORITY_MODULE
#include "LuaInterface.h"
#include "detours.h"
#include "module.h"
#include "lua.h"
#include "versioninfo.h"

#if SYSTEM_LINUX
#include <dlfcn.h>
#include <link.h>
#include <signal.h>
#include <time.h>
#include <stdio.h>
#include <fcntl.h>
#include <elf.h>
#include <execinfo.h>
#include <ucontext.h>
#include <linux/prctl.h>
#include <sys/ptrace.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <sys/user.h>
#endif
#include <atomic>
#include <deque>
#include "iluashared.h"
#include <inttypes.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

/*
	HolyLib's crash handler to hopefully create a useful crash dump without needing gdb setup
	The goal of the crash handler is to ATTEMPT crash dumping even with unsafe signal functions!
	Once it's done, it'll trigger the original crash to let any attached gdb also trigger for a proper debug log yet not all servers have gdb installed.

	If we fail, it's fine, if we succeed, yay.
*/

class CCrashHandlerModule : public IModule
{
public:
	void LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit) override;
	void LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua) override;
	void Init(CreateInterfaceFn* appfn, CreateInterfaceFn* gamefn) override;
	void Shutdown() override;
	void InitDetour(bool bPreServer) override;
	void Think(bool bSimulating) override;
	const char* Name() override { return "crashhandler"; };
	int Compatibility() override { return LINUX32 | LINUX64; };
};

#if _WIN32
// Stuff for VS to not complain
#define dprintf(num, ...) printf(__VA_ARGS__)
#endif

static CCrashHandlerModule g_pCrashHandlerModule;
IModule* pCrashHandlerModule = &g_pCrashHandlerModule;

/*
	This a custom lua allocator due to Lua being highly unstable when it crashed.
	So to reach mostly stable conditions we use our own simple allocator.
	It is not meant to be complex in any way, it purely exists for the temporary use inside the signal handler.
*/
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

/*
	NOTE:
	This is always stack allocated inside the signal handler thread
	this is due to the signal handler having his own dedicated stack which we've setup
	This is safe only because the signal handler thread sleeps while we call back to the main thread and use the SignalData.
	If the signal thread ever proceeds before the main thread finished using the SignalData (specifically the luaAlloc for the lua callback) then we will definitely die.
	Though that cannot happen at all with our current code as the signal handler only proceeds once the main thread set that it's done.
*/
struct SignalData
{
	const char* crashOrigin;
	int fileDescriptor;
	LuaAlloc luaAlloc;
};

static std::atomic<SignalData*> g_pSignalData;
static std::atomic<bool> g_bInducedCrash = false;

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

	ToDo:
		Implement DWARF reading - https://en.wikipedia.org/wiki/DWARF
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
	static std::atomic<int> g_nGenerationCount = 0;
	int count = g_nGenerationCount.load();
	g_nGenerationCount.store(count+1);

	time_t t = time(nullptr);

	struct tm timeInfo;
	localtime_r(&t, &timeInfo);

	char pTemp[255];
	strftime(pTemp, sizeof(pTemp), "garrysmod/holylib/crashes/crash_%Y-%m-%d_%H:%M:%S", &timeInfo);
	snprintf(buffer, bufferSize, "%s-%i.log", pTemp, count);
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

struct SavedCrash
{
	int signal;
	siginfo_t info;
	ucontext_t context;
};

static SavedCrash g_pSavedCrash;
static std::atomic<bool> g_bAttemptedBacktrace = false;

static void DumpLuaState(int fileDescriptor)
{
	// Unsafe but we want to dump the main state!
	GarrysMod::Lua::ILuaShared* pLuaShared = Lua::GetShared();
	if (pLuaShared)
	{
		dprintf(fileDescriptor, "Lua Stack trace:\n");
		dprintf(fileDescriptor, pLuaShared->GetStackTraces());
	}

	dprintf(fileDescriptor, "Lua Stack values:\n");

	lua_State* pState = g_Lua->GetState();
	TValue* pBase = pState->base;
	int nTop = (int)(pState->top - pState->base);
	dprintf(fileDescriptor, "  Stack size: %i\n", nTop);
	for (int i=0; i<nTop; ++i)
	{
		dprintf(fileDescriptor, "  %i: %s\n", i, Lua::TValueToString(pBase));
		pBase++;
	}
}

static bool AttemptLuaCallback(bool bMainThreadCrash);
static void CrashHandler(int signal, siginfo_t* signalInfo, void* ucontext)
{
	// ToDo: Fix this in the future
	// If we hit this, we- the handler crashed in backtrace, so we restore the original signal for the core dump / gdb to be right.
	if (g_bAttemptedBacktrace.load())
	{
		setcontext(&g_pSavedCrash.context);
		ReturnSignal();
		return;
	} else {
		g_pSavedCrash.signal = signal;
		memcpy(&g_pSavedCrash.info, signalInfo, sizeof(siginfo_t));
		memcpy(&g_pSavedCrash.context, (ucontext_t*)ucontext, sizeof(ucontext_t));
	}

	static thread_local bool g_bIsInSignalHandler = false;

	// If this happens, we crashed in our handler, so let's skip that and let gdb catch the original crash
	if (g_bIsInSignalHandler)
	{
		ReturnSignal();
		return;
	}

	g_bIsInSignalHandler = true;

	ModuleInfo pModuleInfo;

	char crashFileName[64];
	GenerateCrashFileName(crashFileName, sizeof(crashFileName));

	int fileDescriptor = ::open(crashFileName, O_CREAT | O_WRONLY | O_APPEND, 0644);
	if (fileDescriptor < 0)
		fileDescriptor = STDERR_FILENO;

	dprintf(STDERR_FILENO, "%s: %s crash log \"%s\"\n", fileDescriptor == STDERR_FILENO ? "Dumping crash log to console - Failed to write" : "Wrote", PROJECT_NAME, crashFileName);

	dprintf(fileDescriptor, "===== CRASH REPORT =====\n");
	dprintf(fileDescriptor, "Signal: %d (%s)\n", signal, strsignal(signal));
	dprintf(fileDescriptor, "Fault address: %p\n", signalInfo->si_addr);
	dprintf(fileDescriptor, "Fault reason: ");
	switch(signalInfo->si_code)
	{
		case SEGV_MAPERR:
			if (signalInfo->si_addr == nullptr)
				dprintf(fileDescriptor, "Null pointer access\n");
			else {
				if ((uintptr_t)signalInfo->si_addr < 0x5000)
					dprintf(fileDescriptor, "Invalid pointer access (probably accessed a null pointer offset)\n");
				else
					dprintf(fileDescriptor, "Invalid pointer access\n");
			}
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

	dprintf(fileDescriptor, "HolyLib: %s\n", HolyLib_GetPluginDescription());

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

	// If we re-entered it's 99% that we crashed when calling backtrace!
	if (g_bInducedCrash.load())
	{
		DumpLuaState(fileDescriptor);

		dprintf(fileDescriptor, "Backtrace generation skipped due to unsafe conditions!\n");
	} else {
		// ToDo: backtrace is not really signal safe! It could deadlock!
		constexpr int bufferSize = 255;
		void* buffer[bufferSize];
		g_bAttemptedBacktrace.store(true);
		int nptrs = backtrace(buffer, bufferSize);
		g_bAttemptedBacktrace.store(false);
		dprintf(fileDescriptor, "Backtrace (%d frames):\n", nptrs);
		for (int i = 0; i < nptrs; ++i)
		{
			uintptr_t addr = (uintptr_t)buffer[i];
			const ModuleInfo::Entry* fmod = pModuleInfo.FindModule(addr);
			uintptr_t offset = 0;
			const char* fileName;
			pModuleInfo.FindFunctionInfo(fmod, addr, &fileName, &offset);

			dprintf(fileDescriptor, "  %p: %s + 0x%" PRIxPTR " [%s]\n", (void*)addr, fileName, offset, fmod ? fmod->path : "UNKNOWN");
		}
	}

	SignalData signalData;
	memset(&signalData, 0, sizeof(SignalData));
	signalData.crashOrigin = moduleName;
	signalData.fileDescriptor = fileDescriptor;

	// Safe since we know 100% that the stack remains.
	g_pSignalData.store(&signalData);

	if (!AttemptLuaCallback(true) && !g_bInducedCrash.load())
		DumpLuaState(fileDescriptor);

	ReturnSignal();

	g_bIsInSignalHandler = false;
}

static void SetupCrashHandlerStack()
{
	static uint8_t signalStackBuffer[200000 + sizeof(ModuleInfo) + sizeof(SignalData)];
	stack_t signalStack{};
	signalStack.ss_sp = signalStackBuffer;
	signalStack.ss_size = sizeof(signalStackBuffer);
	signalStack.ss_flags = 0;
	sigaltstack(&signalStack, nullptr);
}

static void SetupCrashHandler()
{
	struct sigaction signalAction{};
	signalAction.sa_sigaction = CrashHandler;
	sigemptyset(&signalAction.sa_mask);
	signalAction.sa_flags = SA_SIGINFO | SA_ONSTACK | SA_NODEFER;
	sigaction(SIGSEGV, &signalAction, nullptr);
	sigaction(SIGUSR1, &signalAction, nullptr);

	prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY); // Just in case
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
static void DoLuaCallback(bool bMainThreadCrash)
{
	if (!g_Lua)
		return;

	SignalData* signalData = g_pSignalData.load();
	if (!signalData) // No signal data GG
		return;

	if (bMainThreadCrash && !g_bInducedCrash.load())
	{
		GarrysMod::Lua::ILuaShared* pLuaShared = Lua::GetShared();
		if (pLuaShared)
		{
			dprintf(signalData->fileDescriptor, "Lua Stack trace:\n");
			dprintf(signalData->fileDescriptor, pLuaShared->GetStackTraces());
		}

		dprintf(signalData->fileDescriptor, "Lua Stack values:\n");

		lua_State* pState = g_Lua->GetState();
		TValue* pBase = pState->base;
		int nTop = (int)(pState->top - pState->base);
		dprintf(signalData->fileDescriptor, "  Stack size: %i\n", nTop);
		for (int i=0; i<nTop; ++i)
		{
			dprintf(signalData->fileDescriptor, "  %i: %s\n", i, Lua::TValueToString(pBase));
			pBase++;
		}
	}

	// We stop the GC in case a crash is related to an memory allocator or some bs
	Util::func_lua_gc(g_Lua->GetState(), LUA_GCSTOP, 0);
	Util::func_lua_setallocf(g_Lua->GetState(), LuaAlloc::alloc, &signalData->luaAlloc);

	if (Lua::PushHook("HolyLib:OnServerCrash"))
	{
		g_Lua->PushString(signalData->crashOrigin);
		g_Lua->CallFunctionProtected(2, 0, true);
	}
}

static bool AttemptLuaCallback(bool bMainThreadCrash)
{
	if (g_bExpectingCrash.load())
		return false;

	if (ThreadInMainThread())
	{
		DoLuaCallback(bMainThreadCrash);
		g_bThreadAwaitingLua.store(false);
		g_bExpectingCrash.store(true);
		return true;
	} else {
		// We crashed on another thread - so Lua should be good to use
		g_bThreadAwaitingLua.store(true);
		// We wait at best 100ms before just continuing, BUT as fallback behavior, we will then dump the main lua state into the dump too
		for (int i=0; i<10; ++i)
		{
			ThreadSleep(10);
			if (!g_bThreadAwaitingLua.load())
				break;
		}

		return !g_bThreadAwaitingLua.load();
	}
}

static ThreadId_t g_nMainThreadID = -1;
// Unlike our crash handler, we here will be in a more "safe" context.
// ToDo / WIP:
// We want to attach ourselves to the thread and attempt to get a backtrace, BUT we need to figure out a way to get the stack frames safely for unwinding :/
// I would maybe use libunwind, BUT that is not installed in containers, so we must find our own way as I don't think GMod compiles with frame pointers...
static void DumpMainThread()
{
#if SYSTEM_LINUX
	if (ptrace(PTRACE_ATTACH, g_nMainThreadID, nullptr, nullptr) == -1)
		return;

	int status = 0;
	waitpid(g_nMainThreadID, &status, __WALL);

	if (!WIFSTOPPED(status))
		return;

	user_regs_struct regs;
    if (ptrace(PTRACE_GETREGS, g_nMainThreadID, nullptr, &regs) == -1)
        return;
#endif
}

static inline void ExecuteMainThread() // So you have chosen... death
{
	// ToDo: Figure out why backtrace in the handler crashes - maybe skip it if we were the one forcing it
	//       & dump lua regardless of if were on the main thread or not?
	g_bInducedCrash.store(true);
#if SYSTEM_LINUX
	// pthread_kill(g_nMainThreadID, SIGTRAP); // SIGTRAP seems more responsive, yet our signal handler will never be called :sob:
	pthread_kill(g_nMainThreadID, SIGSEGV); // idk why but SIGTRAP works far better though seems to break the backtracing?
	ThreadSleep(3);
	pthread_kill(g_nMainThreadID, SIGUSR1);
#endif
}

static bool ShoudExecuteThreadedCommand(const char* cmd, int cmdLen)
{
	if (!cmd || cmdLen <= 0)
		return false;

	std::string_view strCmd(cmd, cmdLen);
	if (strCmd.rfind("holylib_crashserver", 0) == 0)
	{
		Warning(PROJECT_NAME " - crashhandler: Called holylib_crashserver, nuking main thread!\n");
		ExecuteMainThread();
		return true;
	}
#if MODULE_EXISTS_HOLYLUA
	else if (strCmd.rfind("lua_run_holylib", 0) == 0) {
		std::string_view args = strCmd.substr(15);
		while (!args.empty() && args.front() == ' ') args.remove_prefix(1);

		Lua::ScopedThreadAccess pThreadScope();
		Lua::ThreadAccess pAccess(GetHolyLuaInterface());
		if (pAccess.IsValid())
			pAccess.GetLua()->RunString("RunString", "", args.data(), true, true);

		return true;
	}
#endif
	
	return false;
}

static std::mutex g_pConsoleMutex;
static std::deque<const char*> g_pConsoleBuffer;
static std::atomic<void*> g_pConsole = nullptr;
static Detouring::Hook detour_CTextConsoleUnix_GetLine;
#if ARCHITECTURE_X86_64
static std::atomic<int> g_nConsoleThreadState = ThreadState::STATE_NOTRUNNING;
// This thread effectively only exists on 64x since it uses an older or newer? implementation of CTextConsoleUnix
static SIMPLETHREAD_RETURNVALUE ConsoleThread(void* data)
{
	// Got no valid detour!
	Symbols::CTextConsoleUnix_GetLine func_CTextConsoleUnix_GetLine = detour_CTextConsoleUnix_GetLine.GetTrampoline<Symbols::CTextConsoleUnix_GetLine>();
	if (!func_CTextConsoleUnix_GetLine)
	{
		Warning(PROJECT_NAME " - crashhandler: ConsoleThread stopped due to missing CTextConsoleUnix::GetLine\n");
		g_nConsoleThreadState.store(ThreadState::STATE_NOTRUNNING);
		return 0;
	}

	while (g_nConsoleThreadState.load() == ThreadState::STATE_RUNNING)
	{
		std::vector<const char*> pCommandBuffer;
		char szBuf[511];
		void* pConsole = g_pConsole.load();
		if (pConsole)
		{
			char* cmd = func_CTextConsoleUnix_GetLine(pConsole);
			while (cmd)
			{
				V_snprintf(szBuf, sizeof(szBuf), "%s\n", cmd);
				cmd = func_CTextConsoleUnix_GetLine(pConsole);

				int cmdlen = strlen(szBuf) + 1;
				if (ShoudExecuteThreadedCommand(szBuf, cmdlen))
					continue;

				char* pStr = new char[cmdlen];
				V_strncpy(pStr, szBuf, cmdlen);

				pCommandBuffer.push_back(pStr);
			}
		}

		if (!pCommandBuffer.empty())
		{
			std::lock_guard<std::mutex> lock(g_pConsoleMutex);
			for (const char* cmd : pCommandBuffer)
				g_pConsoleBuffer.push_back(cmd);
		}

		ThreadSleep(5);
	}

	g_nConsoleThreadState.store(ThreadState::STATE_NOTRUNNING);
	return 0;
}
#endif

#if ARCHITECTURE_X86
// add_command is threaded in the engine already! See editline_threadproc in dedicated/console/TextConsoleUnix.cpp
// NOTE: add_command blocks the thread it's running in until the main thread handles the command, so for our case we just put them into the buffer.
static Detouring::Hook detour_add_command;
static bool
#if SYSTEM_LINUX
__attribute__((regparm(2)))
#endif
hook_add_command(const char *cmd, int cmd_len) // yay this is a fking __usercall
{
	if (!cmd || cmd_len <= 0)
		return false;

	if (ShoudExecuteThreadedCommand(cmd, cmd_len))
		return true;

	std::lock_guard<std::mutex> lock(g_pConsoleMutex);

	char* pStr = new char[cmd_len];
	V_strncpy(pStr, cmd, cmd_len);

	g_pConsoleBuffer.push_back(pStr);
	return true;
}
#endif

// it get's called all the way until NULL is returned.
#if ARCHITECTURE_X86
static const char* hook_CTextConsoleUnix_GetLine(void* _this, int index, char *buf, size_t buflen)
#else
static const char* hook_CTextConsoleUnix_GetLine(void* _this)
#endif
{
	if (g_pConsole.load() == nullptr)
		g_pConsole.store(_this);

#if ARCHITECTURE_X86_64
	// Fallback to avoid breaking the entire console input when our thread dies
	if (g_nConsoleThreadState.load() == ThreadState::STATE_NOTRUNNING)
		return detour_CTextConsoleUnix_GetLine.GetTrampoline<Symbols::CTextConsoleUnix_GetLine>()(_this);
#endif

	static thread_local std::deque<const char*> pLocalBuffer;
#if ARCHITECTURE_X86_64
	static thread_local char pBuffer[511];
#endif

	if (pLocalBuffer.empty())
	{
		std::lock_guard<std::mutex> lock(g_pConsoleMutex);
		if (g_pConsoleBuffer.empty())
			return nullptr;

		// We copy it over to avoid performance issues from locking so often
		std::swap(pLocalBuffer, g_pConsoleBuffer);
		g_pConsoleBuffer.clear();
	}

	const char* cmd = pLocalBuffer.front();
#if ARCHITECTURE_X86
	V_strncpy(buf, cmd, buflen);
#else
	V_strncpy(pBuffer, cmd, sizeof(pBuffer));
#endif

	delete[] cmd;
	pLocalBuffer.pop_front();

#if ARCHITECTURE_X86
	return buf;
#else
	return pBuffer;
#endif
}

static ConVar crashhandler_crashtime("holylib_crashhandler_crashtime", "10000", 0, "Time in ms after which the crash handler will catch and nuke the server");
static constexpr int g_nThreadSleep = 10;
static std::atomic<int> g_nLagCount = 0; // one count = (g_nThreadSleep)ms lag.
static std::atomic<bool> g_bSkipWatcher = false;
static std::atomic<int> g_nWatcherThreadState = ThreadState::STATE_NOTRUNNING;
static SIMPLETHREAD_RETURNVALUE CrashWatcherThread(void* data)
{
	// ThreadSetDebugName(ThreadGetCurrentId(), PROJECT_NAME " - CrashWatcher");
	while (g_nWatcherThreadState.load() == ThreadState::STATE_RUNNING)
	{
		ThreadSleep(g_nThreadSleep);

		if (g_bSkipWatcher.load() || g_pModuleManager.GetServerState() != ServerState::RUNNING)
		{
			g_nLagCount.store(0);
			continue;
		}

		if (++g_nLagCount > (crashhandler_crashtime.GetInt() / g_nThreadSleep))
		{
#if MODULE_EXISTS_HOLYLUA
			// Let's consult the HolyLua state to determine whether to force a crash or not
			{
				Lua::ScopedThreadAccess pThreadScope;
				Lua::ThreadAccess pAccess(GetHolyLuaInterface());
				if (pAccess.IsValid())
				{
					GarrysMod::Lua::ILuaInterface* pLua = pAccess.GetLua();
					if (Lua::PushHook("HolyLib:DetermineServerCrash", pLua))
					{
						pLua->PushNumber(g_nLagCount.load() * g_nThreadSleep); // Push lag time in ms
						if (pLua->CallFunctionProtected(2, 1, true))
						{
							bool bDontCrash = pLua->GetBool(-1);
							pLua->Pop(1);

							if (bDontCrash)
							{
								g_nLagCount.store(0);
								continue;
							}
						}
					}
				}
			}
#endif
			printf(PROJECT_NAME " - crashhandler: Detected a freeze, attempting termination...\n");
			ThreadSleep(5); // Sleep for the printf to reach the console (in testing it sometimes just never appeared)
			ExecuteMainThread();
			break;
		}
	}

	g_nWatcherThreadState.store(ThreadState::STATE_NOTRUNNING);
	return 0;
}

// I don't like exposing these as now people could mishandle the watcher :/
LUA_ASM_FUNCTION_STATIC(crashhandler_DisableWatcher)
{
	g_bSkipWatcher.store(true);
	g_nLagCount.store(0);
	return;
}

// We also reset the counter to avoid issues
// in tests once they were done the watcher thread immediately catched in some cases as the counter was high.
LUA_ASM_FUNCTION_STATIC(crashhandler_EnableWatcher)
{
	g_bSkipWatcher.store(false);
	g_nLagCount.store(0);
	return;
}

LUA_ASM_FUNCTION_STATIC(crashhandler_ResetWatcher)
{
	g_nLagCount.store(0);
	return;
}

void CCrashHandlerModule::LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit)
{
	if (bServerInit)
		return;

	Util::StartTable(pLua);
		LUA_AddJITFunc(pLua, CFUNC_TYPE_VOID, crashhandler_DisableWatcher, "DisableWatcher");
		LUA_AddJITFunc(pLua, CFUNC_TYPE_VOID, crashhandler_EnableWatcher, "EnableWatcher");
		LUA_AddJITFunc(pLua, CFUNC_TYPE_VOID, crashhandler_ResetWatcher, "ResetWatcher");
	Util::FinishTable(pLua, "crashhandler");
}

void CCrashHandlerModule::LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua)
{
	Util::NukeTable(pLua, "crashhandler");
}

void CCrashHandlerModule::Think(bool bSimulating)
{
	(void)bSimulating;

	if (g_bThreadAwaitingLua.load())
		AttemptLuaCallback(false);

	g_nLagCount.store(0);
}

static ThreadHandle_t g_pCrashWatcher = nullptr;
static ThreadHandle_t g_pThreadedConsole = nullptr;
void CCrashHandlerModule::Init(CreateInterfaceFn* appfn, CreateInterfaceFn* gamefn)
{
	(void)appfn;
	(void)gamefn;

	g_nMainThreadID = ThreadGetCurrentId();

	g_nWatcherThreadState.store(ThreadState::STATE_RUNNING);
	g_pCrashWatcher = CreateSimpleThread((ThreadFunc_t)CrashWatcherThread, this);

#if ARCHITECTURE_X86_64
	g_nConsoleThreadState.store(ThreadState::STATE_RUNNING);
	g_pThreadedConsole = CreateSimpleThread((ThreadFunc_t)ConsoleThread, this);
#endif

	g_pFullFileSystem->CreateDirHierarchy("holylib/crashes/", "MOD");
#if SYSTEM_LINUX
	SetupCrashHandlerStack();
	SetupCrashHandler();
#endif
}

void CCrashHandlerModule::Shutdown()
{
	if (g_pCrashWatcher)
	{
		if (g_nWatcherThreadState.load() != ThreadState::STATE_NOTRUNNING)
		{
			g_nWatcherThreadState.store(ThreadState::STATE_SHOULD_SHUTDOWN);
			while (g_nWatcherThreadState.load() != ThreadState::STATE_NOTRUNNING) // Wait for shutdown
				ThreadSleep(0);
		}

		ReleaseThreadHandle(g_pCrashWatcher);
		g_pCrashWatcher = nullptr;
	}

#if ARCHITECTURE_X86_64
	if (g_pThreadedConsole)
	{
		if (g_nConsoleThreadState.load() != ThreadState::STATE_NOTRUNNING)
		{
			g_nConsoleThreadState.store(ThreadState::STATE_SHOULD_SHUTDOWN);
			while (g_nConsoleThreadState.load() != ThreadState::STATE_NOTRUNNING) // Wait for shutdown
				ThreadSleep(0);
		}

		ReleaseThreadHandle(g_pThreadedConsole);
		g_pThreadedConsole = nullptr;
	}
#endif

#if SYSTEM_LINUX
	RestoreDefaultHandler();
#endif
}

void CCrashHandlerModule::InitDetour(bool bPreServer)
{
	if (!bPreServer)
		return;

	SourceSDK::FactoryLoader dedicated_loader("dedicated");
#if ARCHITECTURE_X86
	Detour::Create(
		&detour_add_command, "add_command",
		dedicated_loader.GetModule(), Symbols::add_commandSym,
		(void*)hook_add_command, m_pID
	);
#endif

	Detour::Create(
		&detour_CTextConsoleUnix_GetLine, "CTextConsoleUnix::GetLine",
		dedicated_loader.GetModule(), Symbols::CTextConsoleUnix_GetLineSym,
		(void*)hook_CTextConsoleUnix_GetLine, m_pID
	);
}