#ifndef _IVP_HOLYLIB
#define _IVP_HOLYLIB

class IVP_Real_Object;
class IPhysicsEnvironment;

class IVP_HolyLib_Callbacks
{
public:
	virtual ~IVP_HolyLib_Callbacks() {};

	// Returns true if the simulation should be canceled. Let's try to safely stop it.
	virtual bool CheckLag(const char* pName, void* pObject1 = nullptr, void* pObject2 = nullptr) = 0;

	virtual void OnEnvironmentCreated(IPhysicsEnvironment* pEnvironment) = 0;
	virtual void OnEnvironmentDestroyed(IPhysicsEnvironment* pEnvironment) = 0;

	//virtual void OnPhysicsObjectCreated(IPhysicsObject* pObject) = 0;
	//virtual void OnPhysicsObjectDestroyed(IPhysicsObject* pObject) = 0;

	virtual void SimulationBegin() = 0;
	virtual void SimulationFinish() = 0;

	virtual bool ShouldSkip() = 0;

	virtual void ThrowRecheckOVWarning() = 0;
};

extern IVP_HolyLib_Callbacks* g_pHolyLibCallbacks;
extern void SetIVPHolyLibCallbacks(IVP_HolyLib_Callbacks* pCallback);

#endif