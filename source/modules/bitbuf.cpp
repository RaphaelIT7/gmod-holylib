#include "LuaInterface.h"
#include "detours.h"
#include "module.h"
#include "lua.h"
#include "bitbuf.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CBitBufModule : public IModule
{
public:
	virtual void LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit) OVERRIDE;
	virtual void LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua) OVERRIDE;
	virtual const char* Name() { return "bitbuf"; };
	virtual int Compatibility() { return LINUX32 | LINUX64 | WINDOWS32 | WINDOWS64; };
};

static CBitBufModule g_pBitBufModule;
IModule* pBitBufModule = &g_pBitBufModule;

Push_LuaClass(bf_read)
Get_LuaClass(bf_read, "bf_read")

Push_LuaClass(bf_write)
Get_LuaClass(bf_write, "bf_write")

LUA_FUNCTION_STATIC(bf_read__tostring)
{
	bf_read* bf = Get_bf_read(LUA, 1, false);
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

Default__index(bf_read);
Default__newindex(bf_read);
Default__GetTable(bf_read);
Default__gc(bf_read, 
	bf_read* bf = (bf_read*)pStoredData;
	if (bf)
	{
		delete[] bf->GetBasePointer();
		delete bf;
	}
)

LUA_FUNCTION_STATIC(bf_read_IsValid)
{
	bf_read* bf = Get_bf_read(LUA, 1, false);
	
	LUA->PushBool(bf != nullptr);
	return 1;
}

LUA_FUNCTION_STATIC(bf_read_GetNumBitsLeft)
{
	bf_read* bf = Get_bf_read(LUA, 1, true);

	LUA->PushNumber(bf->GetNumBitsLeft());
	return 1;
}

LUA_FUNCTION_STATIC(bf_read_GetNumBitsRead)
{
	bf_read* bf = Get_bf_read(LUA, 1, true);

	LUA->PushNumber(bf->GetNumBitsRead());
	return 1;
}

LUA_FUNCTION_STATIC(bf_read_GetNumBits)
{
	bf_read* bf = Get_bf_read(LUA, 1, true);

	LUA->PushNumber(bf->m_nDataBits);
	return 1;
}

LUA_FUNCTION_STATIC(bf_read_GetNumBytesLeft)
{
	bf_read* bf = Get_bf_read(LUA, 1, true);

	LUA->PushNumber(bf->GetNumBytesLeft());
	return 1;
}

LUA_FUNCTION_STATIC(bf_read_GetNumBytesRead)
{
	bf_read* bf = Get_bf_read(LUA, 1, true);

	LUA->PushNumber(bf->GetNumBytesRead());
	return 1;
}

LUA_FUNCTION_STATIC(bf_read_GetNumBytes)
{
	bf_read* bf = Get_bf_read(LUA, 1, true);

	LUA->PushNumber(bf->m_nDataBytes);
	return 1;
}

LUA_FUNCTION_STATIC(bf_read_GetCurrentBit)
{
#if ARCHITECTURE_IS_X86
	bf_read* bf = Get_bf_read(LUA, 1, true);
#endif

#if ARCHITECTURE_IS_X86_64
	LUA->ThrowError("This is 32x only.");
#else
	LUA->PushNumber(bf->m_iCurBit);
#endif
	return 1;
}

LUA_FUNCTION_STATIC(bf_read_IsOverflowed)
{
	bf_read* bf = Get_bf_read(LUA, 1, true);

	LUA->PushBool(bf->IsOverflowed());
	return 1;
}

LUA_FUNCTION_STATIC(bf_read_PeekUBitLong)
{
	bf_read* bf = Get_bf_read(LUA, 1, true);

	int numbits = (int)LUA->CheckNumber(2);
	LUA->PushNumber(bf->PeekUBitLong(numbits));
	return 1;
}

LUA_FUNCTION_STATIC(bf_read_ReadBitAngle)
{
	bf_read* bf = Get_bf_read(LUA, 1, true);

	int numbits = (int)LUA->CheckNumber(2);
	LUA->PushNumber(bf->ReadBitAngle(numbits));
	return 1;
}

LUA_FUNCTION_STATIC(bf_read_ReadBitAngles)
{
	bf_read* bf = Get_bf_read(LUA, 1, true);

	QAngle ang;
	bf->ReadBitAngles(ang);
	Push_QAngle(LUA, &ang);
	return 1;
}


LUA_FUNCTION_STATIC(bf_read_ReadBitCoord)
{
#if ARCHITECTURE_IS_X86
	bf_read* bf = Get_bf_read(LUA, 1, true);
#endif

#if ARCHITECTURE_IS_X86_64
	LUA->ThrowError("This is 32x only.");
#else
	LUA->PushNumber(bf->ReadBitCoord());
#endif
	return 1;
}

LUA_FUNCTION_STATIC(bf_read_ReadBitCoordBits)
{
#if ARCHITECTURE_IS_X86
	bf_read* bf = Get_bf_read(LUA, 1, true);
#endif

#if ARCHITECTURE_IS_X86_64
	LUA->ThrowError("This is 32x only.");
#else
	LUA->PushNumber(bf->ReadBitCoordBits());
#endif
	return 1;
}

LUA_FUNCTION_STATIC(bf_read_ReadBitCoordMP)
{
#if ARCHITECTURE_IS_X86
	bf_read* bf = Get_bf_read(LUA, 1, true);
#endif

#if ARCHITECTURE_IS_X86_64
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
#if ARCHITECTURE_IS_X86
	bf_read* bf = Get_bf_read(LUA, 1, true);
#endif

#if ARCHITECTURE_IS_X86_64
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
	bf_read* bf = Get_bf_read(LUA, 1, true);

	LUA->PushNumber(bf->ReadBitFloat());
	return 1;
}

LUA_FUNCTION_STATIC(bf_read_ReadBitLong)
{
#if ARCHITECTURE_IS_X86
	bf_read* bf = Get_bf_read(LUA, 1, true);
#endif

#if ARCHITECTURE_IS_X86_64
	LUA->ThrowError("This is 32x only.");
#else
	int numBits = (int)LUA->CheckNumber(2);
	bool bSigned = LUA->GetBool(3);
	LUA->PushNumber(bf->ReadBitLong(numBits, bSigned));
#endif
	return 1;
}

LUA_FUNCTION_STATIC(bf_read_ReadBitNormal)
{
	bf_read* bf = Get_bf_read(LUA, 1, true);

	LUA->PushNumber(bf->ReadBitNormal());
	return 1;
}

#define Bits2Bytes(b) ((b+7)>>3)
LUA_FUNCTION_STATIC(bf_read_ReadBits)
{
	bf_read* bf = Get_bf_read(LUA, 1, true);

	int numBits = (int)LUA->CheckNumber(2);
	int size = PAD_NUMBER( Bits2Bytes(numBits), 4);
	byte* buffer = (byte*)stackalloc( size );
	bf->ReadBits(buffer, numBits);
	LUA->PushString((const char*)buffer);

	return 1;
}

LUA_FUNCTION_STATIC(bf_read_ReadBitVec3Coord)
{
	bf_read* bf = Get_bf_read(LUA, 1, true);

	Vector vec;
	bf->ReadBitVec3Coord(vec);
	Push_Vector(LUA, &vec);

	return 1;
}

LUA_FUNCTION_STATIC(bf_read_ReadBitVec3Normal)
{
	bf_read* bf = Get_bf_read(LUA, 1, true);

	Vector vec;
	bf->ReadBitVec3Normal(vec);
	Push_Vector(LUA, &vec);

	return 1;
}

LUA_FUNCTION_STATIC(bf_read_ReadByte)
{
	bf_read* bf = Get_bf_read(LUA, 1, true);

	LUA->PushNumber(bf->ReadByte());
	return 1;
}

LUA_FUNCTION_STATIC(bf_read_ReadBytes)
{
	bf_read* bf = Get_bf_read(LUA, 1, true);

	int numBytes = (int)LUA->CheckNumber(2);
	byte* buffer = (byte*)stackalloc( numBytes );
	bf->ReadBytes(buffer, numBytes);
	LUA->PushString((const char*)buffer);

	return 1;
}

LUA_FUNCTION_STATIC(bf_read_ReadChar)
{
	bf_read* bf = Get_bf_read(LUA, 1, true);

	LUA->PushNumber(bf->ReadChar());
	return 1;
}

LUA_FUNCTION_STATIC(bf_read_ReadFloat)
{
	bf_read* bf = Get_bf_read(LUA, 1, true);

	LUA->PushNumber(bf->ReadFloat());
	return 1;
}

LUA_FUNCTION_STATIC(bf_read_ReadLong)
{
	bf_read* bf = Get_bf_read(LUA, 1, true);

	LUA->PushNumber(bf->ReadLong());
	return 1;
}

LUA_FUNCTION_STATIC(bf_read_ReadLongLong)
{
	bf_read* bf = Get_bf_read(LUA, 1, true);

	LUA->PushString(std::to_string(bf->ReadLongLong()).c_str());
	return 1;
}

LUA_FUNCTION_STATIC(bf_read_ReadOneBit)
{
	bf_read* bf = Get_bf_read(LUA, 1, true);

	LUA->PushBool(bf->ReadOneBit());
	return 1;
}

LUA_FUNCTION_STATIC(bf_read_ReadSBitLong)
{
	bf_read* bf = Get_bf_read(LUA, 1, true);

	int numBits = (int)LUA->CheckNumber(2);
	LUA->PushNumber(bf->ReadSBitLong(numBits));
	return 1;
}

LUA_FUNCTION_STATIC(bf_read_ReadShort)
{
	bf_read* bf = Get_bf_read(LUA, 1, true);

	LUA->PushNumber(bf->ReadShort());
	return 1;
}

LUA_FUNCTION_STATIC(bf_read_ReadSignedVarInt32)
{
	bf_read* bf = Get_bf_read(LUA, 1, true);

	LUA->PushNumber(bf->ReadSignedVarInt32());
	return 1;
}

LUA_FUNCTION_STATIC(bf_read_ReadSignedVarInt64)
{
	bf_read* bf = Get_bf_read(LUA, 1, true);

	LUA->PushNumber((double)bf->ReadSignedVarInt64());
	return 1;
}

LUA_FUNCTION_STATIC(bf_read_ReadString)
{
	bf_read* bf = Get_bf_read(LUA, 1, true);

	int iSize = 1 << 16;
	char* pStr = new char[iSize]; // 1 << 16 is 64kb which is the max net message size.
	if (bf->ReadString(pStr, iSize))
		LUA->PushString(pStr);
	else
		LUA->PushNil();

	delete[] pStr;

	return 1;
}

LUA_FUNCTION_STATIC(bf_read_ReadUBitLong)
{
	bf_read* bf = Get_bf_read(LUA, 1, true);

	int numBits = (int)LUA->CheckNumber(2);
	LUA->PushNumber(bf->ReadUBitLong(numBits));
	return 1;
}

LUA_FUNCTION_STATIC(bf_read_ReadUBitVar)
{
	bf_read* bf = Get_bf_read(LUA, 1, true);

	LUA->PushNumber(bf->ReadUBitVar());
	return 1;
}

LUA_FUNCTION_STATIC(bf_read_ReadVarInt32)
{
	bf_read* bf = Get_bf_read(LUA, 1, true);

	LUA->PushNumber(bf->ReadVarInt32());
	return 1;
}

LUA_FUNCTION_STATIC(bf_read_ReadVarInt64)
{
	bf_read* bf = Get_bf_read(LUA, 1, true);

	LUA->PushNumber((double)bf->ReadVarInt64());
	return 1;
}

LUA_FUNCTION_STATIC(bf_read_ReadWord)
{
	bf_read* bf = Get_bf_read(LUA, 1, true);

	LUA->PushNumber(bf->ReadWord());
	return 1;
}

LUA_FUNCTION_STATIC(bf_read_Reset)
{
#if ARCHITECTURE_IS_X86
	bf_read* bf = Get_bf_read(LUA, 1, true);
#endif

#if ARCHITECTURE_IS_X86_64
	LUA->ThrowError("This is 32x only.");
#else
	bf->Reset();
#endif

	return 0;
}

LUA_FUNCTION_STATIC(bf_read_Seek)
{
	bf_read* bf = Get_bf_read(LUA, 1, true);

	LUA->PushBool(bf->Seek((int)LUA->CheckNumber(2)));
	return 1;
}

LUA_FUNCTION_STATIC(bf_read_SeekRelative)
{
	bf_read* bf = Get_bf_read(LUA, 1, true);

	LUA->PushBool(bf->SeekRelative((int)LUA->CheckNumber(2)));
	return 1;
}

LUA_FUNCTION_STATIC(bf_read_GetData)
{
	bf_read* bf = Get_bf_read(LUA, 1, true);

	LUA->PushString((const char*)bf->GetBasePointer(), bf->GetNumBytesLeft() + bf->GetNumBytesRead());
	return 1;
}

/*
 * bf_write
 */

LUA_FUNCTION_STATIC(bf_write__tostring)
{
	bf_write* bf = Get_bf_write(LUA, 1, false);
	if (!bf)
	{
		LUA->PushString("bf_write [NULL]");
	} else {
		char szBuf[128] = {};
		V_snprintf(szBuf, sizeof(szBuf),"bf_write [%i]", bf->m_nDataBits); 
		LUA->PushString(szBuf);
	}
	return 1;
}

Default__index(bf_write);
Default__newindex(bf_write);
Default__GetTable(bf_write);
Default__gc(bf_write, 
	bf_write* bf = (bf_write*)pStoredData;
	if (bf)
	{
		delete[] bf->GetBasePointer();
		delete bf;
	}
)

LUA_FUNCTION_STATIC(bf_write_IsValid)
{
	bf_write* bf = Get_bf_write(LUA, 1, false);

	LUA->PushBool(bf != nullptr);
	return 1;
}

LUA_FUNCTION_STATIC(bf_write_GetData)
{
	bf_write* pBF = Get_bf_write(LUA, 1, true);

	LUA->PushString((const char*)pBF->GetBasePointer(), pBF->GetNumBytesWritten());
	return 1;
}

LUA_FUNCTION_STATIC(bf_write_GetNumBytesWritten)
{
	bf_write* pBF = Get_bf_write(LUA, 1, true);

	LUA->PushNumber(pBF->GetNumBytesWritten());
	return 1;
}

LUA_FUNCTION_STATIC(bf_write_GetNumBytesLeft)
{
	bf_write* pBF = Get_bf_write(LUA, 1, true);

	LUA->PushNumber(pBF->GetNumBytesLeft());
	return 1;
}

LUA_FUNCTION_STATIC(bf_write_GetNumBitsWritten)
{
	bf_write* pBF = Get_bf_write(LUA, 1, true);

	LUA->PushNumber(pBF->GetNumBitsWritten());
	return 1;
}

LUA_FUNCTION_STATIC(bf_write_GetNumBitsLeft)
{
	bf_write* pBF = Get_bf_write(LUA, 1, true);

	LUA->PushNumber(pBF->GetNumBitsLeft());
	return 1;
}

LUA_FUNCTION_STATIC(bf_write_GetMaxNumBits)
{
	bf_write* pBF = Get_bf_write(LUA, 1, true);

	LUA->PushNumber(pBF->GetMaxNumBits());
	return 1;
}

LUA_FUNCTION_STATIC(bf_write_IsOverflowed)
{
	bf_write* pBF = Get_bf_write(LUA, 1, true);

	LUA->PushBool(pBF->IsOverflowed());
	return 1;
}

LUA_FUNCTION_STATIC(bf_write_Reset)
{
	bf_write* pBF = Get_bf_write(LUA, 1, true);

	pBF->Reset();
	return 0;
}

LUA_FUNCTION_STATIC(bf_write_GetDebugName)
{
	bf_write* pBF = Get_bf_write(LUA, 1, true);

	LUA->PushString(pBF->GetDebugName());
	return 1;
}

LUA_FUNCTION_STATIC(bf_write_SetDebugName)
{
	bf_write* pBF = Get_bf_write(LUA, 1, true);

	pBF->SetDebugName(LUA->CheckString(2)); // BUG: Do we need to keep a reference?
	return 0;
}

LUA_FUNCTION_STATIC(bf_write_SeekToBit)
{
	bf_write* pBF = Get_bf_write(LUA, 1, true);

	pBF->SeekToBit((int)LUA->CheckNumber(2));
	return 0;
}

LUA_FUNCTION_STATIC(bf_write_WriteFloat)
{
	bf_write* pBF = Get_bf_write(LUA, 1, true);

	pBF->WriteFloat((float)LUA->CheckNumber(2));
	return 0;
}

LUA_FUNCTION_STATIC(bf_write_WriteChar)
{
	bf_write* pBF = Get_bf_write(LUA, 1, true);

	pBF->WriteChar((int)LUA->CheckNumber(2));
	return 0;
}

LUA_FUNCTION_STATIC(bf_write_WriteByte)
{
	bf_write* pBF = Get_bf_write(LUA, 1, true);

	pBF->WriteByte((int)LUA->CheckNumber(2));
	return 0;
}

LUA_FUNCTION_STATIC(bf_write_WriteLong)
{
	bf_write* pBF = Get_bf_write(LUA, 1, true);

	pBF->WriteLong((long)LUA->CheckNumber(2));
	return 0;
}

LUA_FUNCTION_STATIC(bf_write_WriteLongLong)
{
	bf_write* pBF = Get_bf_write(LUA, 1, true);

	if (LUA->IsType(2, GarrysMod::Lua::Type::String))
	{
		pBF->WriteLongLong(strtoull(LUA->GetString(2), NULL, 0));
	} else {
		pBF->WriteLongLong((int64)LUA->CheckNumber(2));
	}
	return 0;
}

LUA_FUNCTION_STATIC(bf_write_WriteBytes)
{
	bf_write* pBF = Get_bf_write(LUA, 1, true);

	const char* pData = LUA->CheckString(2);
	int iLength = LUA->ObjLen(2);
	pBF->WriteBytes(pData, iLength);
	return 0;
}

LUA_FUNCTION_STATIC(bf_write_WriteOneBit)
{
	bf_write* pBF = Get_bf_write(LUA, 1, true);

	pBF->WriteOneBit(LUA->GetBool(2));
	return 0;
}

LUA_FUNCTION_STATIC(bf_write_WriteOneBitAt)
{
	bf_write* pBF = Get_bf_write(LUA, 1, true);

	pBF->WriteOneBitAt((int)LUA->CheckNumber(2), LUA->GetBool(3));
	return 0;
}

LUA_FUNCTION_STATIC(bf_write_WriteShort)
{
	bf_write* pBF = Get_bf_write(LUA, 1, true);

	pBF->WriteShort((int)LUA->CheckNumber(2));
	return 0;
}

LUA_FUNCTION_STATIC(bf_write_WriteWord)
{
	bf_write* pBF = Get_bf_write(LUA, 1, true);

	pBF->WriteWord((int)LUA->CheckNumber(2));
	return 0;
}

LUA_FUNCTION_STATIC(bf_write_WriteSignedVarInt32)
{
	bf_write* pBF = Get_bf_write(LUA, 1, true);

	pBF->WriteSignedVarInt32((int32)LUA->CheckNumber(2));
	return 0;
}

LUA_FUNCTION_STATIC(bf_write_WriteSignedVarInt64)
{
	bf_write* pBF = Get_bf_write(LUA, 1, true);

	pBF->WriteSignedVarInt64((int64)LUA->CheckNumber(2));
	return 0;
}

LUA_FUNCTION_STATIC(bf_write_WriteVarInt32)
{
	bf_write* pBF = Get_bf_write(LUA, 1, true);

	pBF->WriteVarInt32((uint32)LUA->CheckNumber(2));
	return 0;
}

LUA_FUNCTION_STATIC(bf_write_WriteVarInt64)
{
	bf_write* pBF = Get_bf_write(LUA, 1, true);

	pBF->WriteVarInt64((uint64)LUA->CheckNumber(2));
	return 0;
}

LUA_FUNCTION_STATIC(bf_write_WriteUBitVar)
{
	bf_write* pBF = Get_bf_write(LUA, 1, true);

	pBF->WriteUBitVar((unsigned int)LUA->CheckNumber(2));
	return 0;
}

LUA_FUNCTION_STATIC(bf_write_WriteUBitLong)
{
	bf_write* pBF = Get_bf_write(LUA, 1, true);

	pBF->WriteUBitLong((unsigned int)LUA->CheckNumber(2), (int)LUA->CheckNumber(3), LUA->GetBool(4));
	return 0;
}

LUA_FUNCTION_STATIC(bf_write_WriteBitAngle)
{
	bf_write* pBF = Get_bf_write(LUA, 1, true);

	pBF->WriteBitAngle((float)LUA->CheckNumber(2), (int)LUA->CheckNumber(3));
	return 0;
}

LUA_FUNCTION_STATIC(bf_write_WriteBitAngles)
{
	bf_write* pBF = Get_bf_write(LUA, 1, true);

	QAngle* ang = Get_QAngle(LUA, 2);
	pBF->WriteBitAngles(*ang);
	return 0;
}

LUA_FUNCTION_STATIC(bf_write_WriteBitVec3Coord)
{
	bf_write* pBF = Get_bf_write(LUA, 1, true);

	Vector* vec = Get_Vector(LUA, 2);
	pBF->WriteBitVec3Coord(*vec);
	return 0;
}

LUA_FUNCTION_STATIC(bf_write_WriteBitVec3Normal)
{
	bf_write* pBF = Get_bf_write(LUA, 1, true);

	Vector* vec = Get_Vector(LUA, 2);
	pBF->WriteBitVec3Normal(*vec);
	return 0;
}

LUA_FUNCTION_STATIC(bf_write_WriteBits)
{
	bf_write* pBF = Get_bf_write(LUA, 1, true);

	const char* pData = LUA->CheckString(2);
	int iBits = (int)LUA->CheckNumber(3);
	LUA->PushBool(pBF->WriteBits(pData, iBits));
	return 1;
}

LUA_FUNCTION_STATIC(bf_write_WriteBitsFromBuffer)
{
	bf_write* pBF = Get_bf_write(LUA, 1, true);
	bf_read* pBFRead = Get_bf_read(LUA, 2, true);

	int iBits = (int)LUA->CheckNumber(3);
	LUA->PushBool(pBF->WriteBitsFromBuffer(pBFRead, iBits));
	return 1;
}

LUA_FUNCTION_STATIC(bf_write_WriteBitNormal)
{
	bf_write* pBF = Get_bf_write(LUA, 1, true);

	pBF->WriteBitNormal((float)LUA->CheckNumber(2));
	return 0;
}

LUA_FUNCTION_STATIC(bf_write_WriteBitLong)
{
	bf_write* pBF = Get_bf_write(LUA, 1, true);

	pBF->WriteBitLong((unsigned int)LUA->CheckNumber(2), (int)LUA->CheckNumber(3), LUA->GetBool(4));
	return 0;
}

LUA_FUNCTION_STATIC(bf_write_WriteBitFloat)
{
	bf_write* pBF = Get_bf_write(LUA, 1, true);

	pBF->WriteBitFloat((float)LUA->CheckNumber(2));
	return 0;
}

LUA_FUNCTION_STATIC(bf_write_WriteBitCoord)
{
	bf_write* pBF = Get_bf_write(LUA, 1, true);

	pBF->WriteBitCoord((float)LUA->CheckNumber(2));
	return 0;
}

LUA_FUNCTION_STATIC(bf_write_WriteBitCoordMP)
{
	bf_write* pBF = Get_bf_write(LUA, 1, true);

	bool bIntegral = LUA->GetBool(2);
	bool bLowPrecision = LUA->GetBool(3);
#if ARCHITECTURE_IS_X86_64
	EBitCoordType pType = kCW_None;
	if (bIntegral)
		pType = EBitCoordType::kCW_Integral;

	if (bLowPrecision)
		pType = EBitCoordType::kCW_LowPrecision;

	pBF->WriteBitCoordMP((float)LUA->CheckNumber(2), pType);
#else
	pBF->WriteBitCoordMP((float)LUA->CheckNumber(2), bIntegral, bLowPrecision);
#endif
	return 0;
}

LUA_FUNCTION_STATIC(bf_write_WriteString)
{
	bf_write* pBF = Get_bf_write(LUA, 1, true);

	pBF->WriteString(LUA->CheckString(2));
	return 0;
}

static constexpr int MAX_BUFFER_SIZE = 1 << 18;
static constexpr int MIN_BUFFER_SIZE = 4;
#define CLAMP_BF(val) MAX(MIN(val + 1, MAX_BUFFER_SIZE), MIN_BUFFER_SIZE)

LUA_FUNCTION_STATIC(bitbuf_CopyReadBuffer)
{
	bf_read* pBf = Get_bf_read(LUA, 1, true);

	int iSize = pBf->GetNumBytesRead() + pBf->GetNumBytesLeft();
	int iNewSize = CLAMP_BF(iSize);

	unsigned char* pData = new unsigned char[iNewSize];
	memcpy(pData, pBf->GetBasePointer(), iSize);

	bf_read* pNewBf = new bf_read;
	pNewBf->StartReading(pData, iNewSize);

	Push_bf_read(LUA, pNewBf);

	return 1;
}

LUA_FUNCTION_STATIC(bitbuf_CreateReadBuffer)
{
	const char* pData = LUA->CheckString(1);
	int iLength = LUA->ObjLen(1);
	int iNewLength = CLAMP_BF(iLength);

	unsigned char* cData = new unsigned char[iNewLength];
	memcpy(cData, pData, iLength);

	bf_read* pNewBf = new bf_read;
	pNewBf->StartReading(cData, iNewLength);

	Push_bf_read(LUA, pNewBf);

	return 1;
}

LUA_FUNCTION_STATIC(bitbuf_CreateWriteBuffer)
{
	if (LUA->IsType(1, GarrysMod::Lua::Type::Number))
	{
		int iSize = CLAMP_BF((int)LUA->CheckNumber(1));
		unsigned char* cData = new unsigned char[iSize];

		bf_write* pNewBf = new bf_write;
		pNewBf->StartWriting(cData, iSize);

		Push_bf_write(LUA, pNewBf);
	} else {
		const char* pData = LUA->CheckString(1);
		int iLength = LUA->ObjLen(1);
		int iNewLength = CLAMP_BF(iLength);

		unsigned char* cData = new unsigned char[iNewLength];
		memcpy(cData, pData, iLength);

		bf_write* pNewBf = new bf_write;
		pNewBf->StartWriting(cData, iNewLength);

		Push_bf_write(LUA, pNewBf);
	}

	return 1;
}

void CBitBufModule::LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit)
{
	if (bServerInit)
		return;

	Lua::GetLuaData(pLua)->RegisterMetaTable(Lua::bf_read, pLua->CreateMetaTable("bf_read"));
		Util::AddFunc(pLua, bf_read__tostring, "__tostring");
		Util::AddFunc(pLua, bf_read__index, "__index");
		Util::AddFunc(pLua, bf_read__newindex, "__newindex");
		Util::AddFunc(pLua, bf_read__gc, "__gc");
		Util::AddFunc(pLua, bf_read_GetTable, "GetTable");
		Util::AddFunc(pLua, bf_read_IsValid, "IsValid");
		Util::AddFunc(pLua, bf_read_GetNumBitsLeft, "GetNumBitsLeft");
		Util::AddFunc(pLua, bf_read_GetNumBitsRead, "GetNumBitsRead");
		Util::AddFunc(pLua, bf_read_GetNumBits, "GetNumBits");
		Util::AddFunc(pLua, bf_read_GetNumBytesLeft, "GetNumBytesLeft");
		Util::AddFunc(pLua, bf_read_GetNumBytesRead, "GetNumBytesRead");
		Util::AddFunc(pLua, bf_read_GetNumBytes, "GetNumBytes");
		Util::AddFunc(pLua, bf_read_GetCurrentBit, "GetCurrentBit");
		Util::AddFunc(pLua, bf_read_IsOverflowed, "IsOverflowed");
		Util::AddFunc(pLua, bf_read_PeekUBitLong, "PeekUBitLong");
		Util::AddFunc(pLua, bf_read_ReadBitAngle, "ReadBitAngle");
		Util::AddFunc(pLua, bf_read_ReadBitAngles, "ReadBitAngles");
		Util::AddFunc(pLua, bf_read_ReadBitCoord, "ReadBitCoord");
		Util::AddFunc(pLua, bf_read_ReadBitCoordBits, "ReadBitCoordBits");
		Util::AddFunc(pLua, bf_read_ReadBitCoordMP, "ReadBitCoordMP");
		Util::AddFunc(pLua, bf_read_ReadBitCoordMPBits, "ReadBitCoordMPBits");
		Util::AddFunc(pLua, bf_read_ReadBitFloat, "ReadBitFloat");
		Util::AddFunc(pLua, bf_read_ReadBitLong, "ReadBitLong");
		Util::AddFunc(pLua, bf_read_ReadBitNormal, "ReadBitNormal");
		Util::AddFunc(pLua, bf_read_ReadBits, "ReadBits");
		Util::AddFunc(pLua, bf_read_ReadBitVec3Coord, "ReadBitVec3Coord");
		Util::AddFunc(pLua, bf_read_ReadBitVec3Normal, "ReadBitVec3Normal");
		Util::AddFunc(pLua, bf_read_ReadByte, "ReadByte");
		Util::AddFunc(pLua, bf_read_ReadBytes, "ReadBytes");
		Util::AddFunc(pLua, bf_read_ReadChar, "ReadChar");
		Util::AddFunc(pLua, bf_read_ReadFloat, "ReadFloat");
		Util::AddFunc(pLua, bf_read_ReadLong, "ReadLong");
		Util::AddFunc(pLua, bf_read_ReadLongLong, "ReadLongLong");
		Util::AddFunc(pLua, bf_read_ReadOneBit, "ReadOneBit");
		Util::AddFunc(pLua, bf_read_ReadSBitLong, "ReadSBitLong");
		Util::AddFunc(pLua, bf_read_ReadShort, "ReadShort");
		Util::AddFunc(pLua, bf_read_ReadSignedVarInt32, "ReadSignedVarInt32");
		Util::AddFunc(pLua, bf_read_ReadSignedVarInt64, "ReadSignedVarInt64");
		Util::AddFunc(pLua, bf_read_ReadString, "ReadString");
		Util::AddFunc(pLua, bf_read_ReadUBitLong, "ReadUBitLong");
		Util::AddFunc(pLua, bf_read_ReadUBitVar, "ReadUBitVar");
		Util::AddFunc(pLua, bf_read_ReadVarInt32, "ReadVarInt32");
		Util::AddFunc(pLua, bf_read_ReadVarInt64, "ReadVarInt64");
		Util::AddFunc(pLua, bf_read_ReadWord, "ReadWord");

		// Functions to change the current bit
		Util::AddFunc(pLua, bf_read_Reset, "Reset");
		Util::AddFunc(pLua, bf_read_Seek, "Seek");
		Util::AddFunc(pLua, bf_read_SeekRelative, "SeekRelative");

		// Other functions
		Util::AddFunc(pLua, bf_read_GetData, "GetData");
	pLua->Pop(1);

	Lua::GetLuaData(pLua)->RegisterMetaTable(Lua::bf_write, pLua->CreateMetaTable("bf_write"));
		Util::AddFunc(pLua, bf_write__tostring, "__tostring");
		Util::AddFunc(pLua, bf_write__index, "__index");
		Util::AddFunc(pLua, bf_write__newindex, "__newindex");
		Util::AddFunc(pLua, bf_write__gc, "__gc");
		Util::AddFunc(pLua, bf_write_GetTable, "GetTable");
		Util::AddFunc(pLua, bf_write_IsValid, "IsValid");
		Util::AddFunc(pLua, bf_write_GetData, "GetData");
		Util::AddFunc(pLua, bf_write_GetNumBytesWritten, "GetNumBytesWritten");
		Util::AddFunc(pLua, bf_write_GetNumBytesLeft, "GetNumBytesLeft");
		Util::AddFunc(pLua, bf_write_GetNumBitsWritten, "GetNumBitsWritten");
		Util::AddFunc(pLua, bf_write_GetNumBitsLeft, "GetNumBitsLeft");
		Util::AddFunc(pLua, bf_write_GetMaxNumBits, "GetMaxNumBits");
		Util::AddFunc(pLua, bf_write_IsOverflowed, "IsOverflowed");
		Util::AddFunc(pLua, bf_write_Reset, "Reset");
		Util::AddFunc(pLua, bf_write_GetDebugName, "GetDebugName");
		Util::AddFunc(pLua, bf_write_SetDebugName, "SetDebugName");
		Util::AddFunc(pLua, bf_write_SeekToBit, "SeekToBit");
		Util::AddFunc(pLua, bf_write_WriteFloat, "WriteFloat");
		Util::AddFunc(pLua, bf_write_WriteChar, "WriteChar");
		Util::AddFunc(pLua, bf_write_WriteByte, "WriteByte");
		Util::AddFunc(pLua, bf_write_WriteLong, "WriteLong");
		Util::AddFunc(pLua, bf_write_WriteLongLong, "WriteLongLong");
		Util::AddFunc(pLua, bf_write_WriteBytes, "WriteBytes");
		Util::AddFunc(pLua, bf_write_WriteOneBit, "WriteOneBit");
		Util::AddFunc(pLua, bf_write_WriteOneBitAt, "WriteOneBitAt");
		Util::AddFunc(pLua, bf_write_WriteShort, "WriteShort");
		Util::AddFunc(pLua, bf_write_WriteWord, "WriteWord");
		Util::AddFunc(pLua, bf_write_WriteSignedVarInt32, "WriteSignedVarInt32");
		Util::AddFunc(pLua, bf_write_WriteSignedVarInt64, "WriteSignedVarInt64");
		Util::AddFunc(pLua, bf_write_WriteVarInt32, "WriteVarInt32");
		Util::AddFunc(pLua, bf_write_WriteVarInt64, "WriteVarInt64");
		Util::AddFunc(pLua, bf_write_WriteUBitVar, "WriteUBitVar");
		Util::AddFunc(pLua, bf_write_WriteUBitLong, "WriteUBitLong");
		Util::AddFunc(pLua, bf_write_WriteBitAngle, "WriteBitAngle");
		Util::AddFunc(pLua, bf_write_WriteBitAngles, "WriteBitAngles");
		Util::AddFunc(pLua, bf_write_WriteBitVec3Coord, "WriteBitVec3Coord");
		Util::AddFunc(pLua, bf_write_WriteBitVec3Normal, "WriteBitVec3normal");
		Util::AddFunc(pLua, bf_write_WriteBits, "WriteBits");
		Util::AddFunc(pLua, bf_write_WriteBitsFromBuffer, "WriteBitsFromBuffer");
		Util::AddFunc(pLua, bf_write_WriteBitNormal, "WriteBitNormal");
		Util::AddFunc(pLua, bf_write_WriteBitLong, "WriteBitLong");
		Util::AddFunc(pLua, bf_write_WriteBitFloat, "WriteBitFloat");
		Util::AddFunc(pLua, bf_write_WriteBitCoord, "WriteBitCoord");
		Util::AddFunc(pLua, bf_write_WriteBitCoordMP, "WriteBitCoordMP");
		Util::AddFunc(pLua, bf_write_WriteString, "WriteString");
	pLua->Pop(1);

	Util::StartTable(pLua);
		Util::AddFunc(pLua, bitbuf_CopyReadBuffer, "CopyReadBuffer");
		Util::AddFunc(pLua, bitbuf_CreateReadBuffer, "CreateReadBuffer");
		Util::AddFunc(pLua, bitbuf_CreateWriteBuffer, "CreateWriteBuffer");
	Util::FinishTable(pLua, "bitbuf");
}

void CBitBufModule::LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua)
{
	Util::NukeTable(pLua, "bitbuf");
}