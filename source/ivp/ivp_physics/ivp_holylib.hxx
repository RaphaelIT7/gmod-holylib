#ifndef _IVP_HOLYLIB
#define _IVP_HOLYLIB

class IVP_Real_Object;
class IPhysicsEnvironment;
class IPhysicsObject;

class IVP_HolyLib_Callbacks
{
public:
	virtual ~IVP_HolyLib_Callbacks() {};

	// Returns true if the simulation should be canceled. Let's try to safely stop it.
	virtual bool CheckLag(IPhysicsObject* pObject1 = nullptr, IPhysicsObject* pObject2 = nullptr) = 0;

	virtual void OnEnvironmentCreated(IPhysicsEnvironment* pEnvironment) = 0;
	virtual void OnEnvironmentDestroyed(IPhysicsEnvironment* pEnvironment) = 0;

	//virtual void OnPhysicsObjectCreated(IPhysicsObject* pObject) = 0;
	//virtual void OnPhysicsObjectDestroyed(IPhysicsObject* pObject) = 0;

	inline bool ShouldSkip() { return m_bSkipSimulation; };
	inline void SetShouldSkip(bool bSkipSimulation) { m_bSkipSimulation = bSkipSimulation; };
private:
	bool m_bSkipSimulation = false;
};

extern IVP_HolyLib_Callbacks* g_pHolyLibCallbacks;
extern void SetIVPHolyLibCallbacks(IVP_HolyLib_Callbacks* pCallback);

#endif