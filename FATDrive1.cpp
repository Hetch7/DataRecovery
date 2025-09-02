#include "stdafx.h"
#include ".\fatdrive.h"

CFATDrive::CFATDrive(void)
{
}

CFATDrive::~CFATDrive(void)
{
	if(m_puchFAT)
		delete[] m_puchFAT;
	if(m_puchRootDir)
		delete[] m_puchRootDir;
}

void CFATDrive::SetDrive(HANDLE hDev, BYTE bDrive, CListCtrl *pList)
{
	m_hDrive = hDev;
	m_bDriveNo = bDrive;
	m_pList = pList;
}

void CFATDrive::LoadRootDir()
{
	if(ReadLogicalSectors(0, 1, (LPBYTE)&m_BPBlock))
		if(m_BPBlock.fat32.sectorsPerFAT == 0 && !memcmp(m_BPBlock.fat32.fileSystemType,"FAT32",5))
		{
//			MessageBox(NULL, _T("‚±‚ê‚ÍFAT32‚Å‚·!"), _T("DataRecovery"), MB_OK);
		}
		else
			return;
	else
	{
		MessageBox(NULL, _T("BPB‚ð“Ç‚Ýž‚ß‚Ü‚¹‚ñ‚Å‚µ‚½"), _T("DataRecovery"), MB_OK);
		return;
	}

	// save some parameters
	memcpy(&m_bytesPerSector, m_BPBlock.fat32.bytesPerSector,sizeof(BYTE)*2);
	m_sectorsPerCluster = m_BPBlock.fat32.sectorsPerCluster;
	m_numberOfFATs = m_BPBlock.fat32.numberOfFATs;
	memcpy(&m_reservedSectors, m_BPBlock.fat32.reservedSectors,sizeof(BYTE)*2);
	memcpy(&m_bigSectorsPerFAT, m_BPBlock.fat32.bigSectorsPerFAT,sizeof(BYTE)*4);
	memcpy(&m_rootDirStrtClus, m_BPBlock.fat32.rootDirStrtClus,sizeof(BYTE)*4);

	// caluculate basic parameters
	m_dwBytePerCluster = m_bytesPerSector * m_sectorsPerCluster;
	m_dwUserDataOffset = m_reservedSectors + m_numberOfFATs * m_bigSectorsPerFAT;

	// load FAT
	m_puchFAT = new BYTE[m_bytesPerSector * m_bigSectorsPerFAT];
	if(!ReadLogicalSectors(m_reservedSectors, m_bigSectorsPerFAT, m_puchFAT)) 
	{
		MessageBox(NULL, _T("FAT‚ð“Ç‚Ýž‚ß‚Ü‚¹‚ñ‚Å‚µ‚½"), _T("DataRecovery"), MB_OK);
		return;
	}

	// caluculate Root Directory Size
	DWORD dwClusterNo;
	m_dwRootDirSize = m_dwBytePerCluster;
	DWORD dwCurPos = sizeof(BYTE) * 4 * m_rootDirStrtClus;
	for(;;)
	{
		memcpy(&dwClusterNo,&m_puchFAT[dwCurPos],sizeof(BYTE)*4);
		if(dwClusterNo >= 0x00000002 && dwClusterNo <= 0x0ffffff6) //cluster chain
		{
			m_dwRootDirSize =+ m_dwBytePerCluster;
			dwCurPos = sizeof(BYTE) * 4 * dwClusterNo;
		}
		else if(dwClusterNo >= 0x0ffffff8 && dwClusterNo <= 0x0fffffff)
			break;
	}
	m_puchRootDir = new BYTE[m_dwRootDirSize];

	// load Root Directory
	DWORD dwCurPosRD = 0;
	dwClusterNo = m_rootDirStrtClus;
	dwCurPos = sizeof(BYTE) * 4 * m_rootDirStrtClus;
	for(;;)
	{
		if(!ReadLogicalSectors((dwClusterNo-2)*m_sectorsPerCluster+m_dwUserDataOffset, m_sectorsPerCluster, &m_puchRootDir[dwCurPosRD])) 
		{
			MessageBox(NULL, _T("FAT‚ð“Ç‚Ýž‚ß‚Ü‚¹‚ñ‚Å‚µ‚½"), _T("DataRecovery"), MB_OK);
			break;
		}
		dwCurPosRD += m_dwBytePerCluster;
		memcpy(&dwClusterNo,&m_puchFAT[dwCurPos],sizeof(BYTE)*4);
		if(dwClusterNo >= 0x00000002 && dwClusterNo <= 0x0ffffff6) //cluster chain
		{
			dwCurPos = sizeof(BYTE) * 4 * dwClusterNo;
		}
		else if(dwClusterNo >= 0x0ffffff8 && dwClusterNo <= 0x0fffffff)
			break;
	}

	dwCurPos = 0;
	DWORD dwDeleted = 0;
	TCHAR name1st;
	CString cstrFileName;
	static TCHAR szFileName[255] = "";
	for(;;)
	{
		memcpy(&m_DirEntry,&m_puchRootDir[dwCurPos],sizeof(m_DirEntry));
		if(m_DirEntry.name[0] == 0) //No more entry
			break;

		if(m_DirEntry.name[0] == 0xe5) // this is it!
		{
			if(m_DirEntry.attribute == 0x0f) //LFN ext. so get first char of Filename
			{
				memcpy(&m_LNFEntry,&m_DirEntry,sizeof(m_DirEntry));
				memcpy(szFileName,m_LNFEntry.name1st,sizeof(BYTE)*10);
				memcpy(&szFileName[10],m_LNFEntry.name2nd,sizeof(BYTE)*12);
				memcpy(&szFileName[22],m_LNFEntry.name3rd,sizeof(BYTE)*4);

				// Unicode to Multibyte char
				int		nLen;
				char*	pszMBCS;
				nLen = ::WideCharToMultiByte(CP_ACP, 0, (LPCWSTR)szFileName, -1, NULL, 0, NULL, NULL );
				pszMBCS = new char[nLen];
				::WideCharToMultiByte(CP_ACP, 0, (LPCWSTR)szFileName, wcslen((LPCWSTR)szFileName)+1, pszMBCS, nLen, NULL, NULL );
				memcpy(szFileName,pszMBCS,sizeof(char)*nLen); 
				delete	pszMBCS;
				
				dwCurPos += sizeof(m_DirEntry);
				continue;
			}
			else if(m_DirEntry.attribute == 0x20)
			{
				m_pList->InsertItem(dwDeleted,szFileName);
				dwDeleted++;
			}
		}
		dwCurPos += sizeof(m_DirEntry);
	}
	return;
}

BOOL CFATDrive::ReadLogicalSectors(DWORD dwStartSector, WORD wSectors, LPBYTE lpSectBuff)
{
	BOOL           fResult;
	DWORD          cb;
	DIOC_REGISTERS reg = {0};
	DISKIO         dio;

	dio.dwStartSector = dwStartSector;
	dio.wSectors      = wSectors;
	dio.dwBuffer      = (DWORD)lpSectBuff;

	reg.reg_EAX = 0x7305;   // Ext_ABSDiskReadWrite
	reg.reg_EBX = (DWORD)&dio;
	reg.reg_ECX = -1;
	reg.reg_EDX = m_bDriveNo;   // Int 21h, fn 7305h drive numbers are 1-based

	fResult = DeviceIoControl(m_hDrive, VWIN32_DIOC_DOS_DRIVEINFO,
		&reg, sizeof(reg),
		&reg, sizeof(reg), &cb, 0);

	// Determine if the DeviceIoControl call and the read succeeded.
	fResult = fResult && !(reg.reg_Flags & CARRY_FLAG);

	return fResult;
}
