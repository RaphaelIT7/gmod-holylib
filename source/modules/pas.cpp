#include "module.h"
#include <GarrysMod/Lua/Interface.h>
#include "lua.h"
#include <sourcesdk/cmodel_private.h>
#include "player.h"

class CPASModule : public IModule
{
public:
	virtual void LuaInit(bool bServerInit) OVERRIDE;
	virtual void LuaShutdown() OVERRIDE;
	virtual void InitDetour(bool bPreServer) OVERRIDE;
	virtual const char* Name() { return "pas"; };
	virtual int Compatibility() { return LINUX32 | LINUX64; };
};

extern Vector* Get_Vector(int iStackPos);

CPASModule g_pPASModule;
IModule* pPASModule = &g_pPASModule;

// NOTE: We should move the CM stuff into a seperate file :|
CCollisionBSPData* gBSPData;
CCollisionBSPData g_BSPData; // The compiler needs it :<
int CM_PointLeafnum_r(CCollisionBSPData *pBSPData, const Vector& p, int num)
{
	float		d;
	cnode_t		*node;
	cplane_t	*plane;

	while (num >= 0)
	{
		node = pBSPData->map_rootnode + num;
		plane = node->plane;
		
		if (plane->type < 3)
			d = p[plane->type] - plane->dist;
		else
			d = DotProduct (plane->normal, p) - plane->dist;
		if (d < 0)
			num = node->children[1];
		else
			num = node->children[0];
	}

	return -1 - num;
}

int CM_PointLeafnum(const Vector& p)
{
	if (!gBSPData->numplanes)
		return 0;

	return CM_PointLeafnum_r(gBSPData, p, 0);
}

int CM_LeafCluster(int leafnum)
{
	Assert( leafnum >= 0 );
	Assert( leafnum < gBSPData->numleafs );

	return gBSPData->map_leafs[leafnum].cluster;
}

void CM_NullVis( CCollisionBSPData *pBSPData, byte *out )
{
	int numClusterBytes = (pBSPData->numclusters+7)>>3;	
	byte *out_p = out;

	while (numClusterBytes)
	{
		*out_p++ = 0xff;
		numClusterBytes--;
	}
}

void CM_DecompressVis( CCollisionBSPData *pBSPData, int cluster, int visType, byte *out )
{
	int		c;
	byte	*out_p;
	int		numClusterBytes;

	if ( !pBSPData )
		Assert( false );

	if ( cluster > pBSPData->numclusters || cluster < 0 )
	{
		CM_NullVis( pBSPData, out );
		return;
	}

	if ( !pBSPData->numvisibility || !pBSPData->map_vis )
	{	
		CM_NullVis( pBSPData, out );
		return;		
	}

	byte *in = ((byte *)pBSPData->map_vis) + pBSPData->map_vis->bitofs[cluster][visType];
	numClusterBytes = (pBSPData->numclusters+7)>>3;	
	out_p = out;

	if ( !in )
	{	
		CM_NullVis( pBSPData, out );
		return;		
	}

	do
	{
		if (*in)
		{
			*out_p++ = *in++;
			continue;
		}

		c = in[1];
		in += 2;
		if ((out_p - out) + c > numClusterBytes)
		{
			c = numClusterBytes - (out_p - out);
			ConMsg( "warning: Vis decompression overrun\n" );
		}
		while (c)
		{
			*out_p++ = 0;
			c--;
		}
	} while (out_p - out < numClusterBytes);
}

const byte *CM_Vis( byte *dest, int destlen, int cluster, int visType )
{
	CCollisionBSPData *pBSPData = GetCollisionBSPData();

	if ( !dest || visType > 2 || visType < 0 )
	{
		Warning( "CM_Vis: error");
		return NULL;
	}

	if ( cluster == -1 )
	{
		int len = (pBSPData->numclusters+7)>>3;
		if ( len > destlen )
		{
			Warning( "CM_Vis:  buffer not big enough (%i but need %i)\n",
				destlen, len );
		}
		memset( dest, 0, (pBSPData->numclusters+7)>>3 );
	}
	else
	{
		CM_DecompressVis( pBSPData, cluster, visType, dest );
	}

	return dest;
}

LUA_FUNCTION_STATIC(pas_TestPAS) // This is based off SV_DetermineMulticastRecipients
{
	Vector* orig;
	LUA->CheckType(1, GarrysMod::Lua::Type::Vector);
	//if (LUA->IsType(1, GarrysMod::Lua::Type::Vector))
	//{
		orig = Get_Vector(1);
	/*} else {
		LUA->CheckType(1, GarrysMod::Lua::Type::Entity);
		CBaseEntity* ent = Util::Get_Entity(1, false);
		if (!ent)
			LUA->ArgError(1, "Tried to use a NULL entity");

		orig = (Vector*)&ent->GetAbsOrigin(); // ToDo: This currently breaks the compile.
	}*/

	Vector* hearPos;
	LUA->CheckType(2, GarrysMod::Lua::Type::Vector);
	//if (LUA->IsType(2, GarrysMod::Lua::Type::Vector))
	//{
		hearPos = Get_Vector(2);
	/*} else {
		LUA->CheckType(2, GarrysMod::Lua::Type::Entity);
		CBaseEntity* ent = Util::Get_Entity(2, false);
		if (!ent)
			LUA->ArgError(2, "Tried to use a NULL entity");

		hearPos = (Vector*)&ent->GetAbsOrigin();
	}*/

	int cluster = CM_LeafCluster(CM_PointLeafnum(*orig));
	byte pas[MAX_MAP_LEAFS/8];
	const byte *pMask = CM_Vis(pas, sizeof(pas), cluster, DVIS_PAS);

	int iBitNumber = CM_LeafCluster(CM_PointLeafnum(*hearPos));

	LUA->PushBool(!(pMask[iBitNumber>>3] & (1<<(iBitNumber&7)))); // I think something is wrong here.
	return 1;
}

void CPASModule::LuaInit(bool bServerInit)
{
	if (bServerInit)
		return;

	Util::StartTable();
		Util::AddFunc(pas_TestPAS, "TestPAS");
	Util::FinishTable("pas");
}

void CPASModule::LuaShutdown()
{
	g_Lua->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
		g_Lua->PushNil();
		g_Lua->SetField(-2, "pas");
	g_Lua->Pop(1);
}

void CPASModule::InitDetour(bool bPreServer)
{
	if ( bPreServer ) { return; }

	SourceSDK::FactoryLoader engine_loader("engine");
	gBSPData = Detour::ResolveSymbol<CCollisionBSPData>(engine_loader, Symbols::g_BSPDataSym);
	Detour::CheckValue("get class", "CCollisionBSPData", gBSPData != NULL);
}