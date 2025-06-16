#include "Bootil/Bootil.h"
#include "configsystem.h"

static CConfigSystem pConfigSystem;
IConfigSystem* g_pConfigSystem = &pConfigSystem;

// Expose it :3
EXPOSE_INTERFACE(CConfigSystem, IConfigSystem, INTERFACEVERSION_CONFIG);

IConfig* CConfigSystem::LoadConfig(const char* pFilePath)
{
	return new CConfig(pFilePath);
}

CConfig::CConfig(const char* pFileName)
{
	m_strFileName = pFileName;
	Load();
}

ConfigState CConfig::GetState()
{
	return m_pState;
}

Bootil::Data::Tree& CConfig::GetData()
{
	return m_pData;
}

bool CConfig::Save()
{
	Bootil::BString pData;
	if (!Bootil::Data::Json::Export(m_pData, pData, true))
	{
		m_pState = INVALID_JSON;
		return false; // Invalid json!
	}

	// Trim or else we might have a new line in our file for some reason
	Bootil::String::Util::Trim(pData);
	return Bootil::File::Write(m_strFileName, pData);
}

bool CConfig::Load()
{
	m_pData.Clear();

	Bootil::File::CreateFilePath(m_strFileName);
	Bootil::BString pData;
	if (Bootil::File::Read(m_strFileName, pData))
	{
		if (!Bootil::Data::Json::Import(m_pData, pData.c_str()))
		{
			// Why don't we just reset it?
			// Because in this case the json may be invalid
			// so the user's changes probably broke it.
			// And we don't want to just reset it and nuke their changes.
			m_pState = INVALID_JSON;
			return false;
		}
	} else {
		m_pState = ConfigState::FRESH; // We failed to read it? Reset it!
		return false;
	}

	m_pState = ConfigState::OK;
	return true;
}

void CConfig::Destroy()
{
	delete (CConfig*)this;
}