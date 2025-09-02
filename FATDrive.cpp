#include "stdafx.h"
#include "FATdrive.h"
#include "Resource.h"
#include "Winioctl.h"
#include <shlwapi.h>

DWORD g_dwDeleted; //Listed deleted file count

CFATDrive::CFATDrive(void)
{
	m_puchFAT = NULL;
}

CFATDrive::~CFATDrive(void)
{
	extern bool g_bStop;
	g_bStop = true;

	if(m_puchFAT)
		delete[] m_puchFAT;
}

void CFATDrive::SetDrive(HANDLE hDev, BYTE byDriveNo, bool bIsNTandFAT, CListCtrl *pList, CEdit *pEditDelFiles, CProgressCtrl *pProgSearch)
{
	m_hDrive = hDev;
	m_byDriveNo = byDriveNo;
	m_pList = pList;
	m_pEditDelFiles = pEditDelFiles;
	m_pProgSearch = pProgSearch;
	m_bIsNTandFAT = bIsNTandFAT;
	m_bytesPerSector = 512;
}

int CFATDrive::LoadFAT()
{
	CString cstrMessage;
	m_bIsFAT12 = false;
	m_bIsFAT16 = false;

	int i;
	for(i = 0; i < 32; i++) //4
	{
		if(ReadLogicalSectors(0, 1, (LPBYTE)&m_BPBlock))
		{
			if(m_BPBlock.fat32.sectorsPerFAT == 0 && !memcmp(m_BPBlock.fat32.fileSystemType,"FAT32",5))
				;
			else if(m_BPBlock.fat32.sectorsPerFAT != 0 && !memcmp(m_BPBlock.fat16.fileSystemType,"FAT16",5))
				m_bIsFAT16 = true;
			else if(m_BPBlock.fat32.sectorsPerFAT != 0 && !memcmp(m_BPBlock.fat16.fileSystemType,"FAT12",5))
				m_bIsFAT12 = true;
			else
			{
				cstrMessage.LoadString(IDS_ERRMSG_UNKNOWN_FILESYSTEM);
				AfxMessageBox(cstrMessage, MB_OK|MB_ICONERROR);
				return ERROR_UNKNOWN_FILESYSTEM;
			}
			break;
		}
		m_bytesPerSector = m_bytesPerSector + 512;
	}

	if(i == 32) //4
	{
		cstrMessage.LoadString(IDS_ERRMSG_READ_BPB);
		AfxMessageBox(cstrMessage, MB_OK|MB_ICONERROR);
		return ERROR_INVALID_DRIVE;
	}

	// save some parameters
	m_bytesPerSector = *((WORD *)m_BPBlock.fat32.bytesPerSector);
	m_sectorsPerCluster = m_BPBlock.fat32.sectorsPerCluster;
	m_numberOfFATs = m_BPBlock.fat32.numberOfFATs;
	m_reservedSectors = *((WORD *)m_BPBlock.fat32.reservedSectors);
	m_rootDirStrtClus = *((DWORD *)m_BPBlock.fat32.rootDirStrtClus);
	if(m_bIsFAT12 || m_bIsFAT16)
	{
		m_rootEntries =  *((WORD *)m_BPBlock.fat16.rootEntries);
		m_SectorsPerFAT = *((WORD *)m_BPBlock.fat16.sectorsPerFAT);
	}
	else
		m_SectorsPerFAT = *((DWORD *)m_BPBlock.fat32.bigSectorsPerFAT);

	// caluculate basic parameters
	m_dwBytePerCluster = m_bytesPerSector * m_sectorsPerCluster;
	if(m_bIsFAT12 || m_bIsFAT16)
	{
		DWORD RootDirSectors = ((m_rootEntries * 32) + (m_bytesPerSector - 1)) / m_bytesPerSector;
		m_dwUserDataOffset = m_reservedSectors + m_numberOfFATs * m_SectorsPerFAT + RootDirSectors;
	}
	else
		m_dwUserDataOffset = m_reservedSectors + m_numberOfFATs * m_SectorsPerFAT;

	// load FAT
	m_puchFAT = new BYTE[m_bytesPerSector * m_SectorsPerFAT];
	if(m_bIsFAT12 || m_bIsFAT16)
	{
		if(!ReadLogicalSectors16(m_reservedSectors, (WORD)m_SectorsPerFAT, m_puchFAT)) 
			return ERROR_FAT_READ_FAILURE;
	}
	else
	{
		if(!ReadLogicalSectors(m_reservedSectors, (WORD)m_SectorsPerFAT, m_puchFAT)) 
			return ERROR_FAT_READ_FAILURE;
	}
	return ERROR_SUCCESS;
}

void CFATDrive::BeginScan(CString cstrFileNamePart)
{
	CString	cstrMessage, cstrDrive;
	m_cstrFileNamePart = cstrFileNamePart;
	m_cstrFileNamePart.MakeLower();
	g_dwDeleted = 0;
	
	TCHAR szRoot[3] = "";
	_stprintf_s(szRoot, 3, "%c:", m_byDriveNo + 0x60);
	CString cstrPathName = szRoot;
	cstrPathName.MakeUpper();

	cstrDrive.LoadString(IDS_FOLDER_UNKNOWN);
	cstrDrive = cstrPathName + "\\" + cstrDrive;

	if(m_bIsFAT12)
	{
		ScanDirectory12(0, cstrPathName, true, false);
		cstrMessage.LoadString(IDS_MSG_MORE_SCAN);
		if(MessageBox(NULL, cstrMessage, "DataRecovery", MB_OKCANCEL|MB_ICONINFORMATION|MB_TOPMOST) == IDCANCEL)
			return;
		ScanMore12(cstrDrive);
	}
	else if(m_bIsFAT16)
	{
		ScanDirectory16(0, cstrPathName, true, false);
		cstrMessage.LoadString(IDS_MSG_MORE_SCAN);
		if(MessageBox(NULL, cstrMessage, "DataRecovery", MB_OKCANCEL|MB_ICONINFORMATION|MB_TOPMOST) == IDCANCEL)
			return;
		ScanMore16(cstrDrive);
	}
	else
	{
		ScanDirectory32(m_rootDirStrtClus, cstrPathName, true, false);
		cstrMessage.LoadString(IDS_MSG_MORE_SCAN);
		if(MessageBox(NULL, cstrMessage, "DataRecovery", MB_OKCANCEL|MB_ICONINFORMATION|MB_TOPMOST) == IDCANCEL)
			return;
		ScanMore32(cstrDrive);
	}
	return;
}

void CFATDrive::ScanMore12(CString &cstrPathName)
{
	extern bool g_bStop;
	WORD wClusterNo;
	DWORD dwEntCnt = m_bytesPerSector * m_SectorsPerFAT / 3;

	m_pProgSearch->SetPos(0);
	m_pProgSearch->SetRange32(0, dwEntCnt);
	m_pProgSearch->SetStep(1);

	// find empty clusters and scan
	for(DWORD i = 1; i < dwEntCnt; i++)
	{
		if(g_bStop)
			break;

		wClusterNo = *((WORD *)&m_puchFAT[i*3]);
		wClusterNo = wClusterNo & 0x0FFF;
		if(wClusterNo == 0x000) 
			if(!ScanInEmptyCluster(cstrPathName, i*2))
				break;

		wClusterNo = *((WORD *)&m_puchFAT[i*3+1]);
		wClusterNo = wClusterNo >> 4;
		if(wClusterNo == 0x000) 
			if(!ScanInEmptyCluster(cstrPathName, i*2+1))
				break;

		m_pProgSearch->StepIt();
	}
	m_pProgSearch->SetPos(0);
}

void CFATDrive::ScanMore16(CString &cstrPathName)
{
	extern bool g_bStop;
	WORD wClusterNo;
	DWORD dwEntCnt = m_bytesPerSector * m_SectorsPerFAT / 2;

	m_pProgSearch->SetPos(0);
	m_pProgSearch->SetRange32(0, dwEntCnt);
	m_pProgSearch->SetStep(256);

	for(DWORD i = 2; i < dwEntCnt; i++)
	{
		if(g_bStop)
			break;

		wClusterNo = *((WORD *)&m_puchFAT[i*2]);
		if(wClusterNo == 0x0000)
		{
			if(!ScanInEmptyCluster(cstrPathName,i))
				break;
		}

		if((i & 0x000000ff) == 0)
			m_pProgSearch->StepIt();
	}

	m_pProgSearch->SetPos(0);
}

void CFATDrive::ScanMore32(CString &cstrPathName)
{
	extern bool g_bStop;
	DWORD dwClusterNo;
	DWORD dwEntCnt = m_bytesPerSector * m_SectorsPerFAT / 4;

	m_pProgSearch->SetPos(0);
	m_pProgSearch->SetRange32(0, dwEntCnt);
	m_pProgSearch->SetStep(256);

	// find empty clusters
	for(DWORD i = 2; i < dwEntCnt; i++)
	{
		if(g_bStop) // user wants to stop scanning
			break;

		dwClusterNo = *((DWORD *)&m_puchFAT[i*4]);
		dwClusterNo = dwClusterNo & 0x0fffffff;
		if(dwClusterNo == 0x00000000)
		{
			if(!ScanInEmptyCluster(cstrPathName,i))
				break;
		}

		if((i & 0x000000ff) == 0)
			m_pProgSearch->StepIt();
	}
	m_pProgSearch->SetPos(0);
}

BOOL CFATDrive::ScanInEmptyCluster(CString &cstrPathName, DWORD nClusterNo)
{
	BYTE *puchCluster;
	puchCluster = new BYTE[m_dwBytePerCluster];

	if(m_bIsFAT12 || m_bIsFAT16)
	{
		if(!ReadLogicalSectors16(m_dwUserDataOffset + (nClusterNo-2) * m_sectorsPerCluster, m_sectorsPerCluster, puchCluster)) 
		{
			//CString	cstrMessage;
			//cstrMessage.LoadString(IDS_ERRMSG_READ_ECLUSTER);
			//AfxMessageBox(cstrMessage, MB_OK|MB_ICONERROR);
			return FALSE;
		}
	}
	else
	{
		if(!ReadLogicalSectors(m_dwUserDataOffset + (nClusterNo-2) * m_sectorsPerCluster, m_sectorsPerCluster, puchCluster)) 
		{
			//CString	cstrMessage;
			//cstrMessage.LoadString(IDS_ERRMSG_READ_ECLUSTER);
			//AfxMessageBox(cstrMessage, MB_OK|MB_ICONERROR);
			return FALSE;
		}
	}

	DirEntry aDirEntry;
	LNFEntry aLNFEntry;
	LVITEM listItem;
	TCHAR szFileName[255] = "";
	TCHAR szBuffer[255] = "";
	DWORD iTemp = 0;
	int iLNFcnt = 0; //LNF sub entry count
	WORD wDate,wTime;
	DWORD dwStartCluster, dwStartLo, dwStartHi;
	DWORD dwFileSize;
	DWORD dwEntryCnt = m_dwBytePerCluster / 32;
	DWORD dwCurPos = 0;

	for(DWORD i = 0; i < dwEntryCnt ; i++)
	{
		memcpy(&aDirEntry,&puchCluster[dwCurPos],sizeof(aDirEntry));

		if(aDirEntry.name[0] == 0xe5) // this is it!
		{
			//if((aDirEntry.attribute & 0x3f) == 0x0f && aDirEntry.reserved == 0x00) //LNF ext. get file name(Unicode)
			if(aDirEntry.attribute == 0x0f && aDirEntry.reserved == 0x00) //LNF ext. get file name(Unicode)
			{
				memcpy(&aLNFEntry,&aDirEntry,sizeof(aDirEntry));

				if(iLNFcnt > 8)
				{
					dwCurPos += sizeof(aDirEntry);
					iLNFcnt = 0;
					continue;
				}

				// concatenate LNF sub entries
				if(iLNFcnt < 9)
				{
					memmove_s(&szFileName[26], 229, szFileName, 26*iLNFcnt);
					memcpy(szFileName,aLNFEntry.name1st,10);
					memcpy(&szFileName[10],aLNFEntry.name2nd,12);
					memcpy(&szFileName[22],aLNFEntry.name3rd,4);
				}
				dwCurPos += sizeof(aDirEntry);
				iLNFcnt++;
				iTemp = i;
				continue;
			}
			else if(aDirEntry.attribute == 0x20)
			{
				iLNFcnt = 0;
				if(iTemp + 1 != i) // no LNF entries
				{
					// making short file name 
					int chr = ' ';
					TCHAR szExt[3] = "";
					memcpy(&szFileName, aDirEntry.name,8);
					memcpy(&szExt, aDirEntry.extension,3);

					if(aDirEntry.name[0] == 0xe5)		
						szFileName[0] = 0x24; // convert delete mark to '$'

					if(aDirEntry.name[0] == 0x05)
						szFileName[0] = (TCHAR)0xe5; // convert Kanji first byte

					// skip not ascii file name
					if( !((szFileName[1] >= 0x30 && szFileName[1] <= 0x39) || (szFileName[1] >= 0x40 && szFileName[1] <= 0x5A) || szFileName[1] == 0x20))
					{
						dwCurPos += sizeof(aDirEntry);
						continue;
					}

					TCHAR *pDest = _tcschr(szFileName, chr);
					if(pDest)
					{
						if(szExt[0] == ' ')
							szFileName[pDest-szFileName] = 0x00;
						else
						{
							szFileName[pDest-szFileName] = 0x2e;
							memcpy(&szFileName[pDest-szFileName+1], aDirEntry.extension,3);
							szFileName[pDest-szFileName+4] = 0x00;
						}
					}
					else
					{
						if(szExt[0] == ' ')
							szFileName[8] = 0x00;
						else
						{
							memcpy(&szFileName[9], aDirEntry.extension,3);
							szFileName[8] = 0x2e; // '.'
							szFileName[12] = 0x00;
						}
					}
				}
				else //has LNF entries
				{
					//Unicode to Multibyte char
					int		nLen;
					TCHAR*	pszMBCS;
					nLen = ::WideCharToMultiByte(CP_ACP, 0, (LPCWSTR)szFileName, -1, NULL, 0, NULL, NULL );
					if(nLen == 0)
					{
						memset(szFileName, 0 ,255);
						dwCurPos += sizeof(aDirEntry);
						continue;
					}
					pszMBCS = new TCHAR[nLen];
					::WideCharToMultiByte(CP_ACP, 0, (LPCWSTR)szFileName, wcslen((LPCWSTR)szFileName)+1, pszMBCS, nLen, NULL, NULL );
					memcpy(szFileName,pszMBCS,sizeof(TCHAR)*nLen); 
					delete	pszMBCS;
				}
			}

			if(aDirEntry.attribute == 0x20)
			{
				if(m_bIsFAT12 || m_bIsFAT16)
				{
					dwStartCluster = *((WORD *)aDirEntry.cluster);
				}
				else
				{
					dwStartLo = *((WORD *)aDirEntry.cluster);
					dwStartHi = *((WORD *)aDirEntry.clusterHighWord);
					dwStartCluster = (dwStartHi << 16) | dwStartLo; 
					dwStartCluster = dwStartCluster & 0x0fffffff;
				}
				dwFileSize = *((DWORD *)aDirEntry.fileSize);

				if(szFileName[0] != 0x00 && dwFileSize != 0)
				{
					// partial file name matching
					CString cstrFileName = szFileName;

					if(m_cstrFileNamePart != "" && cstrFileName.Find(m_cstrFileNamePart, 0) == -1) // file name match
						goto NotDisplay;

					//extern TCHAR g_szDirType[80];
					SHFILEINFO sfi;

					// get file type system icon
					listItem.mask = LVIF_TEXT | LVIF_PARAM | LVIF_IMAGE;
					listItem.iItem = g_dwDeleted;
					listItem.iSubItem = 0;
					listItem.pszText = szFileName;
					LPTSTR pszExt = PathFindExtension(szFileName);
					// get file icon index from system image list
					if(SHGetFileInfo(pszExt, FILE_ATTRIBUTE_NORMAL, &sfi, sizeof(sfi), SHGFI_SYSICONINDEX | SHGFI_TYPENAME | SHGFI_USEFILEATTRIBUTES ))
						listItem.iImage = sfi.iIcon;
					listItem.lParam = (LPARAM)g_dwDeleted;
					// add item to list
					m_pList->InsertItem(&listItem);

					// add type to item
					_stprintf_s(szBuffer, 255, "%s", sfi.szTypeName);
					m_pList->SetItemText(g_dwDeleted,2,szBuffer);

					//add path to item
					m_pList->SetItemText(g_dwDeleted, 1, cstrPathName);

					//add filesize to item
					_stprintf_s(szBuffer, 255,"%10u", dwFileSize);
					m_pList->SetItemText(g_dwDeleted, 3, szBuffer);

					//add modified time & date to item
					wDate = *((WORD *)aDirEntry.updateDate);
					wTime = *((WORD *)aDirEntry.updateTime);
					DATE date;
					DosDateTimeToVariantTime(wDate, wTime, &date);
					COleDateTime cTime(date);
					_stprintf_s(szBuffer, 255,"%4d/%02d/%02d %02d:%02d", cTime.GetYear(),cTime.GetMonth(),cTime.GetDay(),cTime.GetHour(),cTime.GetMinute());
					m_pList->SetItemText(g_dwDeleted, 4, szBuffer);

					//add attribute
					SetAttribute(szBuffer, aDirEntry.attribute);
					m_pList->SetItemText(g_dwDeleted, 5, szBuffer);

					//add start-cluster number to item
					_stprintf_s(szBuffer, 255, "%d",dwStartCluster);
					m_pList->SetItemText(g_dwDeleted,6,szBuffer);

					//add cluster number to item
					_stprintf_s(szBuffer, 255, "%d", nClusterNo);
					m_pList->SetItemText(g_dwDeleted,7,szBuffer);

					//add cluster offset to item
					_stprintf_s(szBuffer, 255, "%d",dwCurPos);
					m_pList->SetItemText(g_dwDeleted,8,szBuffer);

					g_dwDeleted++;

					//show deleted files count
					_stprintf_s(szBuffer, 255, "%d", g_dwDeleted);
					m_pEditDelFiles->SetWindowText(szBuffer); //show deleted files count
NotDisplay:;
				}
			}
		}
		iLNFcnt = 0;
		memset(szFileName, 0 ,255);
		dwCurPos += sizeof(aDirEntry);
	}
	delete[] puchCluster;
	return TRUE;
}

void CFATDrive::ScanDirectory12(WORD wDirStrtClus, CString &cstrPathName, bool bRootDir, bool bDeletedDir)
{
	BYTE *puchDir;
	WORD wClusterNo = wDirStrtClus;
	WORD wClusterNoPrev = wDirStrtClus;
	DWORD dwDirSize = m_dwBytePerCluster;
	DWORD dwCurPos = wDirStrtClus * 1.5;
	DWORD RootDirSectors = ((m_rootEntries * 32) + (m_bytesPerSector - 1)) / m_bytesPerSector;

	extern bool g_bStop;
	if(g_bStop) // user wants to stop scanning
		return;

	if(bRootDir) // Load Root Directory
	{
		dwDirSize = RootDirSectors * m_bytesPerSector;
		puchDir = new BYTE[dwDirSize];
		if(!ReadLogicalSectors16(m_reservedSectors + m_numberOfFATs * m_SectorsPerFAT, RootDirSectors, puchDir)) 
		{
			CString	cstrMessage;
			cstrMessage.LoadString(IDS_ERRMSG_READ_ROOTDIR);
			AfxMessageBox(cstrMessage, MB_OK|MB_ICONERROR);
			return;
		}
	}
	else // Load Sub Directory
	{
		// caluculate Directory Size by following cluster chain
		if(!bDeletedDir) // if not deleted directory, set directory size to cluster size
		{
			for(;;)
			{
				if(dwCurPos > m_bytesPerSector * m_SectorsPerFAT - 2)
					break;

				wClusterNo = *((WORD *)&m_puchFAT[dwCurPos]);
				if(wClusterNoPrev & 0x0001) 
					wClusterNo = wClusterNo >> 4;	/* Cluster number is ODD */
				else
					wClusterNo = wClusterNo & 0x0FFF;	/* Cluster number is EVEN */

				if(wClusterNo >= 0x002 && wClusterNo <= 0xff6) //cluster chain
				{
					dwDirSize += m_dwBytePerCluster;
					dwCurPos = wClusterNo * 1.5;
					wClusterNoPrev = wClusterNo;
				}
				else if((wClusterNo >= 0xff8 && wClusterNo <= 0xfff) || wClusterNo == 0xff7 || wClusterNo == 0x001 || wClusterNo == 0x000) //last cluster and others
					break;
			}
		}

		puchDir = new BYTE[dwDirSize];

		DWORD dwCurPosRD = 0;
		wClusterNoPrev = wDirStrtClus;
		dwCurPos = wDirStrtClus * 1.5;

		// Load Directory
		for(;;)
		{
			if(!ReadLogicalSectors16((wClusterNo-2)*m_sectorsPerCluster+m_dwUserDataOffset, m_sectorsPerCluster, &puchDir[dwCurPosRD])) 
			{
				CString	cstrMessage;
				cstrMessage.LoadString(IDS_ERRMSG_READ_DIRECTORY);
				AfxMessageBox(cstrMessage, MB_OK|MB_ICONERROR);
				break;
			}

			if(dwCurPos > m_bytesPerSector * m_SectorsPerFAT - 2)
				break;

			wClusterNo = *((WORD *)&m_puchFAT[dwCurPos]);
			if(wClusterNoPrev & 0x0001) 
				wClusterNo = wClusterNo >> 4;	/* Cluster number is ODD */
			else
				wClusterNo = wClusterNo & 0x0FFF;	/* Cluster number is EVEN */

			if((wClusterNo >= 0xff8 && wClusterNo <= 0xfff) || wClusterNo == 0xff7 || wClusterNo == 0x001 || wClusterNo == 0x000 || bDeletedDir)
				break;
			else if(wClusterNo >= 0x002 && wClusterNo <= 0xff6) //cluster chain
			{
				dwCurPosRD += m_dwBytePerCluster;
				dwCurPos = wClusterNo * 1.5;
				wClusterNoPrev = wClusterNo;
			}
		}
	}

	DirEntry aDirEntry;
	LNFEntry aLNFEntry;
	LVITEM listItem;
	dwCurPos = 0;
	TCHAR szFileName[255] = "";
	TCHAR szBuffer[255] = "";
	DWORD iTemp = 0;
	int iLNFcnt = 0; //LNF sub entry count
	WORD wDate,wTime;
	WORD wStartCluster;
	DWORD dwFileSize;
	DWORD dwEntryCnt = dwDirSize/sizeof(aDirEntry);

	// set progress bar
	if(bRootDir)
	{
		int iRootEntCnt = 0;
		// count Root directory entries
		for(DWORD i = 0; i < dwEntryCnt ; i++)
		{
			memcpy(&aDirEntry,&puchDir[dwCurPos],sizeof(aDirEntry));			
			if(aDirEntry.name[0] == 0x00) //No more entry, skip search 
				break;
			iRootEntCnt++;
			dwCurPos += sizeof(aDirEntry);
		}
		m_pProgSearch->SetRange(0, iRootEntCnt);
		m_pProgSearch->SetStep(1);
	}

	dwCurPos = 0;
	// Scan Directory
	for(DWORD i = 0; i < dwEntryCnt ; i++)
	{
		if(bRootDir)
			m_pProgSearch->StepIt();

		memcpy(&aDirEntry,&puchDir[dwCurPos],sizeof(aDirEntry));
		
		//if Directory entry is already ocuppied by other data, skip search
		if(bDeletedDir && ((aDirEntry.attribute & 0xc0) != 0))
			break;

		if(aDirEntry.name[0] == 0x00) //No more entry, skip search 
			break;

		if(aDirEntry.name[0] == 0x2e) //. and .. entry in sub directory, next entry
		{
			dwCurPos += sizeof(aDirEntry);
			continue;
		}

		//if((aDirEntry.attribute & 0x3f) == 0x0f && aDirEntry.reserved == 0x00) //LNF ext. get file name(Unicode)
		if(aDirEntry.attribute == 0x0f && aDirEntry.reserved == 0x00) //LNF ext. get file name(Unicode)
		{
			memcpy(&aLNFEntry,&aDirEntry,sizeof(aDirEntry));

			// concatenate LNF sub entries
			if(iLNFcnt < 9)
			{
				memmove_s(&szFileName[26], 229, szFileName, 26*iLNFcnt);
				memcpy(szFileName,aLNFEntry.name1st,10);
				memcpy(&szFileName[10],aLNFEntry.name2nd,12);
				memcpy(&szFileName[22],aLNFEntry.name3rd,4);
			}
			dwCurPos += sizeof(aDirEntry);
			iLNFcnt++;
			iTemp = i;
			continue;
		}
		else if((aDirEntry.attribute & 0x18) == 0x00 || (aDirEntry.attribute & 0x18) == 0x10) // file or directory
		{
			iLNFcnt = 0;
			if(iTemp+1 != i) // no LNF entries
			{
				// making short file name 
				int chr = ' ';
				TCHAR szExt[3] = "";
				memcpy(&szFileName, aDirEntry.name,8);
				memcpy(&szExt, aDirEntry.extension,3);
				
				if(aDirEntry.name[0] == 0xe5)		
					szFileName[0] = 0x24; // convert delete mark to '$'
				
				if(aDirEntry.name[0] == 0x05)
					szFileName[0] = (TCHAR)0xe5; // convert Kanji first byte
				
				TCHAR *pDest = _tcschr(szFileName, chr);
				if(pDest)
				{
					if(szExt[0] == ' ')
						szFileName[pDest-szFileName] = 0x00;
					else
					{
						szFileName[pDest-szFileName] = 0x2e;
						memcpy(&szFileName[pDest-szFileName+1], aDirEntry.extension,3);
						szFileName[pDest-szFileName+4] = 0x00;
					}
				}
				else
				{
					if(szExt[0] == ' ')
						szFileName[8] = 0x00;
					else
					{
						memcpy(&szFileName[9], aDirEntry.extension,3);
						szFileName[8] = 0x2e; // '.'
						szFileName[12] = 0x00;
					}
				}
			}
			else //has LNF entries
			{
				// Unicode to Multibyte char
				int		nLen;
				TCHAR*	pszMBCS;
				nLen = ::WideCharToMultiByte(CP_ACP, 0, (LPCWSTR)szFileName, -1, NULL, 0, NULL, NULL );
				pszMBCS = new TCHAR[nLen];
				::WideCharToMultiByte(CP_ACP, 0, (LPCWSTR)szFileName, wcslen((LPCWSTR)szFileName)+1, pszMBCS, nLen, NULL, NULL );
				memcpy(szFileName,pszMBCS,sizeof(TCHAR)*nLen); 
				delete	pszMBCS;					
			}
		}

		if(aDirEntry.name[0] != 0xe5 && (aDirEntry.attribute & 0x18) == 0x10) //Non deleted directory, Scan Directory  
		{
			wStartCluster = *((WORD *)aDirEntry.cluster);

			CString cstrPath = szFileName;
			cstrPathName = cstrPathName + "\\" + cstrPath;

			ScanDirectory12(wStartCluster, cstrPathName, false, false);
			dwCurPos += sizeof(aDirEntry);
			continue;
		}	

		if(aDirEntry.name[0] == 0xe5 || bDeletedDir) // this is it!
		{
			wStartCluster = *((WORD *)aDirEntry.cluster);
			dwFileSize = *((DWORD *)aDirEntry.fileSize);
			if((aDirEntry.attribute & 0x18) == 0x10 || dwFileSize != 0) //SubDirectory or File with some size
			{
				// find cluster number in FAT. if not 0, it is overwritten.
				WORD wClusterNoTmp;
				DWORD dwCurPosTmp = wStartCluster * 1.5;

				if(dwCurPosTmp > m_bytesPerSector * m_SectorsPerFAT - 2)
					continue;

				wClusterNoTmp = *((WORD *)&m_puchFAT[dwCurPosTmp]);
				if(wStartCluster & 0x0001) 
					wClusterNoTmp = wClusterNoTmp >> 4;	/* Cluster number is ODD */
				else
					wClusterNoTmp = wClusterNoTmp & 0x0FFF;	/* Cluster number is EVEN */

				if(wClusterNoTmp == 0 && szFileName[0] != 0x00) // cluster not overwritten and filename not null 
				{
					// partial file name matching
					CString cstrFileName = szFileName;
					cstrFileName.MakeLower();
					
					if(m_cstrFileNamePart != "" && cstrFileName.Find(m_cstrFileNamePart, 0) == -1) // file name match
						goto PartialNameUnmatchSkip;

					extern int g_iDirIcon;
					extern TCHAR g_szDirType[80];
					SHFILEINFO sfi;

					// get file type system icon
					listItem.mask = LVIF_TEXT | LVIF_PARAM | LVIF_IMAGE;
					listItem.iItem = g_dwDeleted;
					listItem.iSubItem = 0;
					listItem.pszText = szFileName;
					LPTSTR pszExt = PathFindExtension(szFileName);
					if((aDirEntry.attribute & 0x18) == 0x10) // if Directory, use icon index previously set 
					{
						listItem.iImage = g_iDirIcon;
					} // get file icon index from system image list
					else if(SHGetFileInfo(pszExt, FILE_ATTRIBUTE_NORMAL, &sfi, sizeof(sfi), SHGFI_SYSICONINDEX | SHGFI_TYPENAME | SHGFI_USEFILEATTRIBUTES ))
						listItem.iImage = sfi.iIcon;
					listItem.lParam = (LPARAM)g_dwDeleted;
					// add item to list
					m_pList->InsertItem(&listItem);

					// add type to item
					if((aDirEntry.attribute & 0x18) == 0x10)
						_stprintf_s(szBuffer, 255, "%s", g_szDirType);
					else
						_stprintf_s(szBuffer, 255, "%s", sfi.szTypeName);
					m_pList->SetItemText(g_dwDeleted,2,szBuffer);
					//add parent directory to item
					if(cstrPathName.GetLength() == 2)
						cstrPathName += "\\"; 
					m_pList->SetItemText(g_dwDeleted, 1, cstrPathName);
					//add filesize to item
					if((aDirEntry.attribute & 0x18) == 0x00) // file only
					{
						_stprintf_s(szBuffer, 255, "%10u", dwFileSize);
						m_pList->SetItemText(g_dwDeleted, 3, szBuffer);
					}
					//add modified time & date to item
					wDate = *((WORD *)aDirEntry.updateDate);
					wTime = *((WORD *)aDirEntry.updateTime);
					DATE date;
					DosDateTimeToVariantTime(wDate, wTime, &date);
					COleDateTime cTime(date);
					_stprintf_s(szBuffer, 255, "%4d/%02d/%02d %02d:%02d", cTime.GetYear(),cTime.GetMonth(),cTime.GetDay(),cTime.GetHour(),cTime.GetMinute());
					m_pList->SetItemText(g_dwDeleted, 4, szBuffer);

					//add attribute
					SetAttribute(szBuffer, aDirEntry.attribute);
					m_pList->SetItemText(g_dwDeleted, 5, szBuffer);

					//add start-cluster number to item
					_stprintf_s(szBuffer, 255, "%d",wStartCluster);
					m_pList->SetItemText(g_dwDeleted,6,szBuffer);

					g_dwDeleted++;

					//show deleted files count
					_stprintf_s(szBuffer, 255, "%d", g_dwDeleted);
					m_pEditDelFiles->SetWindowText(szBuffer); //show deleted files count
				}
			}

PartialNameUnmatchSkip:			
			if((aDirEntry.attribute & 0x18) == 0x10) //Scan SubDirectory
			{
				if(cstrPathName.ReverseFind('\\') == 2) // cut last \ in "a:\"
					cstrPathName.TrimRight('\\');
				CString cstrPath = szFileName;
				cstrPathName = cstrPathName + "\\" + cstrPath;
				ScanDirectory12(wStartCluster, cstrPathName, false, true);
			}
		}
		iLNFcnt = 0;
		memset(szFileName, 0 ,255);
		dwCurPos += sizeof(aDirEntry);
	}

	cstrPathName = cstrPathName.Left(cstrPathName.ReverseFind('\\'));

	if(bRootDir)
		m_pProgSearch->SetPos(0);

	delete[] puchDir;
	return;
}

void CFATDrive::ScanDirectory16(WORD wDirStrtClus, CString &cstrPathName, bool bRootDir, bool bDeletedDir)
{
	BYTE *puchDir;
	WORD wClusterNo;
	DWORD dwDirSize = m_dwBytePerCluster;
	DWORD dwCurPos = 2 * wDirStrtClus;
	DWORD RootDirSectors = ((m_rootEntries * 32) + (m_bytesPerSector - 1)) / m_bytesPerSector;

	extern bool g_bStop;
	if(g_bStop) // user wants to stop scanning
		return;

	if(bRootDir) // Load Root Directory
	{
		dwDirSize = RootDirSectors * m_bytesPerSector;
		puchDir = new BYTE[dwDirSize];
		if(!ReadLogicalSectors16(m_reservedSectors + m_numberOfFATs * m_SectorsPerFAT, RootDirSectors, puchDir)) 
		{
			CString	cstrMessage;
			cstrMessage.LoadString(IDS_ERRMSG_READ_ROOTDIR);
			AfxMessageBox(cstrMessage, MB_OK|MB_ICONERROR);
			return;
		}
	}
	else // Load Sub Directory
	{
		// caluculate Directory Size by following cluster chain
		if(!bDeletedDir) // if Deleted directory, set directory size to cluster size
		{
			for(;;)
			{
				if(dwCurPos > m_bytesPerSector * m_SectorsPerFAT - 2)
					break;

				wClusterNo = *((WORD *)&m_puchFAT[dwCurPos]);
				if(wClusterNo >= 0x0002 && wClusterNo <= 0xfff6) //cluster chain
				{
					dwDirSize += m_dwBytePerCluster;
					dwCurPos = 2 * wClusterNo;
				}
				else if(wClusterNo >= 0xfff8 && wClusterNo <= 0xffff || wClusterNo == 0x0000  || wClusterNo == 0x0001 || wClusterNo == 0xfff7) //last cluster and others
					break;
			}
		}

		puchDir = new BYTE[dwDirSize];

		DWORD dwCurPosRD = 0;
		wClusterNo = wDirStrtClus;
		dwCurPos = 2 * wDirStrtClus;

		// Load Directory
		for(;;)
		{
			if(!ReadLogicalSectors16((wClusterNo-2)*m_sectorsPerCluster+m_dwUserDataOffset, m_sectorsPerCluster, &puchDir[dwCurPosRD])) 
			{
				CString	cstrMessage;
				//cstrMessage.Format("wClusterNo=%d, m_dwBytePerCluster=%d, dwCurPosRD=%d", wClusterNo, m_dwBytePerCluster, dwCurPosRD);
				cstrMessage.LoadString(IDS_ERRMSG_READ_DIRECTORY);
				AfxMessageBox(cstrMessage, MB_OK|MB_ICONERROR);
				break;
			}

			if(dwCurPos > m_bytesPerSector * m_SectorsPerFAT - 2)
				break;

			dwCurPosRD += m_dwBytePerCluster;
			wClusterNo = *((WORD *)&m_puchFAT[dwCurPos]);
			if((wClusterNo >= 0xfff8 && wClusterNo <= 0xffff) || wClusterNo == 0x0000 || wClusterNo == 0x0001 || wClusterNo == 0xfff7 || bDeletedDir)
				break;
			else if(wClusterNo >= 0x0002 && wClusterNo <= 0xfff6) //cluster chain
				dwCurPos = 2 * wClusterNo;
		}
	}

	DirEntry aDirEntry;
	LNFEntry aLNFEntry;
	LVITEM listItem;
	dwCurPos = 0;
	TCHAR szFileName[255] = "";
	TCHAR szBuffer[255] = "";
	DWORD iTemp = 0;
	int iLNFcnt = 0; //LNF sub entry count
	WORD wDate,wTime;
	WORD wStartCluster;
	DWORD dwFileSize;
	DWORD dwEntryCnt = dwDirSize/sizeof(aDirEntry);

	// set progress bar
	if(bRootDir)
	{
		int iRootEntCnt = 0;
		// count Root directory entries
		for(DWORD i = 0; i < dwEntryCnt ; i++)
		{
			iRootEntCnt++;
			memcpy(&aDirEntry,&puchDir[dwCurPos],sizeof(aDirEntry));			
			if(aDirEntry.name[0] == 0x00) //No more entry, skip search 
				break;
			dwCurPos += sizeof(aDirEntry);
		}
		m_pProgSearch->SetRange(0, iRootEntCnt);
		m_pProgSearch->SetStep(1);
	}

	dwCurPos = 0;
	// Scan Directory
	for(DWORD i = 0; i < dwEntryCnt ; i++)
	{
		if(bRootDir)
			m_pProgSearch->StepIt();

		memcpy(&aDirEntry,&puchDir[dwCurPos],sizeof(aDirEntry));
		
		//if Directory entry is already ocuppied by other data, skip search
		if(bDeletedDir && ((aDirEntry.attribute & 0xc0) != 0))
			break;

		if(aDirEntry.name[0] == 0x00) //No more entry, skip search 
			break;

		if(aDirEntry.name[0] == 0x2e) //. and .. entry in sub directory, next entry
		{
			dwCurPos += sizeof(aDirEntry);
			continue;
		}

		//if((aDirEntry.attribute & 0x3f) == 0x0f && aDirEntry.reserved == 0x00) //LNF ext. get file name(Unicode)
		if(aDirEntry.attribute == 0x0f && aDirEntry.reserved == 0x00) //LNF ext. get file name(Unicode)
		{
			memcpy(&aLNFEntry,&aDirEntry,sizeof(aDirEntry));

			// concatenate LNF sub entries
			if(iLNFcnt < 9)
			{
				memmove_s(&szFileName[26], 229, szFileName, 26*iLNFcnt);
				memcpy(szFileName,aLNFEntry.name1st,10);
				memcpy(&szFileName[10],aLNFEntry.name2nd,12);
				memcpy(&szFileName[22],aLNFEntry.name3rd,4);
			}
			dwCurPos += sizeof(aDirEntry);
			iLNFcnt++;
			iTemp = i;
			continue;
		}
		else if((aDirEntry.attribute & 0x18) == 0x00 || (aDirEntry.attribute & 0x18) == 0x10) // file or directory
		{
			iLNFcnt = 0;
			if(iTemp+1 != i) // no LNF entries
			{
				// making short file name 
				int chr = ' ';
				TCHAR szExt[3] = "";
				memcpy(&szFileName, aDirEntry.name,8);
				memcpy(&szExt, aDirEntry.extension,3);
				
				if(aDirEntry.name[0] == 0xe5)		
					szFileName[0] = 0x24; // convert delete mark to '$'
				
				if(aDirEntry.name[0] == 0x05)
					szFileName[0] = (TCHAR)0xe5; // convert Kanji first byte
								
				TCHAR *pDest = _tcschr(szFileName, chr);
				if(pDest)
				{
					if(szExt[0] == ' ')
						szFileName[pDest-szFileName] = 0x00;
					else
					{
						szFileName[pDest-szFileName] = 0x2e;
						memcpy(&szFileName[pDest-szFileName+1], aDirEntry.extension,3);
						szFileName[pDest-szFileName+4] = 0x00;
					}
				}
				else
				{
					if(szExt[0] == ' ')
						szFileName[8] = 0x00;
					else
					{
						memcpy(&szFileName[9], aDirEntry.extension,3);
						szFileName[8] = 0x2e; // '.'
						szFileName[12] = 0x00;
					}
				}
			}
			else //has LNF entries
			{
				// Unicode to Multibyte char
				int		nLen;
				TCHAR*	pszMBCS;
				nLen = ::WideCharToMultiByte(CP_ACP, 0, (LPCWSTR)szFileName, -1, NULL, 0, NULL, NULL );
				pszMBCS = new TCHAR[nLen];
				::WideCharToMultiByte(CP_ACP, 0, (LPCWSTR)szFileName, wcslen((LPCWSTR)szFileName)+1, pszMBCS, nLen, NULL, NULL );
				memcpy(szFileName,pszMBCS,sizeof(TCHAR)*nLen); 
				delete	pszMBCS;
			}
		}

		if(aDirEntry.name[0] != 0xe5 && (aDirEntry.attribute & 0x18) == 0x10) //Non deleted directory, Scan Directory  
		{
			wStartCluster = *((WORD *)aDirEntry.cluster);

			CString cstrPath = szFileName;
			cstrPathName = cstrPathName + "\\" + cstrPath;

			ScanDirectory16(wStartCluster, cstrPathName, false, false);
			dwCurPos += sizeof(aDirEntry);
			continue;
		}	

		if(aDirEntry.name[0] == 0xe5 || bDeletedDir) // this is it!
		{
			wStartCluster = *((WORD *)aDirEntry.cluster);
			dwFileSize = *((DWORD *)aDirEntry.fileSize);

			if((aDirEntry.attribute & 0x18) == 0x10 || dwFileSize != 0) //SubDirectory or File with some size
			{
				// find cluster number in FAT. if not 0, it is overwritten.
				WORD wClusterNoTmp;
				DWORD dwCurPosTmp = 2 * wStartCluster;
				if(dwCurPosTmp > m_bytesPerSector * m_SectorsPerFAT - 2)
					continue;
				wClusterNoTmp = *((WORD *)&m_puchFAT[dwCurPosTmp]);

				if(wClusterNoTmp == 0 && szFileName[0] != 0x00) // cluster not overwritten and filename not null 
				{
					// partial file name matching
					CString cstrFileName = szFileName;
					cstrFileName.MakeLower();
					if(m_cstrFileNamePart != "" && cstrFileName.Find(m_cstrFileNamePart, 0) == -1) // file name match
						goto PartialNameUnmatchSkip;

					extern int g_iDirIcon;
					extern TCHAR g_szDirType[80];
					SHFILEINFO sfi;

					// get file type system icon
					listItem.mask = LVIF_TEXT | LVIF_PARAM | LVIF_IMAGE;
					listItem.iItem = g_dwDeleted;
					listItem.iSubItem = 0;
					listItem.pszText = szFileName;
					LPTSTR pszExt = PathFindExtension(szFileName);
					if((aDirEntry.attribute & 0x18) == 0x10) // if Directory, use icon index previously set 
					{
						listItem.iImage = g_iDirIcon;
					} // get file icon index from system image list
					else if(SHGetFileInfo(pszExt, FILE_ATTRIBUTE_NORMAL, &sfi, sizeof(sfi), SHGFI_SYSICONINDEX | SHGFI_TYPENAME | SHGFI_USEFILEATTRIBUTES ))
						listItem.iImage = sfi.iIcon;
					listItem.lParam = (LPARAM)g_dwDeleted;
					// add item to list
					m_pList->InsertItem(&listItem);

					// add type to item
					if((aDirEntry.attribute & 0x18) == 0x10)
						_stprintf_s(szBuffer, 255, "%s", g_szDirType);
					else
						_stprintf_s(szBuffer, 255, "%s", sfi.szTypeName);
					m_pList->SetItemText(g_dwDeleted,2,szBuffer);
					//add parent directory to item
					if(cstrPathName.GetLength() == 2)
						cstrPathName += "\\"; 
					m_pList->SetItemText(g_dwDeleted, 1, cstrPathName);
					//add filesize to item
					if((aDirEntry.attribute & 0x18) == 0x00) // file only
					{
						_stprintf_s(szBuffer, 255, "%10u", dwFileSize);
						m_pList->SetItemText(g_dwDeleted, 3, szBuffer);
					}
					//add modified time & date to item
					wDate = *((WORD *)aDirEntry.updateDate);
					wTime = *((WORD *)aDirEntry.updateTime);
					DATE date;
					DosDateTimeToVariantTime(wDate, wTime, &date);
					COleDateTime cTime(date);
					_stprintf_s(szBuffer, 255, "%4d/%02d/%02d %02d:%02d", cTime.GetYear(),cTime.GetMonth(),cTime.GetDay(),cTime.GetHour(),cTime.GetMinute());
					m_pList->SetItemText(g_dwDeleted, 4, szBuffer);

					//add attribute
					SetAttribute(szBuffer, aDirEntry.attribute);
					m_pList->SetItemText(g_dwDeleted, 5, szBuffer);

					//add start-cluster number to item
					_stprintf_s(szBuffer, 255, "%d",wStartCluster);
					m_pList->SetItemText(g_dwDeleted,6,szBuffer);

					//add cluster number to item
					_stprintf_s(szBuffer, 255, "%d", 0);
					m_pList->SetItemText(g_dwDeleted,7,szBuffer);

					g_dwDeleted++;

					//show deleted files count
					_stprintf_s(szBuffer, 255, "%d", g_dwDeleted);
					m_pEditDelFiles->SetWindowText(szBuffer);
				}
			}

PartialNameUnmatchSkip:			
			if((aDirEntry.attribute & 0x18) == 0x10) //Scan SubDirectory
			{
				if(cstrPathName.ReverseFind('\\') == 2) // cut last \ in "a:\"
					cstrPathName.TrimRight('\\');
				CString cstrPath = szFileName;
				cstrPathName = cstrPathName + "\\" + cstrPath;
				ScanDirectory16(wStartCluster, cstrPathName, false, true);
			}
		}
		iLNFcnt = 0;
		memset(szFileName, 0 ,255);
		dwCurPos += sizeof(aDirEntry);
	}

	cstrPathName = cstrPathName.Left(cstrPathName.ReverseFind('\\'));

	if(bRootDir)
		m_pProgSearch->SetPos(0);

	delete[] puchDir;
	return;
}

void CFATDrive::ScanDirectory32(DWORD dwDirStrtClus, CString &cstrPathName, bool bRootDir, bool bDeletedDir)
{
	BYTE *puchDir;
	DWORD dwClusterNo;
	DWORD dwDirSize = m_dwBytePerCluster;
	DWORD dwCurPos = 4 * dwDirStrtClus;
	extern bool g_bStop;

	if(g_bStop) // user wants to stop scanning
		return;

	// caluculate Directory Size by following cluster chain
	for(;;)
	{
		if(dwCurPos > m_bytesPerSector * m_SectorsPerFAT - 4)
			break;

		dwClusterNo = *((DWORD *)&m_puchFAT[dwCurPos]);
		dwClusterNo = dwClusterNo & 0x0fffffff;
		if(dwClusterNo >= 0x00000002 && dwClusterNo <= 0x0ffffff6) //cluster chain
		{
			dwDirSize += m_dwBytePerCluster;
			dwCurPos = 4 * dwClusterNo;
		}
		else if(dwClusterNo >= 0x0ffffff8 && dwClusterNo <= 0x0fffffff || dwClusterNo == 0x00000000 || dwClusterNo == 0x00000001 || dwClusterNo == 0x0ffffff7) //last cluster
			break;
	}


	puchDir = new BYTE[dwDirSize];

	DWORD dwCurPosRD = 0;
	dwClusterNo = dwDirStrtClus;
	dwCurPos = 4 * dwDirStrtClus;

	// Load Directory
	for(;;)
	{
		if(!ReadLogicalSectors((dwClusterNo-2)*m_sectorsPerCluster+m_dwUserDataOffset, m_sectorsPerCluster, &puchDir[dwCurPosRD])) 
		{
			CString	cstrMessage;
			cstrMessage.LoadString(IDS_ERRMSG_READ_DIRECTORY);
			AfxMessageBox(cstrMessage, MB_OK|MB_ICONERROR);
			break;
		}

		if(dwCurPos > m_bytesPerSector * m_SectorsPerFAT - 4)
			break;

		dwCurPosRD += m_dwBytePerCluster;
		dwClusterNo = *((DWORD *)&m_puchFAT[dwCurPos]);//next cluster number 
		dwClusterNo = dwClusterNo & 0x0fffffff;
		if((dwClusterNo >= 0x0ffffff8 && dwClusterNo <= 0x0fffffff) || dwClusterNo == 0x00000000 || dwClusterNo == 0x00000001 || dwClusterNo == 0x0ffffff7)
			break;
		else if(dwClusterNo >= 0x00000002 && dwClusterNo <= 0x0ffffff6) //cluster chain
			dwCurPos = 4 * dwClusterNo;
	}

	DirEntry aDirEntry;
	LNFEntry aLNFEntry;
	LVITEM listItem;
	dwCurPos = 0;
	TCHAR szFileName[255] = "";
	TCHAR szBuffer[255] = "";
	DWORD iTemp = 0;
	int iLNFcnt = 0; //LNF sub entry count
	WORD wDate,wTime;
	DWORD dwStartLo,dwStartHi,dwStartCluster,dwFileSize;
	DWORD dwEntryCnt = dwDirSize/sizeof(aDirEntry);

	// set progress bar
	if(bRootDir)
	{
		int iRootEntCnt = 0;
		// count Root directory entries
		for(DWORD i = 0; i < dwEntryCnt ; i++)
		{
			iRootEntCnt++;
			memcpy(&aDirEntry,&puchDir[dwCurPos],sizeof(aDirEntry));			
			if(aDirEntry.name[0] == 0x00) //No more entry, skip search 
				break;
			dwCurPos += sizeof(aDirEntry);
		}
		m_pProgSearch->SetRange(0, iRootEntCnt);
		m_pProgSearch->SetStep(1);
	}

	dwCurPos = 0;
	// Scan Directory
	for(DWORD i = 0; i < dwEntryCnt ; i++)
	{
		if(bRootDir)
			m_pProgSearch->StepIt();

		memcpy(&aDirEntry,&puchDir[dwCurPos],sizeof(aDirEntry));
		
		//if Directory entry is already ocuppied by other data, skip search
		if(bDeletedDir && ((aDirEntry.attribute & 0xc0) != 0))
			break;

		if(aDirEntry.name[0] == 0x00) //No more entry, skip search 
			break;

		if(aDirEntry.name[0] == 0x2e) //. and .. entry in sub directory, next entry
		{
			dwCurPos += sizeof(aDirEntry);
			continue;
		}

		//if((aDirEntry.attribute & 0x3f) == 0x0f && aDirEntry.reserved == 0x00) //LNF ext. get file name(Unicode)
		if(aDirEntry.attribute == 0x0f && aDirEntry.reserved == 0x00) //LNF ext. get file name(Unicode)
		{
			memcpy(&aLNFEntry,&aDirEntry,sizeof(aDirEntry));
	
			// concatenate LNF sub entries
			if(iLNFcnt < 9)
			{
				memmove_s(&szFileName[26], 229,szFileName, 26*iLNFcnt);
				memcpy(szFileName,aLNFEntry.name1st,10);
				memcpy(&szFileName[10],aLNFEntry.name2nd,12);
				memcpy(&szFileName[22],aLNFEntry.name3rd,4);
			}
			dwCurPos += sizeof(aDirEntry);
			iLNFcnt++;
			iTemp = i;
			continue;
		}
		else if((aDirEntry.attribute & 0x18) == 0x00 || (aDirEntry.attribute & 0x18) == 0x10) // file or directory
		{
			iLNFcnt = 0;
			if(iTemp+1 != i) // no LNF entries
			{
				// making short file name 
				int chr = ' ';
				TCHAR szExt[3] = "";
				memcpy(&szFileName, aDirEntry.name,8);
				memcpy(&szExt, aDirEntry.extension,3);
				
				if(aDirEntry.name[0] == 0xe5)		
					szFileName[0] = 0x24; // convert delete mark to '$'
				
				if(aDirEntry.name[0] == 0x05)
					szFileName[0] = (TCHAR)0xe5; // convert Kanji first byte
				
				TCHAR *pDest = _tcschr(szFileName, chr);
				if(pDest)
				{
					if(szExt[0] == ' ')
						szFileName[pDest-szFileName] = 0x00;
					else
					{
						szFileName[pDest-szFileName] = 0x2e;
						memcpy(&szFileName[pDest-szFileName+1], aDirEntry.extension,3);
						szFileName[pDest-szFileName+4] = 0x00;
					}
				}
				else
				{
					if(szExt[0] == ' ')
						szFileName[8] = 0x00;
					else
					{
						memcpy(&szFileName[9], aDirEntry.extension,3);
						szFileName[8] = 0x2e; // '.'
						szFileName[12] = 0x00;
					}
				}
			}
			else //has LNF entries
			{
				// Unicode to Multibyte char
				int		nLen;
				TCHAR*	pszMBCS;
				nLen = ::WideCharToMultiByte(CP_ACP, 0, (LPCWSTR)szFileName, -1, NULL, 0, NULL, NULL );
				pszMBCS = new TCHAR[nLen];
				::WideCharToMultiByte(CP_ACP, 0, (LPCWSTR)szFileName, wcslen((LPCWSTR)szFileName)+1, pszMBCS, nLen, NULL, NULL );
				memcpy(szFileName,pszMBCS,sizeof(TCHAR)*nLen); 
				delete	pszMBCS;					
			}
		}

		if(aDirEntry.name[0] != 0xe5 && (aDirEntry.attribute & 0x18) == 0x10) //Non deleted directory, Scan Directory  
		{
			dwStartLo = *((WORD *)aDirEntry.cluster);
			dwStartHi = *((WORD *)aDirEntry.clusterHighWord);
			dwStartCluster = (dwStartHi << 16) | dwStartLo; 
			dwStartCluster = dwStartCluster & 0x0fffffff;

			if((aDirEntry.attribute & 0x18) == 0x10 || dwFileSize != 0) //SubDirectory or File with some size
			{
				// find cluster number in FAT. if not 0, it is overwritten.
				DWORD dwClusterNoTmp;
				DWORD dwCurPosTmp = 4 * dwStartCluster;
				if(dwCurPosTmp > m_bytesPerSector * m_SectorsPerFAT - 4)
					continue;
				dwClusterNoTmp = *((DWORD *)&m_puchFAT[dwCurPosTmp]);
				dwClusterNoTmp = dwClusterNoTmp & 0x0fffffff;

				if(dwClusterNoTmp == 0 && szFileName[0] != 0x00) // cluster not overwritten and filename not null 
				{
					// partial file name matching
					CString cstrFileName = szFileName;
					cstrFileName.MakeLower();
					if(m_cstrFileNamePart != "" && cstrFileName.Find(m_cstrFileNamePart, 0) == -1) // file name match
						goto PartialNameUnmatchSkip;

					extern int g_iDirIcon;
					extern TCHAR g_szDirType[80];
					SHFILEINFO sfi;

					// get file type system icon
					listItem.mask = LVIF_TEXT | LVIF_PARAM | LVIF_IMAGE;
					listItem.iItem = g_dwDeleted;
					listItem.iSubItem = 0;
					listItem.pszText = szFileName;
					LPTSTR pszExt = PathFindExtension(szFileName);
					if((aDirEntry.attribute & 0x18) == 0x10) // if Directory, use icon index previously set 
					{
						listItem.iImage = g_iDirIcon;
					} // get file icon index from system image list
					else if(SHGetFileInfo(pszExt, FILE_ATTRIBUTE_NORMAL, &sfi, sizeof(sfi), SHGFI_SYSICONINDEX | SHGFI_TYPENAME | SHGFI_USEFILEATTRIBUTES ))
						listItem.iImage = sfi.iIcon;
					listItem.lParam = (LPARAM)g_dwDeleted;
					// add item to list
					m_pList->InsertItem(&listItem);

					// add type to item
					if((aDirEntry.attribute & 0x18) == 0x10)
						_stprintf_s(szBuffer, 255, "%s", g_szDirType);
					else
						_stprintf_s(szBuffer, 255, "%s", sfi.szTypeName);
					m_pList->SetItemText(g_dwDeleted,2,szBuffer);
					//add parent directory to item
					if(cstrPathName.GetLength() == 2)
						cstrPathName += "\\"; 

					CString cstrPathNameTemp = cstrPathName;
					if(cstrPathNameTemp.Find("\\\\") == 2)
						cstrPathNameTemp.Delete(3);

					m_pList->SetItemText(g_dwDeleted, 1, cstrPathNameTemp);
					//add filesize to item
					if((aDirEntry.attribute & 0x18) == 0x00) // file only
					{
						_stprintf_s(szBuffer, 255, "%10u", dwFileSize);
						m_pList->SetItemText(g_dwDeleted, 3, szBuffer);
					}
					//add modified time & date to item
					wDate = *((WORD *)aDirEntry.updateDate);
					wTime = *((WORD *)aDirEntry.updateTime);
					DATE date;
					DosDateTimeToVariantTime(wDate, wTime, &date);
					COleDateTime cTime(date);
					_stprintf_s(szBuffer, 255, "%4d/%02d/%02d %02d:%02d", cTime.GetYear(),cTime.GetMonth(),cTime.GetDay(),cTime.GetHour(),cTime.GetMinute());
					m_pList->SetItemText(g_dwDeleted, 4, szBuffer);

					//add attribute
					SetAttribute(szBuffer, aDirEntry.attribute);
					m_pList->SetItemText(g_dwDeleted, 5, szBuffer);

					//add start-cluster number to item
					_stprintf_s(szBuffer, 255, "%d",dwStartCluster);
					m_pList->SetItemText(g_dwDeleted,6,szBuffer);

					g_dwDeleted++;

					//show deleted files count
					_stprintf_s(szBuffer, 255, "%d", g_dwDeleted);
					m_pEditDelFiles->SetWindowText(szBuffer); //show deleted files count
				}
			}

			CString cstrPath = szFileName;
			cstrPathName = cstrPathName + "\\" + cstrPath;

			ScanDirectory32(dwStartCluster, cstrPathName, false, bDeletedDir);
			dwCurPos += sizeof(aDirEntry);
			continue;
		}	

		if(aDirEntry.name[0] == 0xe5 || bDeletedDir) // this is it!
		{
			dwStartLo = *((WORD *)aDirEntry.cluster);
			dwStartHi = *((WORD *)aDirEntry.clusterHighWord);
			dwStartCluster = (dwStartHi << 16) | dwStartLo; 
			dwStartCluster = dwStartCluster & 0x0fffffff;

			dwFileSize = *((DWORD *)aDirEntry.fileSize);

			if((aDirEntry.attribute & 0x18) == 0x10 || dwFileSize != 0) //SubDirectory or File with some size
			{
				// find cluster number in FAT. if not 0, it is overwritten.
				DWORD dwClusterNoTmp;
				DWORD dwCurPosTmp = 4 * dwStartCluster;
				if(dwCurPosTmp > m_bytesPerSector * m_SectorsPerFAT - 4)
					continue;
				dwClusterNoTmp = *((DWORD *)&m_puchFAT[dwCurPosTmp]);
				dwClusterNoTmp = dwClusterNoTmp & 0x0fffffff;

				if(dwClusterNoTmp == 0 && szFileName[0] != 0x00) // cluster not overwritten and filename not null 
				{
					// partial file name matching
					CString cstrFileName = szFileName;
					cstrFileName.MakeLower();
					if(m_cstrFileNamePart != "" && cstrFileName.Find(m_cstrFileNamePart, 0) == -1) // file name match
						goto PartialNameUnmatchSkip;

					extern int g_iDirIcon;
					extern TCHAR g_szDirType[80];
					SHFILEINFO sfi;

					// get file type system icon
					listItem.mask = LVIF_TEXT | LVIF_PARAM | LVIF_IMAGE;
					listItem.iItem = g_dwDeleted;
					listItem.iSubItem = 0;
					listItem.pszText = szFileName;
					LPTSTR pszExt = PathFindExtension(szFileName);
					if((aDirEntry.attribute & 0x18) == 0x10) // if Directory, use icon index previously set 
					{
						listItem.iImage = g_iDirIcon;
					} // get file icon index from system image list
					else if(SHGetFileInfo(pszExt, FILE_ATTRIBUTE_NORMAL, &sfi, sizeof(sfi), SHGFI_SYSICONINDEX | SHGFI_TYPENAME | SHGFI_USEFILEATTRIBUTES ))
						listItem.iImage = sfi.iIcon;
					listItem.lParam = (LPARAM)g_dwDeleted;
					// add item to list
					m_pList->InsertItem(&listItem);

					// add type to item
					if((aDirEntry.attribute & 0x18) == 0x10)
						_stprintf_s(szBuffer, 255, "%s", g_szDirType);
					else
						_stprintf_s(szBuffer, 255, "%s", sfi.szTypeName);
					m_pList->SetItemText(g_dwDeleted,2,szBuffer);
					//add parent directory to item
					if(cstrPathName.GetLength() == 2)
						cstrPathName += "\\"; 

					CString cstrPathNameTemp = cstrPathName;
					if(cstrPathNameTemp.Find("\\\\") == 2)
						cstrPathNameTemp.Delete(3);

					m_pList->SetItemText(g_dwDeleted, 1, cstrPathNameTemp);
					//add filesize to item
					if((aDirEntry.attribute & 0x18) == 0x00) // file only
					{
						_stprintf_s(szBuffer, 255, "%10u", dwFileSize);
						m_pList->SetItemText(g_dwDeleted, 3, szBuffer);
					}
					//add modified time & date to item
					wDate = *((WORD *)aDirEntry.updateDate);
					wTime = *((WORD *)aDirEntry.updateTime);
					DATE date;
					DosDateTimeToVariantTime(wDate, wTime, &date);
					COleDateTime cTime(date);
					_stprintf_s(szBuffer, 255, "%4d/%02d/%02d %02d:%02d", cTime.GetYear(),cTime.GetMonth(),cTime.GetDay(),cTime.GetHour(),cTime.GetMinute());
					m_pList->SetItemText(g_dwDeleted, 4, szBuffer);

					//add attribute
					SetAttribute(szBuffer, aDirEntry.attribute);
					m_pList->SetItemText(g_dwDeleted, 5, szBuffer);

					//add start-cluster number to item
					_stprintf_s(szBuffer, 255, "%d",dwStartCluster);
					m_pList->SetItemText(g_dwDeleted,6,szBuffer);

					g_dwDeleted++;

					//show deleted files count
					_stprintf_s(szBuffer, 255, "%d", g_dwDeleted);
					m_pEditDelFiles->SetWindowText(szBuffer); //show deleted files count
				}
			}

PartialNameUnmatchSkip:			
			if((aDirEntry.attribute & 0x18) == 0x10) //Scan SubDirectory
			{
				CString cstrPath = szFileName;
				cstrPathName = cstrPathName + "\\" + cstrPath;
				ScanDirectory32(dwStartCluster, cstrPathName, false, true);
			}
		}
		iLNFcnt = 0;
		memset(szFileName, 0 ,255);
		dwCurPos += sizeof(aDirEntry);
	}

	cstrPathName = cstrPathName.Left(cstrPathName.ReverseFind('\\'));

	if(bRootDir)
		m_pProgSearch->SetPos(0);

	delete[] puchDir;
	return;
}

int CFATDrive::RecoveryFile(HANDLE hNewFile, DWORD dwStartCluster, LONGLONG iFileSize, CProgressCtrl *pProgDataRecovery)
{
	DWORD dwBytesRead = 0;
	DWORD dwBytesToRead = m_bytesPerSector;
	//int iSectorCnt = iFileSize / m_bytesPerSector;
	//int iSectorRemainder = iFileSize % m_bytesPerSector;
	//int iClusterOffset = 1;
	//int iSectorOffset = 0;
	DWORD iSectorCnt = iFileSize / m_bytesPerSector;
	DWORD iSectorRemainder = iFileSize % m_bytesPerSector;
	DWORD iClusterOffset = 1;
	DWORD iSectorOffset = 0;
	iSectorCnt++;


	BYTE *puchData = new BYTE[m_bytesPerSector];

	for(DWORD i = 0; i < iSectorCnt; i++)
	{
		if(i != 0 && i%m_sectorsPerCluster == 0)//has reached cluster limit
		{
			for(;;) //find next empty(not overwritten) cluster
			{
				DWORD dwClusterNoTmp;
				DWORD dwClusterNoPrev;
				DWORD dwCurPosTmp;
				if(m_bIsFAT12)
				{
					WORD wClusterNoTmp;
					dwClusterNoPrev	= dwStartCluster + iClusterOffset;
					dwCurPosTmp = dwClusterNoPrev + dwClusterNoPrev / 2;
					wClusterNoTmp = *((WORD *)&m_puchFAT[dwCurPosTmp]);
					if(dwClusterNoPrev & 0x0001) 
						wClusterNoTmp = wClusterNoTmp >> 4;	/* Cluster number is ODD */
					else
						wClusterNoTmp = wClusterNoTmp & 0x0FFF;	/* Cluster number is EVEN */
					dwClusterNoTmp = wClusterNoTmp;
				}
				else if(m_bIsFAT16)
				{
					dwCurPosTmp = 2 * (dwStartCluster + iClusterOffset);
					dwClusterNoTmp = *((WORD *)&m_puchFAT[dwCurPosTmp]);
				}
				else
				{
					dwCurPosTmp = 4 * (dwStartCluster + iClusterOffset);
					dwClusterNoTmp = *((DWORD *)&m_puchFAT[dwCurPosTmp]);
				}

				if(dwClusterNoTmp == 0)
				{
					dwStartCluster += iClusterOffset;
					iClusterOffset = 1;
					iSectorOffset = 0;
					break;
				}
				iClusterOffset++;
			}
		}
		// read sectors one by one
		if(m_bIsFAT12 || m_bIsFAT16)
		{
			if(!ReadLogicalSectors16((dwStartCluster-2)*m_sectorsPerCluster+m_dwUserDataOffset+iSectorOffset, 1, puchData)) 
			{
				delete[] puchData;
				return GetLastError();
			}
		}
		else
		{
			if(!ReadLogicalSectors((dwStartCluster-2)*m_sectorsPerCluster+m_dwUserDataOffset+iSectorOffset, 1, puchData)) 
			{
				delete[] puchData;
				return GetLastError();	
			}
		}

		if(i == iSectorCnt-1) //if last sector
			dwBytesToRead = iSectorRemainder; // write only remainder
		if(!WriteFile(hNewFile,puchData,dwBytesToRead,&dwBytesRead,NULL))
		{
			delete[] puchData;
			return GetLastError();
		}

		iSectorOffset++;
		pProgDataRecovery->OffsetPos(m_bytesPerSector);
	}
	delete[] puchData;
	return ERROR_SUCCESS;
}

int CFATDrive::DeleteFile(DWORD dwStartCluster, int iFileSize, DWORD dwClusterNo, DWORD dwClusterOffset, CProgressCtrl *pProgDataRecovery)
{
	////////////// 空きクラスタスキャンでリストアップされたファイルの消去
	if(dwClusterNo != 0)
	{
		BYTE *puchCluster, *puchNull32;
		puchCluster = new BYTE[m_dwBytePerCluster];

		// いったん所定のクラスタを読み込み
		if(m_bIsFAT12 || m_bIsFAT16)
		{
			if(!ReadLogicalSectors16(m_dwUserDataOffset + (dwClusterNo-2) * m_sectorsPerCluster, m_sectorsPerCluster, puchCluster)) 
			{
				delete[] puchCluster;
				return GetLastError();
			}
		}
		else
		{
			if(!ReadLogicalSectors(m_dwUserDataOffset + (dwClusterNo-2) * m_sectorsPerCluster, m_sectorsPerCluster, puchCluster)) 
			{
				delete[] puchCluster;
				return GetLastError();
			}
		}

		// 目的ファイルのレコードのみNULLにして
		puchNull32 = new BYTE[32];
		ZeroMemory(puchNull32, 32);
		memcpy(&puchCluster[dwClusterOffset], puchNull32, 32);

		// 再度ディスク上にクラスタを書き込む
		if(!WriteLogicalSectors(m_dwUserDataOffset + (dwClusterNo-2) * m_sectorsPerCluster, m_sectorsPerCluster, puchCluster)) 
		{
			delete[] puchNull32;
			delete[] puchCluster;
			return GetLastError();
		}

		delete[] puchNull32;
		delete[] puchCluster;
		return ERROR_SUCCESS;
	}

	////////////// 通常スキャンでリストアップされたファイルの消去
	int iSectorCnt = iFileSize / m_bytesPerSector;
	int iClusterOffset = 1;
	int iSectorOffset = 0;
	iSectorCnt++;

	BYTE *puchData = new BYTE[m_bytesPerSector];
	ZeroMemory(puchData, m_bytesPerSector);

	// overwrite the start cluster of memory FAT with last cluster flag
	if(m_bIsFAT12)
	{
		DWORD dwCurPosTmp = dwStartCluster + dwStartCluster / 2;
		WORD wClusterNoTmp = *((WORD *)&m_puchFAT[dwCurPosTmp]);
		if(dwStartCluster & 0x00000001) /* Cluster number is ODD */
			wClusterNoTmp = wClusterNoTmp | 0xfff0;	
		else						/* Cluster number is EVEN */
			wClusterNoTmp = wClusterNoTmp | 0x0fff;	
		*((WORD *)&m_puchFAT[dwCurPosTmp]) = wClusterNoTmp;
	}
	else if(m_bIsFAT16)
	{
		*((WORD *)&m_puchFAT[dwStartCluster*2]) = 0xffff;
	}
	else
	{
		*((DWORD *)&m_puchFAT[dwStartCluster*4]) = 0xffffffff;
	}
		
	// write memory FAT to disc
	if(!WriteLogicalSectors(m_reservedSectors, (WORD)m_SectorsPerFAT, m_puchFAT)) 
		return ERROR_FAT_READ_FAILURE;


	// delete actual data
	for(int i = 0; i < iSectorCnt; i++)
	{
		if(i != 0 && i%m_sectorsPerCluster == 0)//has reached cluster limit
		{
			for(;;) //find next empty(not overwritten) cluster
			{
				DWORD dwClusterNoTmp;
				DWORD dwClusterNoPrev;
				DWORD dwCurPosTmp;
				if(m_bIsFAT12)
				{
					WORD wClusterNoTmp;
					dwClusterNoPrev	= dwStartCluster + iClusterOffset;
					dwCurPosTmp = dwClusterNoPrev + dwClusterNoPrev / 2;
					wClusterNoTmp = *((WORD *)&m_puchFAT[dwCurPosTmp]);
					if(dwClusterNoPrev & 0x00000001) 
						wClusterNoTmp = wClusterNoTmp >> 4;	/* Cluster number is ODD */
					else
						wClusterNoTmp = wClusterNoTmp & 0x0FFF;	/* Cluster number is EVEN */
					dwClusterNoTmp = wClusterNoTmp;
				}
				else if(m_bIsFAT16)
				{
					dwCurPosTmp = 2 * (dwStartCluster + iClusterOffset);
					dwClusterNoTmp = *((WORD *)&m_puchFAT[dwCurPosTmp]);
				}
				else
				{
					dwCurPosTmp = 4 * (dwStartCluster + iClusterOffset);
					dwClusterNoTmp = *((DWORD *)&m_puchFAT[dwCurPosTmp]);
				}

				if(dwClusterNoTmp == 0)
				{
					dwStartCluster += iClusterOffset;
					iClusterOffset = 1;
					iSectorOffset = 0;
					break;
				}
				iClusterOffset++;
			}
		}
		// write null sectors one by one to disc
		if(!WriteLogicalSectors((dwStartCluster-2)*m_sectorsPerCluster+m_dwUserDataOffset+iSectorOffset, 1, puchData)) 
		{
			delete[] puchData;
			return GetLastError();
		}
		iSectorOffset++;
		pProgDataRecovery->OffsetPos(m_bytesPerSector);
	}

	delete[] puchData;
	return ERROR_SUCCESS;
}

BOOL CFATDrive::ReadLogicalSectors(DWORD dwStartSector, WORD wSectors, LPBYTE lpSectBuff)
{
	if(m_bIsNTandFAT)
	{
		DWORD bytesread, dwRet, dwError;
		LARGE_INTEGER liFilePos;

		liFilePos.QuadPart = (LONGLONG)dwStartSector*m_bytesPerSector;
		dwRet = SetFilePointer(m_hDrive, liFilePos.LowPart, &liFilePos.HighPart, FILE_BEGIN);
		if (dwRet == INVALID_SET_FILE_POINTER && (dwError = GetLastError()) != NO_ERROR )
		{
			//ErrorMessage(dwError);
			//AfxMessageBox(_T("CFATDrive::ReadLogicalSectors#SetFilePointer Failed!!"), MB_OKCANCEL|MB_ICONEXCLAMATION);
			return FALSE;
		}
		if (!ReadFile (m_hDrive, lpSectBuff, m_bytesPerSector*wSectors, &bytesread, NULL) )
		{
			//ErrorMessage(GetLastError());
			//AfxMessageBox(_T("CFATDrive::ReadLogicalSectors#ReadFile Failed!!"), MB_OKCANCEL|MB_ICONEXCLAMATION);
			return FALSE;
		}
		return TRUE;
	}
	else
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
		reg.reg_EDX = m_byDriveNo;   // Int 21h, fn 7305h drive numbers are 1-based

		fResult = DeviceIoControl(m_hDrive, VWIN32_DIOC_DOS_DRIVEINFO,
			&reg, sizeof(reg),
			&reg, sizeof(reg), &cb, 0);

		// Determine if the DeviceIoControl call and the read succeeded.
		fResult = fResult && !(reg.reg_Flags & CARRY_FLAG);

		return fResult;
	}
}

BOOL CFATDrive::ReadLogicalSectors16(DWORD dwStartSector, WORD wSectors, LPBYTE lpSectBuff)
{
	if(m_bIsNTandFAT)
	{
		DWORD bytesread, dwRet, dwError;
		LARGE_INTEGER liFilePos;

		liFilePos.QuadPart = (LONGLONG)dwStartSector*m_bytesPerSector;
		dwRet = SetFilePointer(m_hDrive, liFilePos.LowPart, &liFilePos.HighPart, FILE_BEGIN);
		if (dwRet == INVALID_SET_FILE_POINTER && (dwError = GetLastError()) != NO_ERROR )
		{ 
			ErrorMessage(dwError);
			AfxMessageBox(_T("CFATDrive::ReadLogicalSectors16#SetFilePointer Failed!!"), MB_OKCANCEL|MB_ICONEXCLAMATION);
			return FALSE;
		}
		if (!ReadFile(m_hDrive, lpSectBuff, m_bytesPerSector*wSectors, &bytesread, NULL) )
		{
			ErrorMessage(GetLastError());
			AfxMessageBox("CFATDrive::ReadLogicalSectors16#ReadFile Failed!!");
			return FALSE;
		}
		return TRUE;
	}
	else
	{
		BOOL           fResult;
		DWORD          cb;
		DIOC_REGISTERS reg = {0};
		DISKIO         dio = {0};

		dio.dwStartSector = dwStartSector;
		dio.wSectors      = wSectors;
		dio.dwBuffer      = (DWORD)lpSectBuff;

		reg.reg_EAX = m_byDriveNo - 1;    // Int 25h drive numbers are 0-based.
		reg.reg_EBX = (DWORD)&dio;
		reg.reg_ECX = 0xFFFF;        // use DISKIO struct

		fResult = DeviceIoControl(m_hDrive, VWIN32_DIOC_DOS_INT25,
			&reg, sizeof(reg),
			&reg, sizeof(reg), &cb, 0);

		// Determine if the DeviceIoControl call and the read succeeded.
		fResult = fResult && !(reg.reg_Flags & CARRY_FLAG);

		return fResult;
	}
}

BOOL CFATDrive::WriteLogicalSectors(DWORD dwStartSector, WORD wSectors, LPBYTE lpSectBuff)
{
	if(m_bIsNTandFAT)
	{
		DWORD bytesWritten, dwRet, dwError;
		LARGE_INTEGER liFilePos;

		liFilePos.QuadPart = (LONGLONG)dwStartSector*m_bytesPerSector;
		dwRet = SetFilePointer(m_hDrive, liFilePos.LowPart, &liFilePos.HighPart, FILE_BEGIN);
		if (dwRet == INVALID_SET_FILE_POINTER && (dwError = GetLastError()) != NO_ERROR )
		{
			ErrorMessage(dwError);
			AfxMessageBox(_T("CFATDrive::WriteLogicalSectors#SetFilePointer Failed!!"), MB_OKCANCEL|MB_ICONEXCLAMATION);
			return FALSE;
		}

		if (!WriteFile(m_hDrive, lpSectBuff, m_bytesPerSector*wSectors, &bytesWritten, NULL) )
		{
			ErrorMessage(GetLastError());
			AfxMessageBox(_T("CFATDrive::WriteLogicalSectors#WriteFile Failed!!"), MB_OKCANCEL|MB_ICONEXCLAMATION);
			return FALSE;
		}
		return TRUE;
	}
	else
	{
		BOOL           fResult = FALSE;
		BOOL		   bLocked = FALSE;	
		DWORD          cb;
		DIOC_REGISTERS reg = {0};
		DISKIO         dio;

		dio.dwStartSector = dwStartSector;
		dio.wSectors      = wSectors;
		dio.dwBuffer      = (DWORD)lpSectBuff;

		reg.reg_EAX = 0x7305;   // Ext_ABSDiskReadWrite
		reg.reg_EBX = (DWORD)&dio;
		reg.reg_ECX = -1;
		reg.reg_EDX = m_byDriveNo;   // Int 21h, fn 7305h drive numbers are 1-based
		reg.reg_ESI = 0x6001;   // Normal file data (See function
                             // documentation for other values)

		for(BYTE i = 0; i < 4; i++)
		{
			if(LockLogicalVolume(i, 0))
			{
				bLocked = TRUE; 
				break;
			}
		} 
		if(!bLocked)
			return fResult;
		fResult = DeviceIoControl(m_hDrive, VWIN32_DIOC_DOS_DRIVEINFO, &reg, sizeof(reg), &reg, sizeof(reg), &cb, 0);
		fResult = fResult && !(reg.reg_Flags & CARRY_FLAG);
		UnlockLogicalVolume(); 

		return fResult;
	}
}

BOOL CFATDrive::LockLogicalVolume(BYTE bLockLevel, DWORD wPermissions)
{
    BOOL fResult;
	DIOC_REGISTERS reg = {0};  
    BYTE bDeviceCat; //can be either 0x48 or 0x08 
    DWORD cb; 
//Try first with device category 0x48 for FAT32 volumes. If it 
//doesn't work, try again with device category 0x08. If that 
//doesn't work, then the lock failed. 
    bDeviceCat = 0x48; 
ATTEMPT_AGAIN: 
    reg.reg_EAX = 0x440D; 
    reg.reg_EBX = MAKEWORD(m_byDriveNo, bLockLevel);
    reg.reg_ECX = MAKEWORD(0x4A, bDeviceCat);
    reg.reg_EDX = wPermissions;
    fResult = DeviceIoControl(m_hDrive, VWIN32_DIOC_DOS_IOCTL, &reg, sizeof(reg), &reg, sizeof(reg), &cb, 0);
	fResult = fResult && !(reg.reg_Flags & CARRY_FLAG);
    if(!fResult && (bDeviceCat != 0x08))
	{
        bDeviceCat = 0x08; 
        goto ATTEMPT_AGAIN; 
	} 
    return fResult; 
}

BOOL CFATDrive::UnlockLogicalVolume()
{
    BOOL fResult; 
	DIOC_REGISTERS reg = {0};  
    BYTE bDeviceCat; // can be either 0x48 or 0x08 
    DWORD cb; 
// Try first with device category 0x48 for FAT32 volumes. If it 
// doesn't work, try again with device category 0x08. If that 
// doesn't work, then the unlock failed. 
    bDeviceCat = 0x48; 
ATTEMPT_AGAIN: 
    reg.reg_EAX = 0x440D; 
    reg.reg_EBX = m_byDriveNo; 
    reg.reg_ECX = MAKEWORD(0x6A, bDeviceCat); 
    fResult = DeviceIoControl(m_hDrive, VWIN32_DIOC_DOS_IOCTL, &reg, sizeof(reg), &reg, sizeof(reg), &cb, 0);
	fResult = fResult && !(reg.reg_Flags & CARRY_FLAG);
    if(!fResult && (bDeviceCat != 0x08))
	{
        bDeviceCat = 0x08; 
        goto ATTEMPT_AGAIN; 
	} 
    return fResult; 
}

void CFATDrive::SetAttribute(TCHAR *szBuffer, BYTE attri)
{
	_tcscpy_s(szBuffer, 255, "");

	// Set Attribute 
	if(attri & 0x01)
		_tcscat_s(szBuffer, 255, "R-");

	if(attri & 0x02)
		_tcscat_s(szBuffer, 255, "S-");

	if(attri & 0x04)
		_tcscat_s(szBuffer, 255, "H-");

	if(attri & 0x10)
		_tcscat_s(szBuffer, 255, "D-");

	if(attri & 0x20)
		_tcscat_s(szBuffer, 255,"A-");

	if(strlen(szBuffer) != 0)
		szBuffer[strlen(szBuffer)-1] = 0; 
}

void CFATDrive::ErrorMessage(DWORD dwErrorNo)
{
	TCHAR *lpMsgBuf;
	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM,NULL,dwErrorNo, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),(LPTSTR) &lpMsgBuf, 0, NULL);
	AfxMessageBox((LPTSTR)lpMsgBuf, MB_OK|MB_ICONERROR);
	LocalFree(lpMsgBuf);
}