#include "LuaInterface.h"
#include "detours.h"
#include "module.h"
#include "lua.h"
#include "util.h"
#include <networkstringtabledefs.h>
#include <vprof.h>
#include "embree4/rtcore.h"
#include "vphysics_interface.h"
#include "eiface.h"
#include "player.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CEmbreeModule : public IModule
{
public:
#if SYSTEM_LINUX && ARCHITECTURE_X86_64 || HOLYLIB_DEVELOPMENT
	virtual void LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit) OVERRIDE;
	virtual void LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua) OVERRIDE;
	virtual void Init(CreateInterfaceFn* appfn, CreateInterfaceFn* gamefn) OVERRIDE;
#endif
	virtual const char* Name() { return "embree"; };
	virtual int Compatibility() { return LINUX32 | WINDOWS32; };
};

static CEmbreeModule g_pEmbreeModule;
IModule* pEmbreeModule = &g_pEmbreeModule;

#if SYSTEM_LINUX && ARCHITECTURE_X86_64 || HOLYLIB_DEVELOPMENT
static RTCDevice pDevice = NULL;
static RTCGeometry pGeometry = NULL;
static RTCScene pScene = NULL;
static void ReleaseEverything()
{
	if (pGeometry)
	{
		rtcReleaseGeometry(pGeometry);
		pGeometry = NULL;
	}

	if (pScene)
	{
		rtcReleaseScene(pScene);
		pScene = NULL;
	}

	if (pDevice)
	{
		rtcReleaseDevice(pDevice);
		pDevice = NULL;
	}
}

static IPhysicsCollision* pPhysCollide;
LUA_FUNCTION_STATIC(embree_SetupDevice)
{
	ReleaseEverything();

	pDevice = rtcNewDevice("robust=1,threads=4");

	pGeometry = rtcNewGeometry(pDevice, RTC_GEOMETRY_TYPE_TRIANGLE);
	rtcSetGeometryVertexAttributeCount(pGeometry, 0);

	pScene = rtcNewScene(pDevice);

	CBaseEntity* pEntity = Util::GetCBaseEntityFromEdict(Util::engineserver->PEntityOfEntIndex(0));
	if (!pEntity)
		LUA->ThrowError("Missing world!");

	IPhysicsObject* pObject = pEntity->VPhysicsGetObject();
	if (!pObject)
		LUA->ThrowError("Missing world physics!");

	ICollisionQuery* pQueue = pPhysCollide->CreateQueryModel(const_cast<CPhysCollide*>(pObject->GetCollide()));
	if (!pQueue)
		LUA->ThrowError("Failed to create collision queue!");

	std::vector<float> vertexData;
	std::vector<unsigned int> indexData;
	int convexCount = pQueue->ConvexCount();
	for (int convexIndex = 0; convexIndex < convexCount; ++convexIndex)
	{
		int triangleCount = pQueue->TriangleCount(convexIndex);
		for (int triangleIndex = 0; triangleIndex < triangleCount; ++triangleIndex)
		{
			Vector verts[3];
			pQueue->GetTriangleVerts(convexIndex, triangleIndex, verts);

			vertexData.push_back(verts[0].x);
			vertexData.push_back(verts[0].y);
			vertexData.push_back(verts[0].z);

			vertexData.push_back(verts[1].x);
			vertexData.push_back(verts[1].y);
			vertexData.push_back(verts[1].z);

			vertexData.push_back(verts[2].x);
			vertexData.push_back(verts[2].y);
			vertexData.push_back(verts[2].z);

			unsigned int baseIndex = (unsigned int)(vertexData.size() / 3) - 3;
			indexData.push_back(baseIndex);
			indexData.push_back(baseIndex + 1);
			indexData.push_back(baseIndex + 2);
		}
	}

	pPhysCollide->DestroyQueryModel(pQueue);

	// Bet this will crash.
	rtcSetSharedGeometryBuffer(pGeometry, RTC_BUFFER_TYPE_VERTEX, 0, RTC_FORMAT_FLOAT3, vertexData.data(), 0, sizeof(float) * 3, vertexData.size() / 3);
	rtcSetSharedGeometryBuffer(pGeometry, RTC_BUFFER_TYPE_INDEX, 0, RTC_FORMAT_UINT3, indexData.data(), 0, sizeof(unsigned int) * 3, indexData.size() / 3);

	rtcCommitGeometry(pGeometry);
	rtcAttachGeometry(pScene, pGeometry);
	rtcReleaseGeometry(pGeometry);
	pGeometry = NULL;

	rtcCommitScene(pScene);

	return 0;
}

LUA_FUNCTION_STATIC(embree_TraceLine)
{
	Vector* pOrigin = Get_Vector(LUA, 1, true);
	Vector* pDirection = Get_Vector(LUA, 2, true);
	float fNear = (float)LUA->CheckNumberOpt(3, 0.0);
	float fFar = (float)LUA->CheckNumberOpt(3, 999999);

	RTCRayHit rayhit;
	rayhit.ray.org_x = pOrigin->x;
	rayhit.ray.org_y = pOrigin->y;
	rayhit.ray.org_z = pOrigin->z;

	rayhit.ray.dir_x = pDirection->x;
	rayhit.ray.dir_y = pDirection->y;
	rayhit.ray.dir_z = pDirection->z;

	rayhit.ray.tnear = fNear;
	rayhit.ray.tfar = fFar;

	rayhit.hit.geomID = RTC_INVALID_GEOMETRY_ID;

	rtcIntersect1(pScene, &rayhit);

	if (rayhit.hit.geomID != RTC_INVALID_GEOMETRY_ID)
	{
		float hitX = rayhit.ray.org_x + rayhit.ray.tfar * rayhit.ray.dir_x;
		float hitY = rayhit.ray.org_y + rayhit.ray.tfar * rayhit.ray.dir_y;
		float hitZ = rayhit.ray.org_z + rayhit.ray.tfar * rayhit.ray.dir_z;

		Vector hitPos(hitX, hitY, hitZ);
		Push_Vector(LUA, &hitPos);
	} else {
		LUA->PushNil();
	}

	return 1;
}

void CEmbreeModule::Init(CreateInterfaceFn* appfn, CreateInterfaceFn* gamefn)
{
	if (appfn[0])
	{
		pPhysCollide = (IPhysicsCollision*)appfn[0](VPHYSICS_COLLISION_INTERFACE_VERSION, NULL);
	} else {
		SourceSDK::FactoryLoader vphysics_loader("vphysics");
		pPhysCollide = vphysics_loader.GetInterface<IPhysicsCollision>(VPHYSICS_COLLISION_INTERFACE_VERSION);
	}
}

void CEmbreeModule::LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit)
{
	if (bServerInit)
		return;

	Util::StartTable(pLua);
		Util::AddFunc(pLua, embree_SetupDevice, "SetupDevice");
		Util::AddFunc(pLua, embree_TraceLine, "TraceLine");
	Util::FinishTable(pLua, "embree");
}

void CEmbreeModule::LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua)
{
	ReleaseEverything();
}
#endif