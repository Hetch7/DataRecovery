// NTFSDrive.h: interface for the CNTFSDrive class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_NTFSDRIVE_H__078B2392_2978_4C23_97FD_166C4B234BF3__INCLUDED_)
#define AFX_NTFSDRIVE_H__078B2392_2978_4C23_97FD_166C4B234BF3__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "MFTRecord.h"

#pragma pack(push, curAlignment)

#pragma pack(1)

#define SAME_PARTITION 41L
#define TOO_MUCH_MFT 42L
#define BROKEN_MFT_RECORD 44L
#define MEMORY_ALLOCATE_LIMIT 45L
#define TOO_LONG_FILENAME 46L

////////////////////////////  boot sector info ///////////////////////////////////////
struct NTFS_PART_BOOT_SEC
{
	char		chJumpInstruction[3];
	char		chOemID[4];
	char		chDummy[4];
	
	struct NTFS_BPB
	{
		WORD		wBytesPerSec;
		BYTE		uchSecPerClust;
		WORD		wReservedSec;
		BYTE		uchReserved[3];
		WORD		wUnused1;
		BYTE		uchMediaDescriptor;
		WORD		wUnused2;
		WORD		wSecPerTrack;
		WORD		wNumberOfHeads;
		DWORD		dwHiddenSec;
		DWORD		dwUnused3;
		DWORD		dwUnused4;
		LONGLONG	n64TotalSec;
		LONGLONG	n64MFTLogicalClustNum;
		LONGLONG	n64MFTMirrLogicalClustNum;
		int			nClustPerMFTRecord;
		int			nClustPerIndexRecord;
		LONGLONG	n64VolumeSerialNum;
		DWORD		dwChecksum;
	} bpb;

	char		chBootstrapCode[426];
	WORD		wSecMark;
	BYTE    padding[512*7]; /* for dvd-ram drive */
};
/////////////////////////////////////////////////////////////////////////////////////////
#pragma pack(pop, curAlignment)


class CNTFSDrive  
{
protected:
//////////////// physical drive info/////////////
	DWORD m_dwStartSector;
	bool m_bInitialized;
	bool m_bSamePartition;
	DWORD m_dwBytesPerSector;

	int LoadMFT(LONGLONG nStartCluster, DWORD &nTotalFiles, bool bMirrorRead = false);
/////////////////// the MFT info /////////////////
	BYTE *m_puchMFT;  /// the var to hold the loaded entire MFT
	BYTE *m_puchMFTRecord; // 1K, or the cluster size, whichever is larger

public:
	HANDLE	m_hDrive;
	DWORD m_dwMFTLen; // size of MFT
	DWORD m_dwBytesPerCluster;
	DWORD m_dwMFTRecordSz; // MFT record size
	DWORD m_dwFullScanClstNo; // claster index for full scan 
	DWORD m_dwTotalClstNo; // total claster number of the drive
	MFT_CHUNK_INFO m_stMtfChunk;
	BOOL m_bFullScaned;
	int m_nMFTchunkindex;

	struct ST_FILEINFO // this struct is to retrieve the file detail from this class
	{
		char szFilename[_MAX_PATH]; // file name
		char szParentDirName[_MAX_PATH]; // parent directory name
		LONGLONG	n64Create;		// Creation time
		LONGLONG	n64Modify;		// Last Modify time
		LONGLONG	n64Modfil;		// Last modify of record
		LONGLONG	n64Access;		// Last Access time
		DWORD		dwAttributes;	// file attribute updated
		DWORD		dwAttributesD;	// file attribute + directory attri
		LONGLONG	n64Size;		// file size
		bool 		bDeleted;		// if true then its deleted file
	};

	int DeleteFile(DWORD nFileSeq, DWORD nClusterNo, DWORD nClusterOffset);
	int GetFileDetail(DWORD nFileSeq, ST_FILEINFO &stFileInfo, bool bGetDirPath = true);
	VOID GetParentDir(LONGLONG nFileSeq, CString &strDirPath);
	int Read_File_Write(DWORD nFileSeq, HANDLE nNewFile, LONGLONG iFileSize, CProgressCtrl *pProgDataRecovery);
	
	void SetDriveHandle(HANDLE hDrive);
	void SetStartSector(DWORD dwStartSector, DWORD dwBytesPerSector, bool bSameDrive);

	int Initialize(DWORD &nTotalFiles);
	int MakeMFT(BYTE * puchMFTClst);
	CNTFSDrive();
	virtual ~CNTFSDrive();
};

#endif // !defined(AFX_NTFSDRIVE_H__078B2392_2978_4C23_97FD_166C4B234BF3__INCLUDED_)
