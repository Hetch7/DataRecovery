// NTFSDrive.cpp: implementation of the CNTFSDrive class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "NTFSDrive.h"
#include "MFTRecord.h"
#include "resource.h"
#include "LoadMFTDlg.h"
#include "Winioctl.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CNTFSDrive::CNTFSDrive()
{
	m_bInitialized = false;
	
	m_hDrive = 0;
	m_dwBytesPerCluster = 0;
	m_dwBytesPerSector = 0;

	m_puchMFTRecord = 0;
	m_dwMFTRecordSz = 0;

	m_puchMFT = 0;
	m_dwMFTLen = 0;

	m_dwStartSector = 0;
	m_nMFTchunkindex = 0;

	for(int i = 0; i < MFTCHUNK_MAX; i++)
	{
		m_stMtfChunk.n64MFTClstLen[i] = 0;
		m_stMtfChunk.n64MFTClstOffset[i] = 0;
	}
}

CNTFSDrive::~CNTFSDrive()
{
	if(m_puchMFT)
		delete[] m_puchMFT;
	m_puchMFT = 0;
	m_dwMFTLen = 0;

	if(m_puchMFTRecord)
		delete[] m_puchMFTRecord;
	m_dwMFTRecordSz = 0;

	// memory leak check
	_CrtSetDbgFlag ( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
	_CrtDumpMemoryLeaks();
}

void CNTFSDrive::SetDriveHandle(HANDLE hDrive)
{
	m_hDrive = hDrive;
	m_bInitialized = false;
	m_nMFTchunkindex = 0;
}

// this is necessary to start reading a logical drive 
void CNTFSDrive::SetStartSector(DWORD dwStartSector, DWORD dwBytesPerSector, bool bSameDrive)
{
	if(bSameDrive && m_dwStartSector == dwStartSector)
		m_bSamePartition = true;
	else
		m_bSamePartition = false;
	m_dwStartSector = dwStartSector;
	m_dwBytesPerSector = dwBytesPerSector;
}

// initialize will read the MFT record
///   and passes to the LoadMFT to load the entire MFT in to the memory
int CNTFSDrive::Initialize(DWORD &nTotalFiles)
{
	// if it is the same partition as previous one, we needn't initialize
	if(m_bSamePartition && !m_bFullScaned)
	{
		m_bInitialized = true;
		return SAME_PARTITION;
	}

	CString	cstrMessage;
	NTFS_PART_BOOT_SEC ntfsBS;
	DWORD dwBytes, dwRet, dwError;
	LARGE_INTEGER n64StartPos;

	n64StartPos.QuadPart = (ULONGLONG)m_dwBytesPerSector*m_dwStartSector;

	// point the starting NTFS volume sector in the physical drive
	dwRet = SetFilePointer(m_hDrive,n64StartPos.LowPart,&n64StartPos.HighPart,FILE_BEGIN);
	if (dwRet == INVALID_SET_FILE_POINTER && (dwError = GetLastError()) != NO_ERROR )
	{ 
		AfxMessageBox(_T("CNTFSDrive::Initialize#SetFilePointer Failed!"), MB_OKCANCEL|MB_ICONEXCLAMATION);
	} // End of error handler 

	// Read the boot sector for the MFT infomation
	int nRet = ReadFile(m_hDrive,&ntfsBS,sizeof(NTFS_PART_BOOT_SEC),&dwBytes,NULL);
	if(!nRet)
	{
		return GetLastError();
	}

	if(memcmp(ntfsBS.chOemID,"NTFS",4)) // check whether it is realy ntfs
		return ERROR_INVALID_DRIVE;

	m_dwBytesPerSector = ntfsBS.bpb.wBytesPerSec;
	m_dwBytesPerCluster = ntfsBS.bpb.uchSecPerClust * ntfsBS.bpb.wBytesPerSec;
	m_dwTotalClstNo = ntfsBS.bpb.n64TotalSec / ntfsBS.bpb.uchSecPerClust;

	if(m_puchMFTRecord)
		delete[] m_puchMFTRecord;

	if((char)ntfsBS.bpb.nClustPerMFTRecord < 0)
		m_dwMFTRecordSz = 0x01<<((-1)*((char)ntfsBS.bpb.nClustPerMFTRecord));
	else
		m_dwMFTRecordSz = (char)ntfsBS.bpb.nClustPerMFTRecord * ntfsBS.bpb.uchSecPerClust * ntfsBS.bpb.wBytesPerSec;

	m_puchMFTRecord = new BYTE[m_dwMFTRecordSz];
	if(!m_puchMFTRecord)
	{
		cstrMessage.LoadString(IDS_ERRMSG_ALLOCATE_MFT_MEMORY);
		AfxMessageBox(cstrMessage, MB_OK|MB_ICONEXCLAMATION);
		return GetLastError();
	}

	m_bInitialized = true;

	CLoadMFTDlg* loadMFTDlg = new CLoadMFTDlg(1);
	loadMFTDlg->Create(IDD_LOADMFT); // show please-wait dialog
	nRet = LoadMFT(ntfsBS.bpb.n64MFTLogicalClustNum, nTotalFiles);
	loadMFTDlg->DestroyWindow();

	if(nRet)
	{
		if(nRet == ERROR_FILE_NOT_FOUND) //broken MFT so load Mirror-MTF
		{
			loadMFTDlg = new CLoadMFTDlg(1);
			loadMFTDlg->Create(IDD_LOADMFT); // show please-wait dialog
			nRet = LoadMFT(ntfsBS.bpb.n64MFTMirrLogicalClustNum, nTotalFiles, true);
			loadMFTDlg->DestroyWindow();
			if(nRet)
			{
				m_bInitialized = false;
				return nRet;
			}
			else
				return ERROR_SUCCESS;
		}
		m_bInitialized = false;
		return nRet;
	}

	return ERROR_SUCCESS;
}

//// nStartCluster is the MFT table starting cluster
///    the first entry of record in MFT table will always have the MFT record of itself
int CNTFSDrive::LoadMFT(LONGLONG nStartCluster, DWORD &nTotalFiles, bool bMirrorRead)
{
	DWORD dwBytes, dwError, dwRet;
	int nRet;
	LARGE_INTEGER n64Pos;

	if(!m_bInitialized)
		return ERROR_INVALID_ACCESS;

	CMFTRecord cMFTRec;
	
	size_t converted;
	wchar_t uszMFTName[10];
	mbstowcs_s(&converted, uszMFTName, 10, "$MFT", 4);

	// NTFS starting point
	n64Pos.QuadPart = (ULONGLONG)m_dwBytesPerSector*m_dwStartSector;
	// MFT starting point
	n64Pos.QuadPart += (ULONGLONG)nStartCluster*m_dwBytesPerCluster;
	
	dwRet = SetFilePointer(m_hDrive,n64Pos.LowPart,&n64Pos.HighPart,FILE_BEGIN);
	if (dwRet == INVALID_SET_FILE_POINTER && (dwError = GetLastError()) != NO_ERROR )
	{
		AfxMessageBox("CNTFSDrive::LoadMFT#SetFilePointer Failed!!");
		return dwError;
	}

	/// reading the first record in the NTFS table.
	//   the first record in the NTFS is always MFT record
	nRet = ReadFile(m_hDrive,m_puchMFTRecord,m_dwMFTRecordSz,&dwBytes,NULL);
	if(!nRet)
	{
		if(bMirrorRead)
			return GetLastError();
		else
			return ERROR_FILE_NOT_FOUND;
	}

	// now extract the MFT record just like the other MFT table records
	cMFTRec.SetDriveHandle(m_hDrive);
	cMFTRec.SetRecordInfo((LONGLONG)m_dwStartSector*m_dwBytesPerSector, m_dwMFTRecordSz,m_dwBytesPerCluster);

	nRet = cMFTRec.ExtractFile(m_puchMFTRecord,dwBytes, &m_stMtfChunk, &m_nMFTchunkindex);
	if(nRet)
		return nRet;

	if(memcmp(cMFTRec.m_attrFilename.wFilename,uszMFTName,8))
		return ERROR_BAD_DEVICE; // no MFT file available

	if(m_puchMFT)
	{
		delete[] m_puchMFT;
		m_dwMFTLen = 0;
	}

	// this data(m_puchFileData) is special since it is the data of entire MFT file
	m_dwMFTLen = cMFTRec.m_dwFileDataSz;
	m_puchMFT = cMFTRec.m_puchFileData;

	nTotalFiles = m_dwMFTLen / m_dwMFTRecordSz;
	
	return ERROR_SUCCESS;
}

int CNTFSDrive::MakeMFT(BYTE * puchMFTClst)
{
	if(m_dwMFTLen == 0 || m_dwMFTLen <= m_dwBytesPerCluster * m_dwFullScanClstNo)
	{
		BYTE *puchMFTtemp;
		try
		{
			puchMFTtemp = new BYTE[m_dwMFTLen + m_dwBytesPerCluster * 10000];
		}
		catch(CMemoryException* pe)
		{
			return MEMORY_ALLOCATE_LIMIT;
		}

		memcpy(puchMFTtemp, m_puchMFT, m_dwMFTLen);
		m_dwMFTLen += m_dwBytesPerCluster * 10000;

		if(m_puchMFT)
			delete [] m_puchMFT;

		m_puchMFT = puchMFTtemp;
	}
	memcpy(m_puchMFT + m_dwBytesPerCluster * m_dwFullScanClstNo, puchMFTClst, m_dwBytesPerCluster);
	m_dwFullScanClstNo++;
	return ERROR_SUCCESS;
}

int CNTFSDrive::DeleteFile(DWORD nFileSeq,  DWORD nClusterNo, DWORD nClusterOffset)
{
	DWORD dwRet,dwError,dwBytes =0;
	LARGE_INTEGER n64Pos;

	BYTE *buf = new BYTE[m_dwMFTRecordSz];
	ZeroMemory(buf, m_dwMFTRecordSz);

	////////////// 完全スキャンでリストアップされたファイルの消去
	if(nClusterNo != -1)
	{
		BYTE * puchMFTClst = new BYTE[m_dwBytesPerCluster];
		n64Pos.QuadPart = (ULONGLONG)m_dwBytesPerCluster*nClusterNo + nClusterOffset*m_dwMFTRecordSz;

		dwRet = SetFilePointer(m_hDrive, n64Pos.LowPart, &n64Pos.HighPart, FILE_BEGIN);
		if (dwRet == INVALID_SET_FILE_POINTER && (dwError = GetLastError()) != NO_ERROR )
		{ 
			return dwError;
		}

		// ディスク上のMFTレコードに直接NULLを書き込む
		dwRet = WriteFile(m_hDrive, buf, m_dwMFTRecordSz, &dwBytes, NULL);
		if(!dwRet || dwBytes != m_dwMFTRecordSz)
		{
			delete[] buf;
			delete[] puchMFTClst;
			return GetLastError();
		}
		delete[] buf;
		delete[] puchMFTClst;
		return ERROR_SUCCESS;
	}
	

	//////////// 通常スキャンでリストアップされたファイルの消去
	if(!m_bInitialized)
	{
		delete[] buf;
		return ERROR_INVALID_ACCESS;
	}
	if((nFileSeq*m_dwMFTRecordSz+m_dwMFTRecordSz) > m_dwMFTLen)
	{
		delete[] buf;
		return ERROR_NO_MORE_FILES;	
	}
	//if(m_stMtfChunk.n64MFTClstLen[0] == 0)
	//{
	//	delete[] buf;
	//	return TOO_MUCH_MFT;
	//}

	int nMFTchunkindex = 0;
	LONGLONG n64Offset = nFileSeq*m_dwMFTRecordSz;
	LONGLONG n64TotalSize = m_stMtfChunk.n64MFTClstLen[nMFTchunkindex]*m_dwBytesPerCluster;
	LONGLONG n64TotalOffset = m_stMtfChunk.n64MFTClstOffset[nMFTchunkindex];
	while(nFileSeq*m_dwMFTRecordSz > n64TotalSize)
	{
		n64Offset = nFileSeq*m_dwMFTRecordSz - n64TotalSize;
		nMFTchunkindex++;
		n64TotalSize += m_stMtfChunk.n64MFTClstLen[nMFTchunkindex]*m_dwBytesPerCluster;
		n64TotalOffset += m_stMtfChunk.n64MFTClstOffset[nMFTchunkindex];
	}
	n64Offset += n64TotalOffset*m_dwBytesPerCluster;

	n64Pos.QuadPart = n64Offset;
	n64Pos.QuadPart += (LONGLONG)m_dwStartSector*m_dwBytesPerSector;

	//// メモリ上のMFTからファイルレコードを消去
	memcpy(&m_puchMFT[nFileSeq*m_dwMFTRecordSz], buf, m_dwMFTRecordSz);

	dwRet = SetFilePointer(m_hDrive,n64Pos.LowPart,&n64Pos.HighPart,FILE_BEGIN);
	if (dwRet == INVALID_SET_FILE_POINTER && (dwError = GetLastError()) != NO_ERROR ) //changed!!
	{ 
		AfxMessageBox(_T("CNTFSDrive::DeleteFile#SetFilePointer Failed!!"), MB_OKCANCEL|MB_ICONEXCLAMATION); //for debug
		return dwError;
	}

	// ディスク上のMFTからファイルレコードを消去
	dwRet = WriteFile(m_hDrive,buf,m_dwMFTRecordSz,&dwBytes,NULL);
	if(!dwRet || dwBytes != m_dwMFTRecordSz)
	{
		if(buf)
			delete[] buf;
		return GetLastError();
	}

	if(buf)
		delete[] buf;

	return ERROR_SUCCESS;
}

int CNTFSDrive::Read_File_Write(DWORD nFileSeq, HANDLE hNewFile, LONGLONG iFileSize, CProgressCtrl *pProgDataRecovery)
{
	int nRet;
	
	if(!m_bInitialized)
		return ERROR_INVALID_ACCESS;
	
	CMFTRecord cFile;
	cFile.m_bMTFLoaded = true;

	// point the record of the file in the MFT table
	memcpy(m_puchMFTRecord,&m_puchMFT[nFileSeq*m_dwMFTRecordSz],m_dwMFTRecordSz);

	// Then extract that file from the drive
	cFile.SetDriveHandle(m_hDrive);
	cFile.SetRecordInfo((LONGLONG)m_dwStartSector*m_dwBytesPerSector, m_dwMFTRecordSz,m_dwBytesPerCluster);
	nRet = cFile.ExtractFileAndWrite(m_puchMFTRecord,m_dwMFTRecordSz,hNewFile,iFileSize,pProgDataRecovery);
	if(nRet)
		return nRet;

	return ERROR_SUCCESS;
}

int CNTFSDrive::GetFileDetail(DWORD nFileSeq, ST_FILEINFO &stFileInfo, bool bGetDirPath)
{
	int nRet;

	if(!m_bInitialized)
		return ERROR_INVALID_ACCESS;

	if((nFileSeq*m_dwMFTRecordSz+m_dwMFTRecordSz) > m_dwMFTLen)
		return ERROR_NO_MORE_FILES;

	CMFTRecord cFile;
	cFile.m_bMTFLoaded = true;

	// point the record of the file in the MFT table
	memcpy(m_puchMFTRecord,&m_puchMFT[nFileSeq*m_dwMFTRecordSz],m_dwMFTRecordSz);

	// read the only file detail not the file data
	cFile.SetDriveHandle(m_hDrive);
	cFile.SetRecordInfo((LONGLONG)m_dwStartSector*m_dwBytesPerSector, m_dwMFTRecordSz,m_dwBytesPerCluster);
	nRet = cFile.ExtractFile(m_puchMFTRecord,m_dwMFTRecordSz, NULL, NULL, true);
	if(nRet)
		return nRet;

	// set the struct and pass the struct of file detail
	memset(&stFileInfo,0,sizeof(ST_FILEINFO));
	CString strFileName = (LPCWSTR)cFile.m_attrFilename.wFilename;

	if(strFileName.GetLength() >= _MAX_PATH)
		return TOO_LONG_FILENAME;

	_tcscpy_s(stFileInfo.szFilename, _MAX_PATH, strFileName);
	
	stFileInfo.dwAttributesD = cFile.m_attrFilename.dwFlags; // + Dir attri
	stFileInfo.dwAttributes = cFile.m_attrStandard.dwFATAttributes; //updated

	stFileInfo.n64Create = cFile.m_attrStandard.n64Create;
	stFileInfo.n64Modify = cFile.m_attrStandard.n64Modify;
	stFileInfo.n64Access = cFile.m_attrStandard.n64Access;
	stFileInfo.n64Modfil = cFile.m_attrStandard.n64Modfil;

	if(cFile.m_ntfsAttr.uchNonResFlag)
	{
		if(cFile.m_ntfsAttr.Attr.NonResident.n64RealSize == 0)
			stFileInfo.n64Size = cFile.m_attrFilename.n64RealSize;
		else
			stFileInfo.n64Size = cFile.m_ntfsAttr.Attr.NonResident.n64AllocSize;
	}
	else
		stFileInfo.n64Size = cFile.m_ntfsAttr.Attr.Resident.dwLength;

	// extract parent directory path
	CString strDirPath = "";
	if(bGetDirPath)
	{
		GetParentDir(cFile.m_attrFilename.dwMftParentDir, strDirPath);
		strDirPath.TrimLeft(_T("\\"));
	}
	else
	{
		strDirPath.LoadString(IDS_FOLDER_UNKNOWN);
	}

	if(strDirPath.GetLength() >= _MAX_PATH)
		return TOO_LONG_FILENAME;

	_tcscpy_s(stFileInfo.szParentDirName, _MAX_PATH, strDirPath); //(LPCTSTR)

	return ERROR_SUCCESS;
}

VOID CNTFSDrive::GetParentDir(LONGLONG nFileSeq, CString &strDirPath)
{
	CMFTRecord cFile;
	static LONGLONG dwPrevDir;

	// point the record of the file in the MFT table
	memcpy(m_puchMFTRecord,&m_puchMFT[nFileSeq*m_dwMFTRecordSz],m_dwMFTRecordSz);
	cFile.ExtractDir(m_puchMFTRecord,m_dwMFTRecordSz);

	if(dwPrevDir == cFile.m_attrFilename.dwMftParentDir)
	{
		dwPrevDir = 0;
		return;
	}

	//directory only
	if(cFile.m_attrFilename.dwFlags&0x10000000)
	{
		CString strDirName = (LPCWSTR)cFile.m_attrFilename.wFilename;
		strDirPath.Insert(0,strDirName);
		strDirPath.Insert(0,"\\");
		dwPrevDir = cFile.m_attrFilename.dwMftParentDir;
		GetParentDir(cFile.m_attrFilename.dwMftParentDir, strDirPath);
	}
	else
	{
		CString cstrTitle;
		cstrTitle.LoadString(IDS_FOLDER_UNKNOWN);
		strDirPath.Insert(0,cstrTitle);
		dwPrevDir = 0;
		return;
	}
}
