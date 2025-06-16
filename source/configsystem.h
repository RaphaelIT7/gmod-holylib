#include "public/iconfigsystem.h"

class CConfigSystem : public IConfigSystem
{
public:
	virtual IConfig* LoadConfig(const char* pFilePath);
};

class CConfig : public IConfig
{
public:
	CConfig(const char* pFileName);
	virtual ConfigState GetState();
	virtual Bootil::Data::Tree& GetData();
	virtual bool Save();
	virtual bool Load();
	virtual void Destroy();

private:
	std::string m_strFileName = "";
	Bootil::Data::Tree m_pData;
	ConfigState m_pState = ConfigState::NOT_LOADED;

	friend class CConfigSystem;
};