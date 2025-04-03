#include "httplib.h"
#include "LuaInterface.h"
#include "module.h"
#include "lua.h"
#include <baseclient.h>
#include <inetchannel.h>
#include <netadr.h>
#include "unordered_set"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CHTTPServerModule : public IModule
{
public:
	virtual void LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit) OVERRIDE;
	virtual void LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua) OVERRIDE;
	virtual void Think(bool bSimulating) OVERRIDE;
	virtual const char* Name() { return "httpserver"; };
	virtual int Compatibility() { return LINUX32 | LINUX64 | WINDOWS32 | WINDOWS64; };
};

static CHTTPServerModule g_pHttpServerModule;
IModule* pHttpServerModule = &g_pHttpServerModule;

struct HttpResponse {
	bool m_bSetContent = false;
	bool m_bSetRedirect = false;
	bool m_bSetHeader = false;
	int m_iRedirectCode = 302;
	std::string m_strContent = "";
	std::string m_strContentType = "text/plain";
	std::string m_strRedirect = "";
	std::unordered_map<std::string, std::string> m_pHeaders;

	inline void DoResponse(httplib::Response& pResponse)
	{
		if (m_bSetContent)
			pResponse.set_content(m_strContent, m_strContentType);

		if (m_bSetRedirect)
			pResponse.set_redirect(m_strRedirect, m_iRedirectCode);

		if (m_bSetHeader)
			for (auto& [key, value] : m_pHeaders)
				pResponse.set_header(key, value);
	}
};

struct PreparedHttpResponse {
	HttpResponse m_pResponse;
	std::string m_strPath = "";
	std::string m_strMethod = "";
	std::string m_strBody = "";
	std::unordered_map<std::string, std::string> m_pRequiredHeaders;

	inline bool ShouldRespond(const httplib::Request& pRequest)
	{
		if (pRequest.method != m_strMethod)
			return false;

		if (pRequest.path != m_strPath)
			return false;

		if (pRequest.body != m_strBody)
			return false;

		if (m_pRequiredHeaders.size() > 0)
		{
			for (auto& [key, val] : m_pRequiredHeaders)
			{
				auto it = pRequest.headers.find(key);
				if (it == pRequest.headers.end())
					return false;

				if (it->second != val)
					return false;
			}
		}

		return true;
	}

	inline void DoResponse(httplib::Response& pResponse)
	{
		m_pResponse.DoResponse(pResponse);
	}
};

struct HttpRequest {
	~HttpRequest();

	bool m_bHandled = false;
	bool m_bDelete = false; // We only delete from the main thread.
	int m_iFunction = -1;
	std::string m_strPath;
	HttpResponse m_pResponseData;
	httplib::Response m_pResponse;
	httplib::Request m_pRequest;
	int m_pClientUserID = -1;
};

enum
{
	HTTPSERVER_ONLINE,
	HTTPSERVER_OFFLINE
};

class HttpServer;
static std::unordered_set<HttpServer*> g_pHttpServers;
class HttpServer
{
public:
	HttpServer() {
		g_pHttpServers.insert(this);
	}

	~HttpServer()
	{
		if (!ThreadInMainThread())
		{
			Warning(PROJECT_NAME ": Something deleted a HttpServer from another thread!\n"); // Spooky leaking references.
			return;
		}

		if (!g_Lua)
			return;

		for (auto& ref : m_pHandlerReferences)
			Util::ReferenceFree(ref, "HttpServer::~HttpServer - Handler references");

		m_pHandlerReferences.clear();

		// Should we lock the mutex?
		// Naaa it should be fine as the httpserver should NOT be running anymore.
		for (auto& [_, vec] : m_pPreparedResponses)
		{
			for (auto& pPreparedResponse : vec)
			{
				delete pPreparedResponse;
			}
		}
		m_pPreparedResponses.clear();

		g_pHttpServers.erase(this);
	}

	void Start(const char* address, unsigned short port);
	void Stop();
	void Think();

#if ARCHITECTURE_IS_X86_64
	static long long unsigned Server(void* params)
#else
	static unsigned Server(void* params)
#endif
	{
		HttpServer* pServer = (HttpServer*)params;
		pServer->GetServer().listen(pServer->GetAddress(), pServer->GetPort());

		return 0;
	}

	void Get(const char* path, int func, bool ipWhitelist)
	{
		m_pServer.Get(path, CreateHandler(path, func, ipWhitelist));
	}

	void Post(const char* path, int func, bool ipWhitelist)
	{
		m_pServer.Post(path, CreateHandler(path, func, ipWhitelist));
	}

	void Put(const char* path, int func, bool ipWhitelist)
	{
		m_pServer.Put(path, CreateHandler(path, func, ipWhitelist));
	}

	void Patch(const char* path, int func, bool ipWhitelist)
	{
		m_pServer.Patch(path, CreateHandler(path, func, ipWhitelist));
	}

	void Delete(const char* path, int func, bool ipWhitelist)
	{
		m_pServer.Delete(path, CreateHandler(path, func, ipWhitelist));
	}

	void Options(const char* path, int func, bool ipWhitelist)
	{
		m_pServer.Options(path, CreateHandler(path, func, ipWhitelist));
	}

	httplib::Server::Handler CreateHandler(const char* path, int func, bool ipWhitelist);

	void AddPreparedResponse(int userID, PreparedHttpResponse* pResponse)
	{
		m_pPreparedResponsesMutex.Lock();
		auto it = m_pPreparedResponses.find(userID);
		if (it == m_pPreparedResponses.end())
		{
			std::vector<PreparedHttpResponse*> pResponses;
			pResponses.push_back(pResponse);

			m_pPreparedResponses[userID] = pResponses;
			m_pPreparedResponsesMutex.Unlock();
			return;
		}

		it->second.push_back(pResponse);
		m_pPreparedResponsesMutex.Unlock();
	}

public:
	httplib::Server& GetServer() { return m_pServer; };
	unsigned char GetStatus() { return m_iStatus; };
	std::string& GetAddress() { return m_strAddress; };
	unsigned short GetPort() { return m_iPort; };
	void SetThreadSleep(unsigned int threadSleep) { m_iThreadSleep = threadSleep; };
	std::string& GetName() { return m_strName; };
	void SetName(std::string strName) { m_strName = strName; };

	void ClearDisconnectedClient(int userID)
	{
		auto it = m_pPreparedResponses.find(userID);
		if (it == m_pPreparedResponses.end())
			return;

		for (auto& pPreparedResponse : it->second)
		{
			delete pPreparedResponse;
		}

		m_pPreparedResponses.erase(it);
	}

private:
	unsigned char m_iStatus = HTTPSERVER_OFFLINE;
	unsigned short m_iPort = 0;
	unsigned int m_iThreadSleep = 5; // How long the threads sleep / wait for a request to be handled
	bool m_bUpdate = false;
	bool m_bInUpdate = false;
	std::string m_strAddress = "";
	std::vector<HttpRequest*> m_pRequests;
	std::vector<int> m_pHandlerReferences; // Contains the Lua references to the handler functions.
	httplib::Server m_pServer;
	std::string m_strName = "NONAME";

	// userID - Response pairs.
	std::unordered_map<int, std::vector<PreparedHttpResponse*>> m_pPreparedResponses;
	CThreadFastMutex m_pPreparedResponsesMutex;
};

static int HttpResponse_TypeID = -1;
PushReferenced_LuaClass(HttpResponse, HttpResponse_TypeID)
Get_LuaClass(HttpResponse, HttpResponse_TypeID, "HttpResponse")

static int HttpRequest_TypeID = -1;
PushReferenced_LuaClass(HttpRequest, HttpRequest_TypeID)
Get_LuaClass(HttpRequest, HttpRequest_TypeID, "HttpRequest")

HttpRequest::~HttpRequest()
{
	Delete_HttpRequest(this);
	Delete_HttpResponse(&this->m_pResponseData);
}

LUA_FUNCTION_STATIC(HttpResponse__tostring)
{
	HttpResponse* pData = Get_HttpResponse(1, false);
	if (!pData)
		LUA->PushString("HttpResponse [NULL]");
	else
		LUA->PushString("HttpResponse");
	return 1;
}

Default__index(HttpResponse);
Default__newindex(HttpResponse);
Default__GetTable(HttpResponse);

LUA_FUNCTION_STATIC(HttpResponse_IsValid)
{
	LUA->PushBool(Get_HttpResponse(1, false) != NULL);
	return 1;
}

LUA_FUNCTION_STATIC(HttpResponse_SetContent)
{
	HttpResponse* pData = Get_HttpResponse(1, true);
	pData->m_bSetContent = true;
	pData->m_strContent = LUA->CheckString(2);
	pData->m_strContentType = LUA->CheckStringOpt(3, "text/plain");

	return 0;
}

LUA_FUNCTION_STATIC(HttpResponse_SetRedirect)
{
	HttpResponse* pData = Get_HttpResponse(1, true);
	pData->m_bSetRedirect = true;
	pData->m_strRedirect = LUA->CheckString(2);
	pData->m_iRedirectCode = (int)LUA->CheckNumberOpt(3, 302);

	return 0;
}

LUA_FUNCTION_STATIC(HttpResponse_SetHeader)
{
	HttpResponse* pData = Get_HttpResponse(1, true);
	pData->m_bSetHeader = true;
	pData->m_pHeaders[LUA->CheckString(2)] = LUA->CheckString(3);

	return 0;
}

LUA_FUNCTION_STATIC(HttpRequest__tostring)
{
	HttpRequest* pData = Get_HttpRequest(1, false);
	if (!pData)
		LUA->PushString("HttpRequest [NULL]");
	else
		LUA->PushString("HttpRequest");
	return 1;
}

Default__index(HttpRequest);
Default__newindex(HttpRequest);
Default__GetTable(HttpRequest);

LUA_FUNCTION_STATIC(HttpRequest_IsValid)
{
	LUA->PushBool(Get_HttpRequest(1, false) != NULL);
	return 1;
}

LUA_FUNCTION_STATIC(HttpRequest_HasHeader)
{
	HttpRequest* pData = Get_HttpRequest(1, false);

	LUA->PushBool(pData->m_pRequest.has_header(LUA->CheckString(2)));
	return 1;
}

LUA_FUNCTION_STATIC(HttpRequest_HasParam)
{
	HttpRequest* pData = Get_HttpRequest(1, false);

	LUA->PushBool(pData->m_pRequest.has_param(LUA->CheckString(2)));
	return 1;
}

LUA_FUNCTION_STATIC(HttpRequest_GetHeader)
{
	HttpRequest* pData = Get_HttpRequest(1, false);
	const char* header = LUA->CheckString(2);

	LUA->PushString(pData->m_pRequest.get_header_value(header).c_str());
	return 1;
}

LUA_FUNCTION_STATIC(HttpRequest_GetParam)
{
	HttpRequest* pData = Get_HttpRequest(1, false);
	const char* param = LUA->CheckString(2);

	LUA->PushString(pData->m_pRequest.get_param_value(param).c_str());
	return 1;
}

LUA_FUNCTION_STATIC(HttpRequest_GetBody)
{
	HttpRequest* pData = Get_HttpRequest(1, false);

	LUA->PushString(pData->m_pRequest.body.c_str());
	return 1;
}

LUA_FUNCTION_STATIC(HttpRequest_GetRemoteAddr)
{
	HttpRequest* pData = Get_HttpRequest(1, false);

	LUA->PushString(pData->m_pRequest.remote_addr.c_str());
	return 1;
}

LUA_FUNCTION_STATIC(HttpRequest_GetRemotePort)
{
	HttpRequest* pData = Get_HttpRequest(1, false);

	LUA->PushNumber(pData->m_pRequest.remote_port);
	return 1;
}

LUA_FUNCTION_STATIC(HttpRequest_GetLocalAddr)
{
	HttpRequest* pData = Get_HttpRequest(1, false);

	LUA->PushString(pData->m_pRequest.local_addr.c_str());
	return 1;
}

LUA_FUNCTION_STATIC(HttpRequest_GetLocalPort)
{
	HttpRequest* pData = Get_HttpRequest(1, false);

	LUA->PushNumber(pData->m_pRequest.local_port);
	return 1;
}

LUA_FUNCTION_STATIC(HttpRequest_GetMethod)
{
	HttpRequest* pData = Get_HttpRequest(1, false);

	LUA->PushString(pData->m_pRequest.method.c_str());
	return 1;
}

LUA_FUNCTION_STATIC(HttpRequest_GetAuthorizationCount)
{
	HttpRequest* pData = Get_HttpRequest(1, false);

	LUA->PushNumber(pData->m_pRequest.authorization_count_);
	return 1;
}

LUA_FUNCTION_STATIC(HttpRequest_GetContentLength)
{
	HttpRequest* pData = Get_HttpRequest(1, false);

	LUA->PushNumber(pData->m_pRequest.content_length_);
	return 1;
}

LUA_FUNCTION_STATIC(HttpRequest_GetClient)
{
	HttpRequest* pData = Get_HttpRequest(1, false);

	Push_CBaseClient(Util::GetClientByUserID(pData->m_pClientUserID));
	return 1;
}

LUA_FUNCTION_STATIC(HttpRequest_GetPlayer)
{
	HttpRequest* pData = Get_HttpRequest(1, false);
	CBaseClient* pClient = Util::GetClientByUserID(pData->m_pClientUserID);
	CBasePlayer* pPlayer = pClient ? Util::GetPlayerByClient(pClient) : NULL;

	if (pPlayer)
		Util::Push_Entity((CBaseEntity*)pPlayer);
	else
		LUA->PushNil();
	return 1;
}

void CallFunc(int func, HttpRequest* request, HttpResponse* response)
{
	Util::ReferencePush(g_Lua, func);

	if (g_pHttpServerModule.InDebug())
		Msg(PROJECT_NAME ": pushed handler function %i with type %i\n", func, g_Lua->GetType(-1));

	Push_HttpRequest(request);
	Push_HttpResponse(response);

	if (g_Lua->CallFunctionProtected(2, 1, true))
	{
		request->m_bHandled = !g_Lua->GetBool(-1);
		g_Lua->Pop(1);
	}

	Delete_HttpRequest(request);
	Delete_HttpResponse(response); // Destroys the Lua reference after we used it
}

void HttpServer::Start(const char* address, unsigned short port)
{
	if (m_iStatus != HTTPSERVER_OFFLINE)
		Stop();

	m_strAddress = address;
	m_iPort = port;
	CreateSimpleThread((ThreadFunc_t)HttpServer::Server, this);
	m_iStatus = HTTPSERVER_ONLINE;
}

void HttpServer::Stop()
{
	if (m_iStatus == HTTPSERVER_OFFLINE)
		return;

	m_pServer.stop();
	m_iStatus = HTTPSERVER_OFFLINE;
}

void HttpServer::Think()
{
	if (m_iStatus == HTTPSERVER_OFFLINE || !m_bUpdate)
		return;

	/*
	 * BUG: If for some reason a request remain unhandled, they might end up stuck until a second one comes in.
	 * The function below should probably just always run instead, but I fear for my precious performance.
	 * 
	 * Verify: Is this still a thing? Probably.
	 */

	m_bInUpdate = true;
	for (auto it = m_pRequests.begin(); it != m_pRequests.end();)
	{
		auto pEntry = *it;
		if (pEntry->m_bDelete)
		{
			it = m_pRequests.erase(it);
			delete pEntry;
			continue;
		}

		if (!pEntry->m_bHandled)
			CallFunc(pEntry->m_iFunction, pEntry, &pEntry->m_pResponseData);

		++it;
	}

	m_bUpdate = false;
	m_bInUpdate = false;
}

static std::string localAddr = "127.0.0.1";
static std::string loopBack = "loopback";
httplib::Server::Handler HttpServer::CreateHandler(const char* path, int func, bool ipWhitelist)
{
	m_pHandlerReferences.push_back(func);

	return [=](const httplib::Request& req, httplib::Response& res)
	{
		int userID = -1;
		for (auto& pClient : Util::GetClients())
		{
			if (!pClient->IsConnected())
				continue;

			INetChannel* pChannel = pClient->GetNetChannel();
			if (!pChannel)
				continue; // Probably a fake client.

			const netadr_s& addr = pChannel->GetRemoteAddress();
			std::string address = addr.ToString();
			size_t port_pos = address.find(":");
			if (address.substr(0, port_pos) == req.remote_addr || (req.remote_addr == localAddr && address.substr(0, port_pos) == loopBack))
			{
				userID = pClient->GetUserID();
				break;
			}
		}

		if (ipWhitelist && userID == -1)
		{
			if (g_pHttpServerModule.InDebug())
				Msg("holylib - httpserver: Request was denied as the ipWhitelist is enabled and the client couldn't be found.\n");

			return;
		}

		m_pPreparedResponsesMutex.Lock();
		auto it = m_pPreparedResponses.find(userID);
		if (it != m_pPreparedResponses.end())
		{
			for (auto vecIT = it->second.begin(); vecIT != it->second.end(); it++)
			{
				auto pPrepared = *vecIT;
				if (!pPrepared->ShouldRespond(req))
					continue;

				pPrepared->DoResponse(res);
				delete pPrepared;
				it->second.erase(vecIT);
				m_pPreparedResponsesMutex.Unlock();
				return;
			}
		}
		m_pPreparedResponsesMutex.Unlock();

		if (g_pHttpServerModule.InDebug())
			Msg("holylib - httpserver: Waiting for Main thread to pick up request\n");

		HttpRequest* request = new HttpRequest;
		request->m_strPath = path;
		request->m_pRequest = req;
		request->m_iFunction = func;
		request->m_pResponse = res;
		request->m_pClientUserID = userID;
		m_pRequests.push_back(request); // We should add a check here since we could write to it from multiple threads?
		m_bUpdate = true;
		while (!request->m_bHandled)
			ThreadSleep(m_iThreadSleep);

		request->m_pResponseData.DoResponse(res);

		request->m_bDelete = true;

		if (g_pHttpServerModule.InDebug())
			Msg("holylib - httpserver: Finished request\n");
	};
}

static int HttpServer_TypeID = -1;
PushReferenced_LuaClass(HttpServer, HttpServer_TypeID)
Get_LuaClass(HttpServer, HttpServer_TypeID, "HttpServer")

LUA_FUNCTION_STATIC(HttpServer_Think)
{
	HttpServer* pServer = Get_HttpServer(1, true);
	pServer->Think();

	return 0;
}

inline int CheckFunction(GarrysMod::Lua::ILuaInterface* LUA, int iStackPos)
{
	LUA->CheckType(iStackPos, GarrysMod::Lua::Type::Function);
	LUA->Push(iStackPos);
	return Util::ReferenceCreate("CheckFunction - HttpServer blackbox");
}

inline bool CheckBool(GarrysMod::Lua::ILuaInterface* LUA, int iStackPos)
{
	LUA->CheckType(iStackPos, GarrysMod::Lua::Type::Bool);
	return LUA->GetBool(iStackPos);
}

LUA_FUNCTION_STATIC(HttpServer__tostring)
{
	HttpServer* pServer = Get_HttpServer(1, false);
	if (!pServer)
	{
		LUA->PushString("HttpServer [NULL]");
		return 1;
	}

	char szBuf[64] = {};
	V_snprintf(szBuf, sizeof(szBuf),"HttpServer [%s:%s - %s]", pServer->GetAddress().c_str(), std::to_string(pServer->GetPort()).c_str(), pServer->GetName().c_str()); 
	LUA->PushString(szBuf);
	return 1;
}

Default__index(HttpServer);
Default__newindex(HttpServer);
Default__GetTable(HttpServer);

LUA_FUNCTION_STATIC(HttpServer_IsValid)
{
	LUA->PushBool(Get_HttpServer(1, false) != NULL);
	return 1;
}

LUA_FUNCTION_STATIC(HttpServer_Get)
{
	HttpServer* pServer = Get_HttpServer(1, true);
	const char* path = LUA->CheckString(2);
	int func = CheckFunction(LUA, 3);
	bool ipWhitelist = LUA->GetBool(4);

	pServer->Get(path, func, ipWhitelist);

	return 0;
}

LUA_FUNCTION_STATIC(HttpServer_Post)
{
	HttpServer* pServer = Get_HttpServer(1, true);
	const char* path = LUA->CheckString(2);
	int func = CheckFunction(LUA, 3);
	bool ipWhitelist = LUA->GetBool(4);

	pServer->Post(path, func, ipWhitelist);

	return 0;
}

LUA_FUNCTION_STATIC(HttpServer_Put)
{
	HttpServer* pServer = Get_HttpServer(1, true);
	const char* path = LUA->CheckString(2);
	int func = CheckFunction(LUA, 3);
	bool ipWhitelist = LUA->GetBool(4);

	pServer->Put(path, func, ipWhitelist);

	return 0;
}

LUA_FUNCTION_STATIC(HttpServer_Patch)
{
	HttpServer* pServer = Get_HttpServer(1, true);
	const char* path = LUA->CheckString(2);
	int func = CheckFunction(LUA, 3);
	bool ipWhitelist = LUA->GetBool(4);

	pServer->Patch(path, func, ipWhitelist);

	return 0;
}

LUA_FUNCTION_STATIC(HttpServer_Delete)
{
	HttpServer* pServer = Get_HttpServer(1, true);
	const char* path = LUA->CheckString(2);
	int func = CheckFunction(LUA, 3);
	bool ipWhitelist = LUA->GetBool(4);

	pServer->Delete(path, func, ipWhitelist);

	return 0;
}

LUA_FUNCTION_STATIC(HttpServer_Options)
{
	HttpServer* pServer = Get_HttpServer(1, true);
	const char* path = LUA->CheckString(2);
	int func = CheckFunction(LUA, 3);
	bool ipWhitelist = LUA->GetBool(4);

	pServer->Options(path, func, ipWhitelist);

	return 0;
}

LUA_FUNCTION_STATIC(HttpServer_IsRunning)
{
	HttpServer* pServer = Get_HttpServer(1, true);
	LUA->PushBool(pServer->GetStatus() == HTTPSERVER_ONLINE);

	return 1;
}

LUA_FUNCTION_STATIC(HttpServer_SetTCPnodelay)
{
	HttpServer* pServer = Get_HttpServer(1, true);
	pServer->GetServer().set_tcp_nodelay(CheckBool(LUA, 2));

	return 0;
}

LUA_FUNCTION_STATIC(HttpServer_SetReadTimeout)
{
	HttpServer* pServer = Get_HttpServer(1, true);
	pServer->GetServer().set_read_timeout((time_t)LUA->CheckNumber(2), (time_t)LUA->CheckNumber(3));

	return 0;
}

LUA_FUNCTION_STATIC(HttpServer_SetWriteTimeout)
{
	HttpServer* pServer = Get_HttpServer(1, true);
	pServer->GetServer().set_write_timeout((time_t)LUA->CheckNumber(1), (time_t)LUA->CheckNumber(2));

	return 0;
}

LUA_FUNCTION_STATIC(HttpServer_SetPayloadMaxLength)
{
	HttpServer* pServer = Get_HttpServer(1, true);
	pServer->GetServer().set_payload_max_length((size_t)LUA->CheckNumber(2));

	return 0;
}

LUA_FUNCTION_STATIC(HttpServer_SetKeepAliveTimeout)
{
	HttpServer* pServer = Get_HttpServer(1, true);
	pServer->GetServer().set_keep_alive_timeout((time_t)LUA->CheckNumber(2));

	return 0;
}

LUA_FUNCTION_STATIC(HttpServer_SetKeepAliveMaxCount)
{
	HttpServer* pServer = Get_HttpServer(1, true);
	pServer->GetServer().set_keep_alive_max_count((size_t)LUA->CheckNumber(2));

	return 0;
}

LUA_FUNCTION_STATIC(HttpServer_SetMountPoint)
{
	HttpServer* pServer = Get_HttpServer(1, true);
	pServer->GetServer().set_mount_point(LUA->CheckString(2), LUA->CheckString(3));

	return 0;
}

LUA_FUNCTION_STATIC(HttpServer_RemoveMountPoint)
{
	HttpServer* pServer = Get_HttpServer(1, true);
	pServer->GetServer().remove_mount_point(LUA->CheckString(2));

	return 0;
}

LUA_FUNCTION_STATIC(HttpServer_Start)
{
	HttpServer* pServer = Get_HttpServer(1, true);
	const char* address = LUA->CheckString(2);
	unsigned short port = (unsigned short)LUA->CheckNumber(3);

	pServer->Start(address, port);

	return 0;
}

LUA_FUNCTION_STATIC(HttpServer_Stop)
{
	HttpServer* pServer = Get_HttpServer(1, true);
	pServer->Stop();

	return 0;
}

LUA_FUNCTION_STATIC(HttpServer_SetThreadSleep)
{
	HttpServer* pServer = Get_HttpServer(1, true);
	pServer->SetThreadSleep((unsigned int)LUA->CheckNumber(2));

	return 0;
}

LUA_FUNCTION_STATIC(HttpServer_GetPort)
{
	HttpServer* pServer = Get_HttpServer(1, true);

	LUA->PushNumber(pServer->GetPort());
	return 1;
}

LUA_FUNCTION_STATIC(HttpServer_GetAddress)
{
	HttpServer* pServer = Get_HttpServer(1, true);

	LUA->PushString(pServer->GetAddress().c_str());
	return 1;
}

LUA_FUNCTION_STATIC(HttpServer_GetName)
{
	HttpServer* pServer = Get_HttpServer(1, true);

	LUA->PushString(pServer->GetName().c_str());
	return 1;
}

LUA_FUNCTION_STATIC(HttpServer_SetName)
{
	HttpServer* pServer = Get_HttpServer(1, true);

	pServer->SetName(LUA->CheckString(2));
	return 0;
}

LUA_FUNCTION_STATIC(HttpServer_AddPreparedResponse)
{
	HttpServer* pServer = Get_HttpServer(1, true);
	int userID = (int)LUA->CheckNumber(2);
	const char* pPath = LUA->CheckString(3);
	const char* pMethod = LUA->CheckString(4);
	LUA->CheckType(5, GarrysMod::Lua::Type::Table);
	LUA->CheckType(6, GarrysMod::Lua::Type::Function);

	PreparedHttpResponse* pResponse = new PreparedHttpResponse;
	pResponse->m_strPath = pPath;
	pResponse->m_strMethod = pMethod;

	LUA->Push(5);
	LUA->PushNil();
	while (LUA->Next(-2)) {
		LUA->Push(-2);

		const char* key = LUA->GetString(-1);
		const char* value = LUA->GetString(-2);
		pResponse->m_pRequiredHeaders[key] = value;

		LUA->Pop(2);
	}
	LUA->Pop(1);

	Push_HttpResponse(&pResponse->m_pResponse);
	LUA->Push(6);
	LUA->Push(-2);
	LUA->CallFunctionProtected(1, 0, true);
	Delete_HttpResponse(&pResponse->m_pResponse);
	pServer->AddPreparedResponse(userID, pResponse);

	return 0;
}

LUA_FUNCTION_STATIC(httpserver_Create)
{
	Push_HttpServer(new HttpServer);
	return 1;
}

LUA_FUNCTION_STATIC(httpserver_Destroy)
{
	HttpServer* pServer = Get_HttpServer(1, true);

	if (pServer->GetStatus() == HTTPSERVER_ONLINE)
		pServer->Stop();

	Delete_HttpServer(pServer);
	delete pServer;

	return 0;
}

LUA_FUNCTION_STATIC(httpserver_GetAll)
{
	LUA->PreCreateTable(g_pHttpServers.size(), 0);
		int idx = 0;
		for (auto& server : g_pHttpServers)
		{
			Push_HttpServer(server);
			Util::RawSetI(LUA, -2, ++idx);
		}

	return 1;
}

LUA_FUNCTION_STATIC(httpserver_FindByName)
{
	std::string strName = LUA->CheckString(1);
	bool bPushed = false;
	for (auto& server : g_pHttpServers)
	{
		if (server->GetName() == strName)
		{
			Push_HttpServer(server);
			bPushed = true;
		}
	}

	if (!bPushed)
		LUA->PushNil();

	return 1;
}

void HttpServer_OnClientDisconnect(CBaseClient* pClient)
{
	// Verify: As stated above in the HttpServer class, should we add a mutex for this?
	int userID = pClient->GetUserID();
	for (auto& server : g_pHttpServers)
	{
		server->ClearDisconnectedClient(userID);
	}
}

void CHTTPServerModule::LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit)
{
	if (bServerInit)
		return;

	HttpServer_TypeID = g_Lua->CreateMetaTable("HttpServer");
		Util::AddFunc(HttpServer__tostring, "__tostring");
		Util::AddFunc(HttpServer__index, "__index");
		Util::AddFunc(HttpServer__newindex, "__newindex");
		Util::AddFunc(HttpServer_GetTable, "GetTable");
		Util::AddFunc(HttpServer_IsValid, "IsValid");

		Util::AddFunc(HttpServer_Think, "Think");
		Util::AddFunc(HttpServer_Start, "Start");
		Util::AddFunc(HttpServer_Stop, "Stop");

		Util::AddFunc(HttpServer_IsRunning, "IsRunning");
		Util::AddFunc(HttpServer_SetTCPnodelay, "SetTCPnodelay");
		Util::AddFunc(HttpServer_SetReadTimeout, "SetReadTimeout");
		Util::AddFunc(HttpServer_SetWriteTimeout, "SetWriteTimeout");
		Util::AddFunc(HttpServer_SetPayloadMaxLength, "SetPayloadMaxLength");
		Util::AddFunc(HttpServer_SetKeepAliveTimeout, "SetKeepAliveTimeout");
		Util::AddFunc(HttpServer_SetKeepAliveMaxCount, "SetKeepAliveMaxCount");
		Util::AddFunc(HttpServer_SetThreadSleep, "SetThreadSleep");

		Util::AddFunc(HttpServer_SetMountPoint, "SetMountPoint");
		Util::AddFunc(HttpServer_RemoveMountPoint, "RemoveMountPoint");

		Util::AddFunc(HttpServer_Get, "Get");
		Util::AddFunc(HttpServer_Post, "Post");
		Util::AddFunc(HttpServer_Put, "Put");
		Util::AddFunc(HttpServer_Patch, "Patch");
		Util::AddFunc(HttpServer_Delete, "Delete");
		Util::AddFunc(HttpServer_Options, "Options");

		Util::AddFunc(HttpServer_GetPort, "GetPort");
		Util::AddFunc(HttpServer_GetAddress, "GetAddress");
		Util::AddFunc(HttpServer_GetName, "GetName");
		Util::AddFunc(HttpServer_SetName, "SetName");

		Util::AddFunc(HttpServer_AddPreparedResponse, "AddPreparedResponse");
	g_Lua->Pop(1);

	HttpResponse_TypeID = g_Lua->CreateMetaTable("HttpResponse");
		Util::AddFunc(HttpResponse__tostring, "__tostring");
		Util::AddFunc(HttpResponse__index, "__index");
		Util::AddFunc(HttpResponse__newindex, "__newindex");
		Util::AddFunc(HttpResponse_GetTable, "GetTable");
		Util::AddFunc(HttpResponse_IsValid, "IsValid");

		Util::AddFunc(HttpResponse_SetContent, "SetContent");
		Util::AddFunc(HttpResponse_SetHeader, "SetHeader");
		Util::AddFunc(HttpResponse_SetRedirect, "SetRedirect");
	g_Lua->Pop(1);

	HttpRequest_TypeID = g_Lua->CreateMetaTable("HttpRequest");
		Util::AddFunc(HttpRequest__tostring, "__tostring");
		Util::AddFunc(HttpRequest__index, "__index");
		Util::AddFunc(HttpRequest__newindex, "__newindex");
		Util::AddFunc(HttpRequest_GetTable, "GetTable");
		Util::AddFunc(HttpRequest_IsValid, "IsValid");

		Util::AddFunc(HttpRequest_HasHeader, "HasHeader");
		Util::AddFunc(HttpRequest_HasParam, "HasParam");
		Util::AddFunc(HttpRequest_GetHeader, "GetHeader");
		Util::AddFunc(HttpRequest_GetParam, "GetParam");
		Util::AddFunc(HttpRequest_GetBody, "GetBody");
		Util::AddFunc(HttpRequest_GetRemoteAddr, "GetRemoteAddr");
		Util::AddFunc(HttpRequest_GetRemotePort, "GetRemotePort");
		Util::AddFunc(HttpRequest_GetLocalAddr, "GetLocalAddr");
		Util::AddFunc(HttpRequest_GetLocalPort, "GetLocalPort");
		Util::AddFunc(HttpRequest_GetMethod, "GetMethod");
		Util::AddFunc(HttpRequest_GetAuthorizationCount, "GetAuthorizationCount");
		Util::AddFunc(HttpRequest_GetContentLength, "GetContentLength");

		Util::AddFunc(HttpRequest_GetClient, "GetClient");
		Util::AddFunc(HttpRequest_GetPlayer, "GetPlayer");
	g_Lua->Pop(1);

	Util::StartTable();
		Util::AddFunc(httpserver_Create, "Create");
		Util::AddFunc(httpserver_Destroy, "Destroy");
		Util::AddFunc(httpserver_GetAll, "GetAll");
		Util::AddFunc(httpserver_FindByName, "FindByName");
	Util::FinishTable("httpserver");
}

void CHTTPServerModule::LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua)
{
	Util::NukeTable("httpserver");

	// HttpServers WILL persist across map changes.
	DeleteAll_HttpResponse();
	DeleteAll_HttpRequest();
	DeleteAll_HttpServer();

	std::vector<HttpServer*> httpServers; // Copy of g_pHttpServers as when deleting we can't iterate over it.
	for (auto server : g_pHttpServers)
		httpServers.push_back(server);

	for (auto server : httpServers)
		delete server;
}

void CHTTPServerModule::Think(bool simulating)
{
	VPROF_BUDGET("HolyLib - CHTTPServerModule::Think", VPROF_BUDGETGROUP_HOLYLIB);

	for (auto& [httpserver, _] : g_pPushedHttpServer)
		httpserver->Think();
}