#include "public/iconfigsystem.h"

class CConfigSystem : public IConfigSystem
{
public:
	virtual IConfig* LoadConfig(const char* pFilePath);
};

class CConfig : public IConfig
{
public:
	virtual bool FreshConfig();
	virtual bool IsInvalid();
	virtual Bootil::Data::Tree& GetData();
	virtual bool Save();
	virtual void Destroy();

private:
	std::string m_strFileName = "";
	Bootil::Data::Tree m_pData;
	bool m_bFreshlyCreated = false;
	bool m_bIsInvalid = false;

	friend class CConfigSystem;
};