#include "module.h"
#include <GarrysMod/Lua/Interface.h>
#include "lua.h"

class CBitBufModule : public IModule
{
public:
	virtual void Init(CreateInterfaceFn* appfn, CreateInterfaceFn* gamefn);
	virtual void LuaInit(bool bServerInit);
	virtual void LuaShutdown();
	virtual void InitDetour(bool bPreServer);
	virtual void Think(bool bSimulating);
	virtual void Shutdown();
	virtual const char* Name() { return "bitbuf"; };
};

static CBitBufModule g_pBitBufModule;
IModule* pBitBufModule = &g_pBitBufModule;

static int bf_read_TypeID = -1;
void Push_bf_read(bf_read* tbl)
{
	if ( !tbl )
	{
		g_Lua->PushNil();
		return;
	}

	g_Lua->PushUserType(tbl, bf_read_TypeID);
}

void Push_Angle(QAngle* ang)
{
	if ( !ang )
	{
		g_Lua->PushNil();
		return;
	}

	g_Lua->PushUserType(ang, GarrysMod::Lua::Type::Angle);
}

void Push_Vector(Vector* vec)
{
	if ( !vec )
	{
		g_Lua->PushNil();
		return;
	}

	g_Lua->PushUserType(vec, GarrysMod::Lua::Type::Vector);
}

bf_read* Get_bf_read(int iStackPos)
{
	if (!g_Lua->IsType(iStackPos, bf_read_TypeID))
		return NULL;

	return g_Lua->GetUserType<bf_read>(iStackPos, bf_read_TypeID);
}

static int bf_write_TypeID = -1;
void Push_bf_write(bf_write* tbl)
{
	if ( !tbl )
	{
		g_Lua->PushNil();
		return;
	}

	g_Lua->PushUserType(tbl, bf_write_TypeID);
}

bf_write* Get_bf_write(int iStackPos)
{
	if (!g_Lua->IsType(iStackPos, bf_write_TypeID))
		return NULL;

	return g_Lua->GetUserType<bf_write>(iStackPos, bf_write_TypeID);
}

LUA_FUNCTION_STATIC(bf_read__tostring)
{
	bf_read* bf = Get_bf_read(1);
	if (!bf)
		LUA->ArgError(1, "bf_read");

	char szBuf[128] = {};
	V_snprintf(szBuf, sizeof(szBuf),"bf_read [%i]", bf->m_nDataBits); 
	LUA->PushString(szBuf);
	return 1;
}

LUA_FUNCTION_STATIC(bf_read__gc)
{
	bf_read* bf = Get_bf_read(1);
	if (bf)
		delete bf;

	return 0;
}

LUA_FUNCTION_STATIC(bf_read_GetNumBitsLeft)
{
	bf_read* bf = Get_bf_read(1);
	if (!bf)
		LUA->ArgError(1, "bf_read");

	LUA->PushNumber(bf->GetNumBitsLeft());
	return 1;
}

LUA_FUNCTION_STATIC(bf_read_GetNumBitsRead)
{
	bf_read* bf = Get_bf_read(1);
	if (!bf)
		LUA->ArgError(1, "bf_read");

	LUA->PushNumber(bf->GetNumBitsRead());
	return 1;
}

LUA_FUNCTION_STATIC(bf_read_GetNumBits)
{
	bf_read* bf = Get_bf_read(1);
	if (!bf)
		LUA->ArgError(1, "bf_read");

	LUA->PushNumber(bf->m_nDataBits);
	return 1;
}

LUA_FUNCTION_STATIC(bf_read_GetNumBytesLeft)
{
	bf_read* bf = Get_bf_read(1);
	if (!bf)
		LUA->ArgError(1, "bf_read");

	LUA->PushNumber(bf->GetNumBytesLeft());
	return 1;
}

LUA_FUNCTION_STATIC(bf_read_GetNumBytesRead)
{
	bf_read* bf = Get_bf_read(1);
	if (!bf)
		LUA->ArgError(1, "bf_read");

	LUA->PushNumber(bf->GetNumBytesRead());
	return 1;
}

LUA_FUNCTION_STATIC(bf_read_GetNumBytes)
{
	bf_read* bf = Get_bf_read(1);
	if (!bf)
		LUA->ArgError(1, "bf_read");

	LUA->PushNumber(bf->m_nDataBytes);
	return 1;
}

LUA_FUNCTION_STATIC(bf_read_GetCurrentBit)
{
	bf_read* bf = Get_bf_read(1);
	if (!bf)
		LUA->ArgError(1, "bf_read");

	LUA->PushNumber(bf->m_iCurBit);
	return 1;
}

LUA_FUNCTION_STATIC(bf_read_IsOverflowed)
{
	bf_read* bf = Get_bf_read(1);
	if (!bf)
		LUA->ArgError(1, "bf_read");

	LUA->PushBool(bf->IsOverflowed());
	return 1;
}

LUA_FUNCTION_STATIC(bf_read_PeekUBitLong)
{
	bf_read* bf = Get_bf_read(1);
	if (!bf)
		LUA->ArgError(1, "bf_read");

	int numbits = LUA->CheckNumber(2);
	LUA->PushNumber(bf->PeekUBitLong(numbits));
	return 1;
}

LUA_FUNCTION_STATIC(bf_read_ReadBitAngle)
{
	bf_read* bf = Get_bf_read(1);
	if (!bf)
		LUA->ArgError(1, "bf_read");

	int numbits = LUA->CheckNumber(2);
	LUA->PushNumber(bf->ReadBitAngle(numbits));
	return 1;
}

LUA_FUNCTION_STATIC(bf_read_ReadBitAngles)
{
	bf_read* bf = Get_bf_read(1);
	if (!bf)
		LUA->ArgError(1, "bf_read");

	QAngle ang;
	bf->ReadBitAngles(ang);
	Push_Angle(&ang);
	return 1;
}

LUA_FUNCTION_STATIC(bf_read_ReadBitCoord)
{
	bf_read* bf = Get_bf_read(1);
	if (!bf)
		LUA->ArgError(1, "bf_read");

	LUA->PushNumber(bf->ReadBitCoord());
	return 1;
}

LUA_FUNCTION_STATIC(bf_read_ReadBitCoordBits)
{
	bf_read* bf = Get_bf_read(1);
	if (!bf)
		LUA->ArgError(1, "bf_read");

	LUA->PushNumber(bf->ReadBitCoordBits());
	return 1;
}

LUA_FUNCTION_STATIC(bf_read_ReadBitCoordMP)
{
	bf_read* bf = Get_bf_read(1);
	if (!bf)
		LUA->ArgError(1, "bf_read");

	bool bIntegral = LUA->GetBool(2);
	bool bLowPrecision = LUA->GetBool(3);
	LUA->PushNumber(bf->ReadBitCoordMP(bIntegral, bLowPrecision));
	return 1;
}

LUA_FUNCTION_STATIC(bf_read_ReadBitCoordMPBits)
{
	bf_read* bf = Get_bf_read(1);
	if (!bf)
		LUA->ArgError(1, "bf_read");

	bool bIntegral = LUA->GetBool(2);
	bool bLowPrecision = LUA->GetBool(3);
	LUA->PushNumber(bf->ReadBitCoordMPBits(bIntegral, bLowPrecision));
	return 1;
}

LUA_FUNCTION_STATIC(bf_read_ReadBitFloat)
{
	bf_read* bf = Get_bf_read(1);
	if (!bf)
		LUA->ArgError(1, "bf_read");

	LUA->PushNumber(bf->ReadBitFloat());
	return 1;
}

LUA_FUNCTION_STATIC(bf_read_ReadBitLong)
{
	bf_read* bf = Get_bf_read(1);
	if (!bf)
		LUA->ArgError(1, "bf_read");

	int numBits = LUA->CheckNumber(2);
	bool bSigned = LUA->GetBool(3);
	LUA->PushNumber(bf->ReadBitLong(numBits, bSigned));
	return 1;
}

LUA_FUNCTION_STATIC(bf_read_ReadBitNormal)
{
	bf_read* bf = Get_bf_read(1);
	if (!bf)
		LUA->ArgError(1, "bf_read");

	LUA->PushNumber(bf->ReadBitNormal());
	return 1;
}

#define Bits2Bytes(b) ((b+7)>>3)
LUA_FUNCTION_STATIC(bf_read_ReadBits)
{
	bf_read* bf = Get_bf_read(1);
	if (!bf)
		LUA->ArgError(1, "bf_read");

	int numBits = LUA->CheckNumber(2);
	int size = PAD_NUMBER( Bits2Bytes(numBits), 4);
	byte* buffer = (byte*)stackalloc( size );
	bf->ReadBits(buffer, numBits);
	LUA->PushString((const char*)buffer);

	return 1;
}

LUA_FUNCTION_STATIC(bf_read_ReadBitVec3Coord)
{
	bf_read* bf = Get_bf_read(1);
	if (!bf)
		LUA->ArgError(1, "bf_read");

	Vector vec;
	bf->ReadBitVec3Coord(vec);
	Push_Vector(&vec);

	return 1;
}

LUA_FUNCTION_STATIC(bf_read_ReadBitVec3Normal)
{
	bf_read* bf = Get_bf_read(1);
	if (!bf)
		LUA->ArgError(1, "bf_read");

	Vector vec;
	bf->ReadBitVec3Normal(vec);
	Push_Vector(&vec);

	return 1;
}

LUA_FUNCTION_STATIC(bf_read_ReadByte)
{
	bf_read* bf = Get_bf_read(1);
	if (!bf)
		LUA->ArgError(1, "bf_read");

	LUA->PushNumber(bf->ReadByte());

	return 1;
}

LUA_FUNCTION_STATIC(bf_read_ReadBytes)
{
	bf_read* bf = Get_bf_read(1);
	if (!bf)
		LUA->ArgError(1, "bf_read");

	int numBytes = LUA->CheckNumber(2);
	byte* buffer = (byte*)stackalloc( numBytes );
	bf->ReadBytes(buffer, numBytes);
	LUA->PushString((const char*)buffer);

	return 1;
}

LUA_FUNCTION_STATIC(bf_read_ReadChar)
{
	bf_read* bf = Get_bf_read(1);
	if (!bf)
		LUA->ArgError(1, "bf_read");

	LUA->PushNumber(bf->ReadChar());

	return 1;
}

LUA_FUNCTION_STATIC(bf_read_ReadFloat)
{
	bf_read* bf = Get_bf_read(1);
	if (!bf)
		LUA->ArgError(1, "bf_read");

	LUA->PushNumber(bf->ReadFloat());

	return 1;
}

LUA_FUNCTION_STATIC(bf_read_ReadLong)
{
	bf_read* bf = Get_bf_read(1);
	if (!bf)
		LUA->ArgError(1, "bf_read");

	LUA->PushNumber(bf->ReadLong());

	return 1;
}

LUA_FUNCTION_STATIC(bf_read_ReadLongLong)
{
	bf_read* bf = Get_bf_read(1);
	if (!bf)
		LUA->ArgError(1, "bf_read");

	LUA->PushNumber(bf->ReadLongLong());

	return 1;
}

LUA_FUNCTION_STATIC(bf_read_ReadOneBit)
{
	bf_read* bf = Get_bf_read(1);
	if (!bf)
		LUA->ArgError(1, "bf_read");

	LUA->PushBool(bf->ReadOneBit());

	return 1;
}

LUA_FUNCTION_STATIC(bf_read_ReadSBitLong)
{
	bf_read* bf = Get_bf_read(1);
	if (!bf)
		LUA->ArgError(1, "bf_read");

	int numBits = LUA->CheckNumber(2);
	LUA->PushNumber(bf->ReadSBitLong(numBits));

	return 1;
}

LUA_FUNCTION_STATIC(bf_read_ReadShort)
{
	bf_read* bf = Get_bf_read(1);
	if (!bf)
		LUA->ArgError(1, "bf_read");

	LUA->PushNumber(bf->ReadShort());

	return 1;
}

LUA_FUNCTION_STATIC(bf_read_ReadSignedVarInt32)
{
	bf_read* bf = Get_bf_read(1);
	if (!bf)
		LUA->ArgError(1, "bf_read");

	LUA->PushNumber(bf->ReadSignedVarInt32());

	return 1;
}

LUA_FUNCTION_STATIC(bf_read_ReadSignedVarInt64)
{
	bf_read* bf = Get_bf_read(1);
	if (!bf)
		LUA->ArgError(1, "bf_read");

	LUA->PushNumber(bf->ReadSignedVarInt64());

	return 1;
}

LUA_FUNCTION_STATIC(bf_read_ReadString)
{
	bf_read* bf = Get_bf_read(1);
	if (!bf)
		LUA->ArgError(1, "bf_read");

	char str[1<<16]; // 1 << 16 is 64kb which is the max net message size.
	if (bf->ReadString(str, sizeof(str)))
		LUA->PushString(str);
	else
		LUA->PushNil();

	return 1;
}

LUA_FUNCTION_STATIC(bf_read_ReadUBitLong)
{
	bf_read* bf = Get_bf_read(1);
	if (!bf)
		LUA->ArgError(1, "bf_read");

	int numBits = LUA->CheckNumber(2);
	LUA->PushNumber(bf->ReadUBitLong(numBits));

	return 1;
}

LUA_FUNCTION_STATIC(bf_read_ReadUBitVar)
{
	bf_read* bf = Get_bf_read(1);
	if (!bf)
		LUA->ArgError(1, "bf_read");

	LUA->PushNumber(bf->ReadUBitVar());

	return 1;
}

LUA_FUNCTION_STATIC(bf_read_ReadVarInt32)
{
	bf_read* bf = Get_bf_read(1);
	if (!bf)
		LUA->ArgError(1, "bf_read");

	LUA->PushNumber(bf->ReadVarInt32());

	return 1;
}

LUA_FUNCTION_STATIC(bf_read_ReadVarInt64)
{
	bf_read* bf = Get_bf_read(1);
	if (!bf)
		LUA->ArgError(1, "bf_read");

	LUA->PushNumber(bf->ReadVarInt64());

	return 1;
}

LUA_FUNCTION_STATIC(bf_read_ReadWord)
{
	bf_read* bf = Get_bf_read(1);
	if (!bf)
		LUA->ArgError(1, "bf_read");

	LUA->PushNumber(bf->ReadWord());

	return 1;
}

void CBitBufModule::Init(CreateInterfaceFn* appfn, CreateInterfaceFn* gamefn)
{
}

void CBitBufModule::LuaInit(bool bServerInit)
{
	if (!bServerInit)
	{
		bf_read_TypeID = g_Lua->CreateMetaTable("bf_read");
			Util::AddFunc(bf_read__tostring, "__tostring");
			Util::AddFunc(bf_read__gc, "__gc");
			Util::AddFunc(bf_read_GetNumBitsLeft, "GetNumBitsLeft");
			Util::AddFunc(bf_read_GetNumBitsRead, "GetNumBitsRead");
			Util::AddFunc(bf_read_GetNumBits, "GetNumBits");
			Util::AddFunc(bf_read_GetNumBytesLeft, "GetNumBytesLeft");
			Util::AddFunc(bf_read_GetNumBytesRead, "GetNumBytesRead");
			Util::AddFunc(bf_read_GetNumBytes, "GetNumBytes");
			Util::AddFunc(bf_read_GetCurrentBit, "GetCurrentBit");
			Util::AddFunc(bf_read_IsOverflowed, "IsOverflowed");
			Util::AddFunc(bf_read_PeekUBitLong, "PeekUBitLong");
			Util::AddFunc(bf_read_ReadBitAngle, "ReadBitAngle");
			Util::AddFunc(bf_read_ReadBitAngles, "ReadBitAngles");
			Util::AddFunc(bf_read_ReadBitCoord, "ReadBitCoord");
			Util::AddFunc(bf_read_ReadBitCoordBits, "ReadBitCoordBits");
			Util::AddFunc(bf_read_ReadBitCoordMP, "ReadBitCoordMP");
			Util::AddFunc(bf_read_ReadBitCoordMPBits, "ReadBitCoordMPBits");
			Util::AddFunc(bf_read_ReadBitFloat, "ReadBitFloat");
			Util::AddFunc(bf_read_ReadBitLong, "ReadBitLong");
			Util::AddFunc(bf_read_ReadBitNormal, "ReadBitNormal");
			Util::AddFunc(bf_read_ReadBits, "ReadBits");
			Util::AddFunc(bf_read_ReadBitVec3Coord, "ReadBitVec3Coord");
			Util::AddFunc(bf_read_ReadBitVec3Normal, "ReadBitVec3Normal");
			Util::AddFunc(bf_read_ReadByte, "ReadByte");
			Util::AddFunc(bf_read_ReadBytes, "ReadBytes");
			Util::AddFunc(bf_read_ReadChar, "ReadChar");
			Util::AddFunc(bf_read_ReadFloat, "ReadFloat");
			Util::AddFunc(bf_read_ReadLong, "ReadLong");
			Util::AddFunc(bf_read_ReadLongLong, "ReadLongLong");
			Util::AddFunc(bf_read_ReadOneBit, "ReadOneBit");
			Util::AddFunc(bf_read_ReadSBitLong, "ReadSBitLong");
			Util::AddFunc(bf_read_ReadShort, "ReadShort");
			Util::AddFunc(bf_read_ReadSignedVarInt32, "ReadSignedVarInt32");
			Util::AddFunc(bf_read_ReadSignedVarInt64, "ReadSignedVarInt64");
			Util::AddFunc(bf_read_ReadString, "ReadString");
			Util::AddFunc(bf_read_ReadUBitLong, "ReadUBitLong");
			Util::AddFunc(bf_read_ReadUBitVar, "ReadUBitVar");
			Util::AddFunc(bf_read_ReadVarInt32, "ReadVarInt32");
			Util::AddFunc(bf_read_ReadVarInt64, "ReadVarInt64");
			Util::AddFunc(bf_read_ReadWord, "ReadWord");
		g_Lua->Pop(1); // ToDo: Add a IsValid function and maybe seek?
	}
}

void CBitBufModule::LuaShutdown()
{
}

void CBitBufModule::InitDetour(bool bPreServer)
{
	if ( bPreServer ) { return; }

}

void CBitBufModule::Think(bool simulating)
{
}

void CBitBufModule::Shutdown()
{
}