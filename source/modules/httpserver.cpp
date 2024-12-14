#include "httplib.h"
#include "LuaInterface.h"
#include "module.h"
#include "lua.h"
#include <baseclient.h>
#include <inetchannel.h>
#include <netadr.h>

class CHTTPServerModule : public IModule
{
public:
	virtual void LuaInit(bool bServerInit) OVERRIDE;
	virtual void LuaShutdown() OVERRIDE;
	virtual void Think(bool bSimulating) OVERRIDE;
	virtual const char* Name() { return "httpserver"; };
	virtual int Compatibility() { return LINUX32 | LINUX64 | WINDOWS32 | WINDOWS64; };
	virtual bool IsEnabledByDefault() { return false; };
};

CHTTPServerModule g_pHttpServerModule;
IModule* pHttpServerModule = &g_pHttpServerModule;

struct HttpResponse {
	bool bSetContent = false;
	bool bSetRedirect = false;
	bool bSetHeader = false;
	int iRedirectCode = 302;
	std::string strContent = "";
	std::string strContentType = "text/plain";
	std::string strRedirect = "";
	std::unordered_map<std::string, std::string> pHeaders;
};

struct HttpRequest {
	~HttpRequest();

	bool bHandled = false;
	bool bDelete = false; // We only delete from the main thread.
	int iFunction;
	std::string strPath;
	HttpResponse pResponseData;
	httplib::Response pResponse;
	httplib::Request pRequest;
};

enum
{
	HTTPSERVER_ONLINE,
	HTTPSERVER_OFFLINE
};

class HttpServer
{
public:
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

	void Get(const char* path, int func, bool ipwhitelist)
	{
		m_pServer.Get(path, CreateHandler(path, func, ipwhitelist));
	}

	void Post(const char* path, int func, bool ipwhitelist)
	{
		m_pServer.Post(path, CreateHandler(path, func, ipwhitelist));
	}

	void Put(const char* path, int func, bool ipwhitelist)
	{
		m_pServer.Put(path, CreateHandler(path, func, ipwhitelist));
	}

	void Patch(const char* path, int func, bool ipwhitelist)
	{
		m_pServer.Patch(path, CreateHandler(path, func, ipwhitelist));
	}

	void Delete(const char* path, int func, bool ipwhitelist)
	{
		m_pServer.Delete(path, CreateHandler(path, func, ipwhitelist));
	}

	void Options(const char* path, int func, bool ipwhitelist)
	{
		m_pServer.Options(path, CreateHandler(path, func, ipwhitelist));
	}

	httplib::Server::Handler CreateHandler(const char*, int, bool);

public:
	httplib::Server& GetServer() { return m_pServer; };
	unsigned char GetStatus() { return m_iStatus; };
	std::string& GetAddress() { return m_strAddress; };
	unsigned short GetPort() { return m_iPort; };

private:
	unsigned char m_iStatus = HTTPSERVER_OFFLINE;
	unsigned short m_iPort;
	bool m_bUpdate = false;
	bool m_bInUpdate = false;
	std::string m_strAddress;
	std::vector<HttpRequest*> m_pRequests;
	httplib::Server m_pServer;
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
	Delete_HttpResponse(&this->pResponseData);
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

LUA_FUNCTION_STATIC(HttpResponse__index)
{
	if (LUA->FindOnObjectsMetaTable(1, 2))
		return 1;

	LUA->Pop(1);
	LUA->ReferencePush(g_pPushedHttpResponse[Get_HttpResponse(1, true)]->iTableReference);
	if (!LUA->FindObjectOnTable(-1, 2))
		LUA->PushNil();

	LUA->Remove(-2);

	return 1;
}

LUA_FUNCTION_STATIC(HttpResponse__newindex)
{
	LUA->ReferencePush(g_pPushedHttpResponse[Get_HttpResponse(1, true)]->iTableReference);
	LUA->Push(2);
	LUA->Push(3);
	LUA->RawSet(-3);
	LUA->Pop(1);

	return 0;
}

LUA_FUNCTION_STATIC(HttpResponse_GetTable)
{
	LUA->ReferencePush(g_pPushedHttpResponse[Get_HttpResponse(1, true)]->iTableReference); // This should never crash so no safety checks.
	return 1;
}

LUA_FUNCTION_STATIC(HttpResponse_SetContent)
{
	HttpResponse* pData = Get_HttpResponse(1, true);
	pData->bSetContent = true;
	pData->strContent = LUA->CheckString(2);
	pData->strContentType = LUA->CheckString(3);

	return 0;
}

LUA_FUNCTION_STATIC(HttpResponse_SetRedirect)
{
	HttpResponse* pData = Get_HttpResponse(1, true);
	pData->bSetRedirect = true;
	pData->strRedirect = LUA->CheckString(2);
	pData->iRedirectCode = (int)LUA->CheckNumber(3);

	return 0;
}

LUA_FUNCTION_STATIC(HttpResponse_SetHeader)
{
	HttpResponse* pData = Get_HttpResponse(1, true);
	pData->bSetHeader = true;
	pData->pHeaders[LUA->CheckString(2)] = LUA->CheckString(3);

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

LUA_FUNCTION_STATIC(HttpRequest__index)
{
	if (LUA->FindOnObjectsMetaTable(1, 2))
		return 1;

	LUA->Pop(1);
	LUA->ReferencePush(g_pPushedHttpRequest[Get_HttpRequest(1, true)]->iTableReference);
	if (!LUA->FindObjectOnTable(-1, 2))
		LUA->PushNil();

	LUA->Remove(-2);

	return 1;
}

LUA_FUNCTION_STATIC(HttpRequest__newindex)
{
	LUA->ReferencePush(g_pPushedHttpRequest[Get_HttpRequest(1, true)]->iTableReference);
	LUA->Push(2);
	LUA->Push(3);
	LUA->RawSet(-3);
	LUA->Pop(1);

	return 0;
}

LUA_FUNCTION_STATIC(HttpRequest_GetTable)
{
	LUA->ReferencePush(g_pPushedHttpRequest[Get_HttpRequest(1, true)]->iTableReference); // This should never crash so no safety checks.
	return 1;
}

LUA_FUNCTION_STATIC(HttpRequest_HasHeader)
{
	HttpRequest* pData = Get_HttpRequest(1, false);

	LUA->PushBool(pData->pRequest.has_header(LUA->CheckString(2)));
	return 1;
}

LUA_FUNCTION_STATIC(HttpRequest_HasParam)
{
	HttpRequest* pData = Get_HttpRequest(1, false);

	LUA->PushBool(pData->pRequest.has_param(LUA->CheckString(2)));
	return 1;
}

LUA_FUNCTION_STATIC(HttpRequest_GetHeader)
{
	HttpRequest* pData = Get_HttpRequest(1, false);
	const char* header = LUA->CheckString(2);

	LUA->PushString(pData->pRequest.get_header_value(header).c_str());
	return 1;
}

LUA_FUNCTION_STATIC(HttpRequest_GetParam)
{
	HttpRequest* pData = Get_HttpRequest(1, false);
	const char* param = LUA->CheckString(2);

	LUA->PushString(pData->pRequest.get_param_value(param).c_str());
	return 1;
}

LUA_FUNCTION_STATIC(HttpRequest_GetBody)
{
	HttpRequest* pData = Get_HttpRequest(1, false);

	LUA->PushString(pData->pRequest.body.c_str());
	return 1;
}

LUA_FUNCTION_STATIC(HttpRequest_GetRemoteAddr)
{
	HttpRequest* pData = Get_HttpRequest(1, false);

	LUA->PushString(pData->pRequest.remote_addr.c_str());
	return 1;
}

LUA_FUNCTION_STATIC(HttpRequest_GetRemotePort)
{
	HttpRequest* pData = Get_HttpRequest(1, false);

	LUA->PushNumber(pData->pRequest.remote_port);
	return 1;
}

LUA_FUNCTION_STATIC(HttpRequest_GetLocalAddr)
{
	HttpRequest* pData = Get_HttpRequest(1, false);

	LUA->PushString(pData->pRequest.local_addr.c_str());
	return 1;
}

LUA_FUNCTION_STATIC(HttpRequest_GetLocalPort)
{
	HttpRequest* pData = Get_HttpRequest(1, false);

	LUA->PushNumber(pData->pRequest.local_port);
	return 1;
}

LUA_FUNCTION_STATIC(HttpRequest_GetMethod)
{
	HttpRequest* pData = Get_HttpRequest(1, false);

	LUA->PushString(pData->pRequest.method.c_str());
	return 1;
}

LUA_FUNCTION_STATIC(HttpRequest_GetAuthorizationCount)
{
	HttpRequest* pData = Get_HttpRequest(1, false);

	LUA->PushNumber(pData->pRequest.authorization_count_);
	return 1;
}

LUA_FUNCTION_STATIC(HttpRequest_GetContentLength)
{
	HttpRequest* pData = Get_HttpRequest(1, false);

	LUA->PushNumber(pData->pRequest.content_length_);
	return 1;
}

void CallFunc(int func, HttpRequest* request, HttpResponse* response)
{
	g_Lua->ReferencePush(func);

	Push_HttpRequest(request);
	Push_HttpResponse(response);

	g_Lua->PCall(2, 0, 0);

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

	m_bInUpdate = true;
	for (auto it = m_pRequests.begin(); it != m_pRequests.end(); ++it) {
		auto pEntry = (*it);
		if (pEntry->bHandled) { continue; }
		if (pEntry->bDelete) {
			it = m_pRequests.erase(it);
			delete pEntry;
			continue;
		}

		CallFunc(pEntry->iFunction, pEntry, &pEntry->pResponseData);
		pEntry->bHandled = true;
	}

	m_bUpdate = false;
	m_bInUpdate = false;
}

httplib::Server::Handler HttpServer::CreateHandler(const char* path, int func, bool ipwhitelist)
{
	return [=](const httplib::Request& req, httplib::Response& res) {
		if (ipwhitelist) {
			bool found = false;
			for (auto& pClient : Util::GetClients())
			{
				if (!pClient->IsActive())
					continue;

				const netadr_s& addr = pClient->GetNetChannel()->GetRemoteAddress();
				std::string address = addr.ToString();
				size_t port_pos = address.find(":");
				if (address.substr(0, port_pos) == req.remote_addr || (req.remote_addr == "127.0.0.1" && address.substr(0, port_pos) == "loopback")) {
					found = true;
					break;
				}
			}

			if (!found) { return; }
		}

		HttpRequest* request = new HttpRequest;
		request->strPath = path;
		request->pRequest = req;
		request->iFunction = func;
		request->pResponse = res;
		m_pRequests.push_back(request); // We should add a check here since we could write to it from multiple threads?
		m_bUpdate = true;
		while (!request->bHandled) {
			ThreadSleep(1);
		}
		HttpResponse* rdata = &request->pResponseData;
		if (rdata->bSetContent) {
			res.set_content(rdata->strContent, rdata->strContentType);
		}

		if (rdata->bSetRedirect) {
			res.set_redirect(rdata->strRedirect, rdata->iRedirectCode);
		}

		if (rdata->bSetHeader) {
			for (auto& [key, value] : rdata->pHeaders) {
				res.set_header(key, value);
			}
		}

		request->bDelete = true;
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
	return LUA->ReferenceCreate();
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
	V_snprintf(szBuf, sizeof(szBuf),"HttpServer [%s]", (pServer->GetAddress() + std::to_string(pServer->GetPort())).c_str()); 
	LUA->PushString(szBuf);
	return 1;
}

LUA_FUNCTION_STATIC(HttpServer__index)
{
	if (LUA->FindOnObjectsMetaTable(1, 2))
		return 1;

	LUA->Pop(1);
	LUA->ReferencePush(g_pPushedHttpServer[Get_HttpServer(1, true)]->iTableReference);
	if (!LUA->FindObjectOnTable(-1, 2))
		LUA->PushNil();

	LUA->Remove(-2);

	return 1;
}

LUA_FUNCTION_STATIC(HttpServer__newindex)
{
	LUA->ReferencePush(g_pPushedHttpServer[Get_HttpServer(1, true)]->iTableReference);
	LUA->Push(2);
	LUA->Push(3);
	LUA->RawSet(-3);
	LUA->Pop(1);

	return 0;
}

LUA_FUNCTION_STATIC(HttpServer_GetTable)
{
	LUA->ReferencePush(g_pPushedHttpServer[Get_HttpServer(1, true)]->iTableReference);

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

void CHTTPServerModule::LuaInit(bool bServerInit)
{
	if (bServerInit)
		return;

	HttpServer_TypeID = g_Lua->CreateMetaTable("HttpServer");
		Util::AddFunc(HttpServer__tostring, "__tostring");
		Util::AddFunc(HttpServer__index, "__index");
		Util::AddFunc(HttpServer__newindex, "__newindex");
		Util::AddFunc(HttpServer_GetTable, "GetTable");

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

		Util::AddFunc(HttpServer_SetMountPoint, "SetMountPoint");
		Util::AddFunc(HttpServer_RemoveMountPoint, "RemoveMountPoint");

		Util::AddFunc(HttpServer_Get, "Get");
		Util::AddFunc(HttpServer_Post, "Post");
		Util::AddFunc(HttpServer_Put, "Put");
		Util::AddFunc(HttpServer_Patch, "Patch");
		Util::AddFunc(HttpServer_Delete, "Delete");
		Util::AddFunc(HttpServer_Options, "Options");
	g_Lua->Pop(1);

	HttpResponse_TypeID = g_Lua->CreateMetaTable("HttpResponse");
		Util::AddFunc(HttpResponse__tostring, "__tostring");
		Util::AddFunc(HttpResponse__index, "__index");
		Util::AddFunc(HttpResponse__newindex, "__newindex");
		Util::AddFunc(HttpResponse_GetTable, "GetTable");

		Util::AddFunc(HttpResponse_SetContent, "SetContent");
		Util::AddFunc(HttpResponse_SetHeader, "SetHeader");
		Util::AddFunc(HttpResponse_SetRedirect, "SetRedirect");
	g_Lua->Pop(1);

	HttpRequest_TypeID = g_Lua->CreateMetaTable("HttpRequest");
		Util::AddFunc(HttpRequest__tostring, "__tostring");
		Util::AddFunc(HttpRequest__index, "__index");
		Util::AddFunc(HttpRequest__newindex, "__newindex");
		Util::AddFunc(HttpRequest_GetTable, "GetTable");

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
	g_Lua->Pop(1);

	Util::StartTable();
		Util::AddFunc(httpserver_Create, "Create");
		Util::AddFunc(httpserver_Destroy, "Destroy");
	Util::FinishTable("httpserver");
}

void CHTTPServerModule::LuaShutdown()
{
}

void CHTTPServerModule::Think(bool simulating)
{
}