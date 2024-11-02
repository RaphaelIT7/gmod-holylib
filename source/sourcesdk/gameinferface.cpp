#include "usermessages.h"
#include "util.h" // <---- HolyLib include!
#include <player.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static bf_write* g_pMsgBuffer = NULL;

bf_write* GetActiveMessage()
{
	return g_pMsgBuffer;
}

void EntityMessageBegin(CBaseEntity* entity, bool reliable /*= false*/)
{
	Assert(!g_pMsgBuffer);

	Assert(entity);

	g_pMsgBuffer = Util::engineserver->EntityMessageBegin(entity->entindex(), entity->GetServerClass(), reliable);
}

void UserMessageBegin(IRecipientFilter& filter, const char* messagename)
{
	Assert(!g_pMsgBuffer);

	Assert(messagename);

	int msg_type = Util::pUserMessages->LookupUserMessage(messagename);

	if (msg_type == -1)
	{
		Error("UserMessageBegin:  Unregistered message '%s'\n", messagename);
	}

	g_pMsgBuffer = Util::engineserver->UserMessageBegin(&filter, msg_type);
}

void MessageEnd(void)
{
	Assert(g_pMsgBuffer);

	Util::engineserver->MessageEnd();

	g_pMsgBuffer = NULL;
}

void MessageWriteByte(int iValue)
{
	if (!g_pMsgBuffer)
		Error("WRITE_BYTE called with no active message\n");

	g_pMsgBuffer->WriteByte(iValue);
}

void MessageWriteChar(int iValue)
{
	if (!g_pMsgBuffer)
		Error("WRITE_CHAR called with no active message\n");

	g_pMsgBuffer->WriteChar(iValue);
}

void MessageWriteShort(int iValue)
{
	if (!g_pMsgBuffer)
		Error("WRITE_SHORT called with no active message\n");

	g_pMsgBuffer->WriteShort(iValue);
}

void MessageWriteWord(int iValue)
{
	if (!g_pMsgBuffer)
		Error("WRITE_WORD called with no active message\n");

	g_pMsgBuffer->WriteWord(iValue);
}

void MessageWriteLong(int iValue)
{
	if (!g_pMsgBuffer)
		Error("WriteLong called with no active message\n");

	g_pMsgBuffer->WriteLong(iValue);
}

void MessageWriteFloat(float flValue)
{
	if (!g_pMsgBuffer)
		Error("WriteFloat called with no active message\n");

	g_pMsgBuffer->WriteFloat(flValue);
}

void MessageWriteAngle(float flValue)
{
	if (!g_pMsgBuffer)
		Error("WriteAngle called with no active message\n");

	g_pMsgBuffer->WriteBitAngle(flValue, 8);
}

void MessageWriteCoord(float flValue)
{
	if (!g_pMsgBuffer)
		Error("WriteCoord called with no active message\n");

	g_pMsgBuffer->WriteBitCoord(flValue);
}

void MessageWriteVec3Coord(const Vector& rgflValue)
{
	if (!g_pMsgBuffer)
		Error("WriteVec3Coord called with no active message\n");

	g_pMsgBuffer->WriteBitVec3Coord(rgflValue);
}

void MessageWriteVec3Normal(const Vector& rgflValue)
{
	if (!g_pMsgBuffer)
		Error("WriteVec3Normal called with no active message\n");

	g_pMsgBuffer->WriteBitVec3Normal(rgflValue);
}

void MessageWriteAngles(const QAngle& rgflValue)
{
	if (!g_pMsgBuffer)
		Error("WriteVec3Normal called with no active message\n");

	g_pMsgBuffer->WriteBitAngles(rgflValue);
}

void MessageWriteString(const char* sz)
{
	if (!g_pMsgBuffer)
		Error("WriteString called with no active message\n");

	g_pMsgBuffer->WriteString(sz);
}

void MessageWriteEntity(int iValue)
{
	if (!g_pMsgBuffer)
		Error("WriteEntity called with no active message\n");

	g_pMsgBuffer->WriteShort(iValue);
}

void MessageWriteEHandle(CBaseEntity* pEntity)
{
	if (!g_pMsgBuffer)
		Error("WriteEHandle called with no active message\n");

	long iEncodedEHandle;

	if (pEntity)
	{
		EHANDLE hEnt = pEntity;

		int iSerialNum = hEnt.GetSerialNumber() & ((1 << NUM_NETWORKED_EHANDLE_SERIAL_NUMBER_BITS) - 1);
		iEncodedEHandle = hEnt.GetEntryIndex() | (iSerialNum << MAX_EDICT_BITS);
	}
	else
	{
		iEncodedEHandle = INVALID_NETWORKED_EHANDLE_VALUE;
	}

	g_pMsgBuffer->WriteLong(iEncodedEHandle);
}

// bitwise
void MessageWriteBool(bool bValue)
{
	if (!g_pMsgBuffer)
		Error("WriteBool called with no active message\n");

	g_pMsgBuffer->WriteOneBit(bValue ? 1 : 0);
}

void MessageWriteUBitLong(unsigned int data, int numbits)
{
	if (!g_pMsgBuffer)
		Error("WriteUBitLong called with no active message\n");

	g_pMsgBuffer->WriteUBitLong(data, numbits);
}

void MessageWriteSBitLong(int data, int numbits)
{
	if (!g_pMsgBuffer)
		Error("WriteSBitLong called with no active message\n");

	g_pMsgBuffer->WriteSBitLong(data, numbits);
}

void MessageWriteBits(const void* pIn, int nBits)
{
	if (!g_pMsgBuffer)
		Error("WriteBits called with no active message\n");

	g_pMsgBuffer->WriteBits(pIn, nBits);
}