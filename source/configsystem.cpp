#include "Bootil/Bootil.h"
#include "configsystem.h"

static CConfigSystem pConfigSystem;
IConfigSystem* g_pConfigSystem = &pConfigSystem;

// Expose it :3
EXPOSE_INTERFACE(CConfigSystem, IConfigSystem, INTERFACEVERSION_CONFIG);

// We don't pass by reference since StripFilename modifies it.
void CreateFolders(Bootil::BString filePath)
{
	Bootil::String::File::StripFilename(filePath);
	Bootil::File::CreateFolder(filePath);
}

IConfig* CConfigSystem::LoadConfig(const char* pFilePath)
{
	CConfig* pConfig = new CConfig;
	pConfig->m_strFileName = pFilePath;
	pConfig->m_bFreshlyCreated = !Bootil::File::Exists(pConfig->m_strFileName);
	CreateFolders(pConfig->m_strFileName);

	if (!pConfig->m_bFreshlyCreated)
	{
		Bootil::BString pData;
		if (Bootil::File::Read(pConfig->m_strFileName, pData))
		{
			if (!Bootil::Data::Json::Import(pConfig->m_pData, pData.c_str()))
			{
				// Why don't we just reset it?
				// Because in this case the json may be invalid
				// so the user's changes probably broke it.
				pConfig->m_bIsInvalid = true;
			}
		} else {
			pConfig->m_bFreshlyCreated = true; // We failed to read it? Reset it!
		}
	}

	return pConfig;
}

bool CConfig::FreshConfig()
{
	return m_bFreshlyCreated;
}

bool CConfig::IsInvalid()
{
	return m_bIsInvalid;
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
		m_bIsInvalid = true;
		return false; // Invalid json!
	}

	// Trim or else we might have a new line in our file for some reason
	Bootil::String::Util::Trim(pData);
	return Bootil::File::Write(m_strFileName, pData);
}

void CConfig::Destroy()
{
	delete (CConfig*)this;
}