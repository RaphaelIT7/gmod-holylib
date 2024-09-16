#include "module.h"
#include <GarrysMod/Lua/Interface.h>
#include "lua.h"

class CBitBufModule : public IModule
{
public:
	virtual void LuaInit(bool bServerInit) OVERRIDE;
	virtual void LuaShutdown() OVERRIDE;
	virtual const char* Name() { return "bitbuf"; };
	virtual int Compatibility() { return LINUX32 | LINUX64; };
};

static CBitBufModule g_pBitBufModule;
IModule* pBitBufModule = &g_pBitBufModule;

static int bf_read_TypeID = -1;
void Push_bf_read(bf_read* buf)
{
	if (!buf)
	{
		g_Lua->PushNil();
		return;
	}

	g_Lua->PushUserType(buf, bf_read_TypeID);
}

void Push_Angle(QAngle* ang)
{
	if (!ang)
	{
		g_Lua->PushNil();
		return;
	}

	g_Lua->PushUserType(ang, GarrysMod::Lua::Type::Angle);
}

void Push_Vector(Vector* vec)
{
	if (!vec)
	{
		g_Lua->PushNil();
		return;
	}

	g_Lua->PushUserType(vec, GarrysMod::Lua::Type::Vector);
}

bf_read* Get_bf_read(int iStackPos, bool bError)
{
	if (bError)
	{
		if (!g_Lua->IsType(iStackPos, bf_read_TypeID))
			g_Lua->ThrowError("Tried to use something that wasn't bf_read!");

		bf_read* pBF = g_Lua->GetUserType<bf_read>(iStackPos, bf_read_TypeID);
		if (!pBF)
			g_Lua->ThrowError("Tried to use a NULL bf_read!");

		return pBF;
	} else {
		if (!g_Lua->IsType(iStackPos, bf_read_TypeID))
			return NULL;

		return g_Lua->GetUserType<bf_read>(iStackPos, bf_read_TypeID);
	}
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

bf_write* Get_bf_write(int iStackPos, bool bError)
{
	if (bError)
	{
		if (!g_Lua->IsType(iStackPos, bf_write_TypeID))
			return NULL;

		bf_write* pBF = g_Lua->GetUserType<bf_write>(iStackPos, bf_write_TypeID);
		if (!pBF)
			g_Lua->ThrowError("Tried to use a NULL bf_write!");

		return pBF;
	} else {
		if (!g_Lua->IsType(iStackPos, bf_write_TypeID))
			return NULL;

		return g_Lua->GetUserType<bf_write>(iStackPos, bf_write_TypeID);
	}
}

LUA_FUNCTION_STATIC(bf_read__tostring)
{
	bf_read* bf = Get_bf_read(1, false);
	if (!bf)
	{
		LUA->PushString("bf_read [NULL]");
	} else {
		char szBuf[128] = {};
		V_snprintf(szBuf, sizeof(szBuf),"bf_read [%i]", bf->m_nDataBits); 
		LUA->PushString(szBuf);
	}

	return 1;
}

LUA_FUNCTION_STATIC(bf_read__index)
{
	if (!g_Lua->FindOnObjectsMetaTable(1, 2))
		LUA->PushNil();

	return 1;
}

LUA_FUNCTION_STATIC(bf_read__gc)
{
	bf_read* bf = Get_bf_read(1, false);
	if (bf)
	{
		LUA->SetUserType(1, NULL);
		delete[] bf->GetBasePointer();
		delete bf;
	}

	return 0;
}

LUA_FUNCTION_STATIC(bf_read_IsValid)
{
	bf_read* bf = Get_bf_read(1, false);
	
	LUA->PushBool(bf != nullptr);
	return 1;
}

LUA_FUNCTION_STATIC(bf_read_GetNumBitsLeft)
{
	bf_read* bf = Get_bf_read(1, true);

	LUA->PushNumber(bf->GetNumBitsLeft());
	return 1;
}

LUA_FUNCTION_STATIC(bf_read_GetNumBitsRead)
{
	bf_read* bf = Get_bf_read(1, true);

	LUA->PushNumber(bf->GetNumBitsRead());
	return 1;
}

LUA_FUNCTION_STATIC(bf_read_GetNumBits)
{
	bf_read* bf = Get_bf_read(1, true);

	LUA->PushNumber(bf->m_nDataBits);
	return 1;
}

LUA_FUNCTION_STATIC(bf_read_GetNumBytesLeft)
{
	bf_read* bf = Get_bf_read(1, true);

	LUA->PushNumber(bf->GetNumBytesLeft());
	return 1;
}

LUA_FUNCTION_STATIC(bf_read_GetNumBytesRead)
{
	bf_read* bf = Get_bf_read(1, true);

	LUA->PushNumber(bf->GetNumBytesRead());
	return 1;
}

LUA_FUNCTION_STATIC(bf_read_GetNumBytes)
{
	bf_read* bf = Get_bf_read(1, true);

	LUA->PushNumber(bf->m_nDataBytes);
	return 1;
}

LUA_FUNCTION_STATIC(bf_read_GetCurrentBit)
{
	bf_read* bf = Get_bf_read(1, true);

#ifdef ARCHITECTURE_IS_X86_64
	LUA->ThrowError("This is 32x only.");
#else
	LUA->PushNumber(bf->m_iCurBit);
#endif
	return 1;
}

LUA_FUNCTION_STATIC(bf_read_IsOverflowed)
{
	bf_read* bf = Get_bf_read(1, true);

	LUA->PushBool(bf->IsOverflowed());
	return 1;
}

LUA_FUNCTION_STATIC(bf_read_PeekUBitLong)
{
	bf_read* bf = Get_bf_read(1, true);

	int numbits = LUA->CheckNumber(2);
	LUA->PushNumber(bf->PeekUBitLong(numbits));
	return 1;
}

LUA_FUNCTION_STATIC(bf_read_ReadBitAngle)
{
	bf_read* bf = Get_bf_read(1, true);

	int numbits = LUA->CheckNumber(2);
	LUA->PushNumber(bf->ReadBitAngle(numbits));
	return 1;
}

LUA_FUNCTION_STATIC(bf_read_ReadBitAngles)
{
	bf_read* bf = Get_bf_read(1, true);

	QAngle ang;
	bf->ReadBitAngles(ang);
	Push_Angle(&ang);
	return 1;
}


LUA_FUNCTION_STATIC(bf_read_ReadBitCoord)
{
	bf_read* bf = Get_bf_read(1, true);

#ifdef ARCHITECTURE_X86_64
	LUA->ThrowError("This is 32x only.");
#else
	LUA->PushNumber(bf->ReadBitCoord());
#endif
	return 1;
}

LUA_FUNCTION_STATIC(bf_read_ReadBitCoordBits)
{
	bf_read* bf = Get_bf_read(1, true);

#ifdef ARCHITECTURE_X86_64
	LUA->ThrowError("This is 32x only.");
#else
	LUA->PushNumber(bf->ReadBitCoordBits());
#endif
	return 1;
}

LUA_FUNCTION_STATIC(bf_read_ReadBitCoordMP)
{
	bf_read* bf = Get_bf_read(1, true);

#ifdef ARCHITECTURE_X86_64
	LUA->ThrowError("This is 32x only.");
#else
	bool bIntegral = LUA->GetBool(2);
	bool bLowPrecision = LUA->GetBool(3);
	LUA->PushNumber(bf->ReadBitCoordMP(bIntegral, bLowPrecision));
#endif
	return 1;
}

LUA_FUNCTION_STATIC(bf_read_ReadBitCoordMPBits)
{
	bf_read* bf = Get_bf_read(1, true);

#ifdef ARCHITECTURE_X86_64
	LUA->ThrowError("This is 32x only.");
#else
	bool bIntegral = LUA->GetBool(2);
	bool bLowPrecision = LUA->GetBool(3);
	LUA->PushNumber(bf->ReadBitCoordMPBits(bIntegral, bLowPrecision));
#endif
	return 1;
}

LUA_FUNCTION_STATIC(bf_read_ReadBitFloat)
{
	bf_read* bf = Get_bf_read(1, true);

	LUA->PushNumber(bf->ReadBitFloat());
	return 1;
}

LUA_FUNCTION_STATIC(bf_read_ReadBitLong)
{
	bf_read* bf = Get_bf_read(1, true);

#ifdef ARCHITECTURE_X86_64
	LUA->ThrowError("This is 32x only.");
#else
	int numBits = LUA->CheckNumber(2);
	bool bSigned = LUA->GetBool(3);
	LUA->PushNumber(bf->ReadBitLong(numBits, bSigned));
#endif
	return 1;
}

LUA_FUNCTION_STATIC(bf_read_ReadBitNormal)
{
	bf_read* bf = Get_bf_read(1, true);

	LUA->PushNumber(bf->ReadBitNormal());
	return 1;
}

#define Bits2Bytes(b) ((b+7)>>3)
LUA_FUNCTION_STATIC(bf_read_ReadBits)
{
	bf_read* bf = Get_bf_read(1, true);

	int numBits = LUA->CheckNumber(2);
	int size = PAD_NUMBER( Bits2Bytes(numBits), 4);
	byte* buffer = (byte*)stackalloc( size );
	bf->ReadBits(buffer, numBits);
	LUA->PushString((const char*)buffer);

	return 1;
}

LUA_FUNCTION_STATIC(bf_read_ReadBitVec3Coord)
{
	bf_read* bf = Get_bf_read(1, true);

	Vector vec;
	bf->ReadBitVec3Coord(vec);
	Push_Vector(&vec);

	return 1;
}

LUA_FUNCTION_STATIC(bf_read_ReadBitVec3Normal)
{
	bf_read* bf = Get_bf_read(1, true);

	Vector vec;
	bf->ReadBitVec3Normal(vec);
	Push_Vector(&vec);

	return 1;
}

LUA_FUNCTION_STATIC(bf_read_ReadByte)
{
	bf_read* bf = Get_bf_read(1, true);

	LUA->PushNumber(bf->ReadByte());

	return 1;
}

LUA_FUNCTION_STATIC(bf_read_ReadBytes)
{
	bf_read* bf = Get_bf_read(1, true);

	int numBytes = LUA->CheckNumber(2);
	byte* buffer = (byte*)stackalloc( numBytes );
	bf->ReadBytes(buffer, numBytes);
	LUA->PushString((const char*)buffer);

	return 1;
}

LUA_FUNCTION_STATIC(bf_read_ReadChar)
{
	bf_read* bf = Get_bf_read(1, true);

	LUA->PushNumber(bf->ReadChar());

	return 1;
}

LUA_FUNCTION_STATIC(bf_read_ReadFloat)
{
	bf_read* bf = Get_bf_read(1, true);

	LUA->PushNumber(bf->ReadFloat());

	return 1;
}

LUA_FUNCTION_STATIC(bf_read_ReadLong)
{
	bf_read* bf = Get_bf_read(1, true);

	LUA->PushNumber(bf->ReadLong());

	return 1;
}

LUA_FUNCTION_STATIC(bf_read_ReadLongLong)
{
	bf_read* bf = Get_bf_read(1, true);

	LUA->PushNumber(bf->ReadLongLong());

	return 1;
}

LUA_FUNCTION_STATIC(bf_read_ReadOneBit)
{
	bf_read* bf = Get_bf_read(1, true);

	LUA->PushBool(bf->ReadOneBit());

	return 1;
}

LUA_FUNCTION_STATIC(bf_read_ReadSBitLong)
{
	bf_read* bf = Get_bf_read(1, true);

	int numBits = LUA->CheckNumber(2);
	LUA->PushNumber(bf->ReadSBitLong(numBits));

	return 1;
}

LUA_FUNCTION_STATIC(bf_read_ReadShort)
{
	bf_read* bf = Get_bf_read(1, true);

	LUA->PushNumber(bf->ReadShort());

	return 1;
}

LUA_FUNCTION_STATIC(bf_read_ReadSignedVarInt32)
{
	bf_read* bf = Get_bf_read(1, true);

	LUA->PushNumber(bf->ReadSignedVarInt32());

	return 1;
}

LUA_FUNCTION_STATIC(bf_read_ReadSignedVarInt64)
{
	bf_read* bf = Get_bf_read(1, true);

	LUA->PushNumber(bf->ReadSignedVarInt64());

	return 1;
}

LUA_FUNCTION_STATIC(bf_read_ReadString)
{
	bf_read* bf = Get_bf_read(1, true);

	char str[1<<16]; // 1 << 16 is 64kb which is the max net message size.
	if (bf->ReadString(str, sizeof(str)))
		LUA->PushString(str);
	else
		LUA->PushNil();

	return 1;
}

LUA_FUNCTION_STATIC(bf_read_ReadUBitLong)
{
	bf_read* bf = Get_bf_read(1, true);

	int numBits = LUA->CheckNumber(2);
	LUA->PushNumber(bf->ReadUBitLong(numBits));

	return 1;
}

LUA_FUNCTION_STATIC(bf_read_ReadUBitVar)
{
	bf_read* bf = Get_bf_read(1, true);

	LUA->PushNumber(bf->ReadUBitVar());

	return 1;
}

LUA_FUNCTION_STATIC(bf_read_ReadVarInt32)
{
	bf_read* bf = Get_bf_read(1, true);

	LUA->PushNumber(bf->ReadVarInt32());

	return 1;
}

LUA_FUNCTION_STATIC(bf_read_ReadVarInt64)
{
	bf_read* bf = Get_bf_read(1, true);

	LUA->PushNumber(bf->ReadVarInt64());

	return 1;
}

LUA_FUNCTION_STATIC(bf_read_ReadWord)
{
	bf_read* bf = Get_bf_read(1, true);

	LUA->PushNumber(bf->ReadWord());

	return 1;
}

LUA_FUNCTION_STATIC(bf_read_Reset)
{
	bf_read* bf = Get_bf_read(1, true);

#ifdef ARCHITECTURE_X86_64
	LUA->ThrowError("This is 32x only.");
#else
	bf->Reset();
#endif

	return 0;
}

LUA_FUNCTION_STATIC(bf_read_Seek)
{
	bf_read* bf = Get_bf_read(1, true);

	LUA->PushBool(bf->Seek(LUA->CheckNumber(2)));

	return 1;
}

LUA_FUNCTION_STATIC(bf_read_SeekRelative)
{
	bf_read* bf = Get_bf_read(1, true);

	LUA->PushBool(bf->SeekRelative(LUA->CheckNumber(2)));

	return 1;
}

LUA_FUNCTION_STATIC(bf_read_GetData)
{
	bf_read* bf = Get_bf_read(1, true);

	LUA->PushString((const char*)bf->GetBasePointer(), bf->GetNumBytesLeft() + bf->GetNumBytesRead());
	return 1;
}

/*
 * bf_write
 */

LUA_FUNCTION_STATIC(bf_write__tostring)
{
	bf_write* bf = Get_bf_write(1, true);

	char szBuf[128] = {};
	V_snprintf(szBuf, sizeof(szBuf), "bf_write [%i]", bf->m_nDataBits);
	LUA->PushString(szBuf);
	return 1;
}

LUA_FUNCTION_STATIC(bf_write__index)
{
	if (!g_Lua->FindOnObjectsMetaTable(1, 2))
		LUA->PushNil();

	return 1;
}

LUA_FUNCTION_STATIC(bf_write__gc)
{
	bf_write* bf = Get_bf_write(1, false);
	if (bf)
	{
		LUA->SetUserType(1, NULL);
		delete[] bf->GetBasePointer();
		delete bf;
	}

	return 0;
}

LUA_FUNCTION_STATIC(bf_write_IsValid)
{
	bf_write* bf = Get_bf_write(1, false);

	LUA->PushBool(bf != nullptr);
	return 1;
}

LUA_FUNCTION_STATIC(bf_write_GetData)
{
	bf_write* pBF = Get_bf_write(1, true);

	LUA->PushString((const char*)pBF->GetBasePointer(), pBF->GetNumBytesWritten());
	return 1;
}

LUA_FUNCTION_STATIC(bf_write_GetNumBytesWritten)
{
	bf_write* pBF = Get_bf_write(1, true);

	LUA->PushNumber(pBF->GetNumBytesWritten());
	return 1;
}

LUA_FUNCTION_STATIC(bf_write_GetNumBytesLeft)
{
	bf_write* pBF = Get_bf_write(1, true);

	LUA->PushNumber(pBF->GetNumBytesLeft());
	return 1;
}

LUA_FUNCTION_STATIC(bf_write_GetNumBitsWritten)
{
	bf_write* pBF = Get_bf_write(1, true);

	LUA->PushNumber(pBF->GetNumBitsWritten());
	return 1;
}

LUA_FUNCTION_STATIC(bf_write_GetNumBitsLeft)
{
	bf_write* pBF = Get_bf_write(1, true);

	LUA->PushNumber(pBF->GetNumBitsLeft());
	return 1;
}

LUA_FUNCTION_STATIC(bf_write_GetMaxNumBits)
{
	bf_write* pBF = Get_bf_write(1, true);

	LUA->PushNumber(pBF->GetMaxNumBits());
	return 1;
}

LUA_FUNCTION_STATIC(bf_write_IsOverflowed)
{
	bf_write* pBF = Get_bf_write(1, true);

	LUA->PushBool(pBF->IsOverflowed());
	return 1;
}

LUA_FUNCTION_STATIC(bf_write_Reset)
{
	bf_write* pBF = Get_bf_write(1, true);

	pBF->Reset();
	return 0;
}

LUA_FUNCTION_STATIC(bf_write_GetDebugName)
{
	bf_write* pBF = Get_bf_write(1, true);

	LUA->PushString(pBF->GetDebugName());
	return 1;
}

LUA_FUNCTION_STATIC(bf_write_SetDebugName)
{
	bf_write* pBF = Get_bf_write(1, true);

	pBF->SetDebugName(LUA->CheckString(2)); // BUG: Do we need to keep a reference?
	return 0;
}

LUA_FUNCTION_STATIC(bf_write_SeekToBit)
{
	bf_write* pBF = Get_bf_write(1, true);

	pBF->SeekToBit(LUA->CheckNumber(2));
	return 0;
}

LUA_FUNCTION_STATIC(bf_write_WriteFloat)
{
	bf_write* pBF = Get_bf_write(1, true);

	pBF->WriteFloat(LUA->CheckNumber(2));
	return 0;
}

LUA_FUNCTION_STATIC(bf_write_WriteChar)
{
	bf_write* pBF = Get_bf_write(1, true);

	pBF->WriteChar(LUA->CheckNumber(2));
	return 0;
}

LUA_FUNCTION_STATIC(bf_write_WriteByte)
{
	bf_write* pBF = Get_bf_write(1, true);

	pBF->WriteByte(LUA->CheckNumber(2));
	return 0;
}

LUA_FUNCTION_STATIC(bf_write_WriteLong)
{
	bf_write* pBF = Get_bf_write(1, true);

	pBF->WriteLong(LUA->CheckNumber(2));
	return 0;
}

LUA_FUNCTION_STATIC(bf_write_WriteLongLong)
{
	bf_write* pBF = Get_bf_write(1, true);

	pBF->WriteLongLong(LUA->CheckNumber(2));
	return 0;
}

LUA_FUNCTION_STATIC(bf_write_WriteBytes)
{
	bf_write* pBF = Get_bf_write(1, true);

	const char* pData = LUA->CheckString(2);
	int iLength = LUA->ObjLen(2);
	pBF->WriteBytes(pData, iLength);
	return 0;
}

LUA_FUNCTION_STATIC(bf_write_WriteOneBit)
{
	bf_write* pBF = Get_bf_write(1, true);

	pBF->WriteOneBit(LUA->GetBool(2));
	return 0;
}

LUA_FUNCTION_STATIC(bf_write_WriteOneBitAt)
{
	bf_write* pBF = Get_bf_write(1, true);

	pBF->WriteOneBitAt(LUA->CheckNumber(2), LUA->GetBool(3));
	return 0;
}

LUA_FUNCTION_STATIC(bf_write_WriteShort)
{
	bf_write* pBF = Get_bf_write(1, true);

	pBF->WriteShort(LUA->CheckNumber(2));
	return 0;
}

LUA_FUNCTION_STATIC(bf_write_WriteWord)
{
	bf_write* pBF = Get_bf_write(1, true);

	pBF->WriteWord(LUA->CheckNumber(2));
	return 0;
}

LUA_FUNCTION_STATIC(bf_write_WriteSignedVarInt32)
{
	bf_write* pBF = Get_bf_write(1, true);

	pBF->WriteSignedVarInt32(LUA->CheckNumber(2));
	return 0;
}

LUA_FUNCTION_STATIC(bf_write_WriteSignedVarInt64)
{
	bf_write* pBF = Get_bf_write(1, true);

	pBF->WriteSignedVarInt64(LUA->CheckNumber(2));
	return 0;
}

LUA_FUNCTION_STATIC(bf_write_WriteVarInt32)
{
	bf_write* pBF = Get_bf_write(1, true);

	pBF->WriteVarInt32(LUA->CheckNumber(2));
	return 0;
}

LUA_FUNCTION_STATIC(bf_write_WriteVarInt64)
{
	bf_write* pBF = Get_bf_write(1, true);

	pBF->WriteVarInt64(LUA->CheckNumber(2));
	return 0;
}

LUA_FUNCTION_STATIC(bf_write_WriteUBitVar)
{
	bf_write* pBF = Get_bf_write(1, true);

	pBF->WriteUBitVar(LUA->CheckNumber(2));
	return 0;
}

LUA_FUNCTION_STATIC(bf_write_WriteUBitLong)
{
	bf_write* pBF = Get_bf_write(1, true);

	pBF->WriteUBitLong(LUA->CheckNumber(2), LUA->CheckNumber(3), LUA->GetBool(4));
	return 0;
}

LUA_FUNCTION_STATIC(bf_write_WriteBitAngle)
{
	bf_write* pBF = Get_bf_write(1, true);

	pBF->WriteBitAngle(LUA->CheckNumber(2), LUA->CheckNumber(3));
	return 0;
}

LUA_FUNCTION_STATIC(bf_write_WriteBitAngles)
{
	bf_write* pBF = Get_bf_write(1, true);

	QAngle* ang = LUA->GetUserType<QAngle>(2, GarrysMod::Lua::Type::Angle);
	pBF->WriteBitAngles(*ang);
	return 0;
}

LUA_FUNCTION_STATIC(bf_write_WriteBitVec3Coord)
{
	bf_write* pBF = Get_bf_write(1, true);

	Vector* vec = LUA->GetUserType<Vector>(2, GarrysMod::Lua::Type::Vector );
	pBF->WriteBitVec3Coord(*vec);
	return 0;
}

LUA_FUNCTION_STATIC(bf_write_WriteBitVec3Normal)
{
	bf_write* pBF = Get_bf_write(1, true);

	Vector* vec = LUA->GetUserType<Vector>(2, GarrysMod::Lua::Type::Vector);
	pBF->WriteBitVec3Normal(*vec);
	return 0;
}

LUA_FUNCTION_STATIC(bf_write_WriteBits)
{
	bf_write* pBF = Get_bf_write(1, true);

	const char* pData = LUA->CheckString(2);
	int iBits = LUA->CheckNumber(3);
	LUA->PushBool(pBF->WriteBits(pData, iBits));
	return 1;
}

LUA_FUNCTION_STATIC(bf_write_WriteBitsFromBuffer)
{
	bf_write* pBF = Get_bf_write(1, true);
	bf_read* pBFRead = Get_bf_read(2, true);

	int iBits = LUA->CheckNumber(3);
	LUA->PushBool(pBF->WriteBitsFromBuffer(pBFRead, iBits));
	return 1;
}

LUA_FUNCTION_STATIC(bf_write_WriteBitNormal)
{
	bf_write* pBF = Get_bf_write(1, true);

	pBF->WriteBitNormal(LUA->CheckNumber(2));
	return 0;
}

LUA_FUNCTION_STATIC(bf_write_WriteBitLong)
{
	bf_write* pBF = Get_bf_write(1, true);

	pBF->WriteBitLong(LUA->CheckNumber(2), LUA->CheckNumber(3), LUA->GetBool(4));
	return 0;
}

LUA_FUNCTION_STATIC(bf_write_WriteBitFloat)
{
	bf_write* pBF = Get_bf_write(1, true);

	pBF->WriteBitFloat(LUA->CheckNumber(2));
	return 0;
}

LUA_FUNCTION_STATIC(bf_write_WriteBitCoord)
{
	bf_write* pBF = Get_bf_write(1, true);

	pBF->WriteBitCoord(LUA->CheckNumber(2));
	return 0;
}

LUA_FUNCTION_STATIC(bf_write_WriteBitCoordMP)
{
	bf_write* pBF = Get_bf_write(1, true);

	bool bIntegral = LUA->GetBool(2);
	bool bLowPrecision = LUA->GetBool(3);
#ifdef ARCHITECTURE_X86_64
	EBitCoordType pType = kCW_None;
	if (bIntegral)
		pType = EBitCoordType::kCW_Integral;

	if (bLowPrecision)
		pType = EBitCoordType::kCW_LowPrecision;

	pBF->WriteBitCoordMP(LUA->CheckNumber(2), pType);
#else
	pBF->WriteBitCoordMP(LUA->CheckNumber(2), bIntegral, bLowPrecision);
#endif
	return 0;
}

LUA_FUNCTION_STATIC(bitbuf_CopyReadBuffer)
{
	bf_read* pBf = Get_bf_read(1, true);

	int iSize = pBf->GetNumBytesRead() + pBf->GetNumBytesLeft();
	unsigned char* pData = new unsigned char[iSize + 1];
	memcpy(pData, pBf->GetBasePointer(), iSize);

	bf_read* pNewBf = new bf_read;
	pNewBf->StartReading(pData, iSize);

	Push_bf_read(pNewBf);

	return 1;
}

LUA_FUNCTION_STATIC(bitbuf_CreateReadBuffer)
{
	const char* pData = LUA->CheckString(1);
	int iLength = LUA->ObjLen(1);

	unsigned char* cData = new unsigned char[iLength + 1];
	memcpy(cData, pData, iLength);

	bf_read* pNewBf = new bf_read;
	pNewBf->StartReading(cData, iLength);

	Push_bf_read(pNewBf);

	return 1;
}

LUA_FUNCTION_STATIC(bitbuf_CreateWriteBuffer)
{
	if (LUA->IsType(1, GarrysMod::Lua::Type::Number))
	{
		int iSize = LUA->CheckNumber(1);
		unsigned char* cData = new unsigned char[iSize];

		bf_write* pNewBf = new bf_write;
		pNewBf->StartWriting(cData, iSize);

		Push_bf_write(pNewBf);
	} else {
		const char* pData = LUA->CheckString(1);
		int iLength = LUA->ObjLen(1);

		unsigned char* cData = new unsigned char[iLength + 1];
		memcpy(cData, pData, iLength);

		bf_write* pNewBf = new bf_write;
		pNewBf->StartWriting(cData, iLength);

		Push_bf_write(pNewBf);
	}

	return 1;
}

void CBitBufModule::LuaInit(bool bServerInit)
{
	if (bServerInit)
		return;

	bf_read_TypeID = g_Lua->CreateMetaTable("bf_read");
		Util::AddFunc(bf_read__tostring, "__tostring");
		Util::AddFunc(bf_read__index, "__index");
		Util::AddFunc(bf_read__gc, "__gc");
		Util::AddFunc(bf_read_IsValid, "IsValid");
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

		// Functions to change the current bit
		Util::AddFunc(bf_read_Reset, "Reset");
		Util::AddFunc(bf_read_Seek, "Seek");
		Util::AddFunc(bf_read_SeekRelative, "SeekRelative");

		// Other functions
		Util::AddFunc(bf_read_GetData, "GetData");
	g_Lua->Pop(1);

	bf_write_TypeID = g_Lua->CreateMetaTable("bf_write");
		Util::AddFunc(bf_write__tostring, "__tostring");
		Util::AddFunc(bf_write__index, "__index");
		Util::AddFunc(bf_write__gc, "__gc");
		Util::AddFunc(bf_write_IsValid, "IsValid");
		Util::AddFunc(bf_write_GetData, "GetData");
		Util::AddFunc(bf_write_GetNumBytesWritten, "GetNumBytesWritten");
		Util::AddFunc(bf_write_GetNumBytesLeft, "GetNumBytesLeft");
		Util::AddFunc(bf_write_GetNumBitsWritten, "GetNumBitsWritten");
		Util::AddFunc(bf_write_GetNumBitsLeft, "GetNumBitsLeft");
		Util::AddFunc(bf_write_GetMaxNumBits, "GetMaxNumBits");
		Util::AddFunc(bf_write_IsOverflowed, "IsOverflowed");
		Util::AddFunc(bf_write_Reset, "Reset");
		Util::AddFunc(bf_write_GetDebugName, "GetDebugName");
		Util::AddFunc(bf_write_SetDebugName, "SetDebugName");
		Util::AddFunc(bf_write_SeekToBit, "SeekToBit");
		Util::AddFunc(bf_write_WriteFloat, "WriteFloat");
		Util::AddFunc(bf_write_WriteChar, "WriteChar");
		Util::AddFunc(bf_write_WriteByte, "WriteByte");
		Util::AddFunc(bf_write_WriteLong, "WriteLong");
		Util::AddFunc(bf_write_WriteLongLong, "WriteLongLong");
		Util::AddFunc(bf_write_WriteBytes, "WriteBytes");
		Util::AddFunc(bf_write_WriteOneBit, "WriteOneBit");
		Util::AddFunc(bf_write_WriteOneBitAt, "WriteOneBitAt");
		Util::AddFunc(bf_write_WriteShort, "WriteShort");
		Util::AddFunc(bf_write_WriteWord, "WriteWord");
		Util::AddFunc(bf_write_WriteSignedVarInt32, "WriteSignedVarInt32");
		Util::AddFunc(bf_write_WriteSignedVarInt64, "WriteSignedVarInt64");
		Util::AddFunc(bf_write_WriteVarInt32, "WriteVarInt32");
		Util::AddFunc(bf_write_WriteVarInt64, "WriteVarInt64");
		Util::AddFunc(bf_write_WriteUBitVar, "WriteUBitVar");
		Util::AddFunc(bf_write_WriteUBitLong, "WriteUBitLong");
		Util::AddFunc(bf_write_WriteBitAngle, "WriteBitAngle");
		Util::AddFunc(bf_write_WriteBitAngles, "WriteBitAngles");
		Util::AddFunc(bf_write_WriteBitVec3Coord, "WriteBitVec3Coord");
		Util::AddFunc(bf_write_WriteBitVec3Normal, "WriteBitVec3normal");
		Util::AddFunc(bf_write_WriteBits, "WriteBits");
		Util::AddFunc(bf_write_WriteBitsFromBuffer, "WriteBitsFromBuffer");
		Util::AddFunc(bf_write_WriteBitNormal, "WriteBitNormal");
		Util::AddFunc(bf_write_WriteBitLong, "WriteBitLong");
		Util::AddFunc(bf_write_WriteBitFloat, "WriteBitFloat");
		Util::AddFunc(bf_write_WriteBitCoord, "WriteBitCoord");
		Util::AddFunc(bf_write_WriteBitCoordMP, "WriteBitCoordMP");
		
	g_Lua->Pop(1);

	Util::StartTable();
		Util::AddFunc(bitbuf_CopyReadBuffer, "CopyReadBuffer");
		Util::AddFunc(bitbuf_CreateReadBuffer, "CreateReadBuffer");
		Util::AddFunc(bitbuf_CreateWriteBuffer, "CreateWriteBuffer");
	Util::FinishTable("bitbuf");
}

void CBitBufModule::LuaShutdown()
{
	Util::NukeTable("bitbuf");
}