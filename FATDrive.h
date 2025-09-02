#pragma once

#define VWIN32_DIOC_DOS_IOCTL     1
#define VWIN32_DIOC_DOS_INT25     2
#define VWIN32_DIOC_DOS_INT26     3
#define VWIN32_DIOC_DOS_DRIVEINFO 6

#define ERROR_UNKNOWN_FILESYSTEM 42L
#define ERROR_FAT_READ_FAILURE 43L

typedef struct _DIOC_REGISTERS {
	DWORD reg_EBX;
	DWORD reg_EDX;
	DWORD reg_ECX;
	DWORD reg_EAX;
	DWORD reg_EDI;
	DWORD reg_ESI;
	DWORD reg_Flags;
} DIOC_REGISTERS, *PDIOC_REGISTERS;

#define CARRY_FLAG 1

#pragma pack(1)
typedef struct _DISKIO {
	DWORD  dwStartSector;   // starting logical sector number
	WORD   wSectors;        // number of sectors
	DWORD  dwBuffer;        // address of read/write buffer
} DISKIO, * PDISKIO;
#pragma pack()

typedef union BPBlock_t {
    /* FAT16 or FAT12 BPB */
    struct FAT16BPB_t {
        BYTE    jmpOpeCode[3];          /* 0xeb ?? 0x90 */
        char    OEMName[8];
        /* FAT16 */
        BYTE    bytesPerSector[2];      /* bytes/sector */
        BYTE    sectorsPerCluster;      /* sectors/cluster */
        BYTE    reservedSectors[2];     /* reserved sector, beginning with sector 0 */
        BYTE    numberOfFATs;           /* file allocation table */
        BYTE    rootEntries[2];         /* root entry (512) */
        BYTE    totalSectors[2];        /* partion total secter */
        BYTE    mediaDescriptor;        /* 0xf8: Hard Disk */
        BYTE    sectorsPerFAT[2];       /* sector/FAT (FAT32 always zero: see bigSectorsPerFAT) */
        BYTE    sectorsPerTrack[2];     /* sector/track (not use) */
        BYTE    heads[2];               /* heads number (not use) */
        BYTE    hiddenSectors[4];       /* hidden sector number */
        BYTE    bigTotalSectors[4];     /* total sector number */
        /* info */
        BYTE    driveNumber;
        BYTE    unused;
        BYTE    extBootSignature;
        BYTE    serialNumber[4];
        BYTE    volumeLabel[11];
        char    fileSystemType[8];      /* "FAT16   " */
        BYTE    loadProgramCode[448];
        BYTE    sig[2];                 /* 0x55, 0xaa */
    }    fat16;
    /* FAT32 BPB */
    struct FAT32BPB_t {
        BYTE    jmpOpeCode[3];          /* 0xeb ?? 0x90 */
        char    OEMName[8];
        /* FAT32 */
        BYTE    bytesPerSector[2];
        BYTE    sectorsPerCluster;
        BYTE    reservedSectors[2];
        BYTE    numberOfFATs;
        BYTE    rootEntries[2];
        BYTE    totalSectors[2];
        BYTE    mediaDescriptor;
        WORD    sectorsPerFAT;
        BYTE    sectorsPerTrack[2];
        BYTE    heads[2];
        BYTE    hiddenSectors[4];
        BYTE    bigTotalSectors[4];
        BYTE    bigSectorsPerFAT[4];    /* sector/FAT for FAT32 */
        BYTE    extFlags[2];            /* use index zero (follows) */
                                        /* bit 7      0: enable FAT mirroring, 1: disable mirroring */
                                        /* bit 0-3    active FAT number (bit 7 is 1) */
        BYTE    FS_Version[2];
        BYTE    rootDirStrtClus[4];     /* root directory cluster */
        BYTE    FSInfoSec[2];           /* 0xffff: no FSINFO, other: FSINFO sector */
        BYTE    bkUpBootSec[2];         /* 0xffff: no back-up, other: back up boot sector number */
        BYTE    reserved[12];
        /* info */
        BYTE    driveNumber;
        BYTE    unused;
        BYTE    extBootSignature;
        BYTE    serialNumber[4];
        BYTE    volumeLabel[11];
        char    fileSystemType[8];      /* "FAT32   " */
        BYTE    loadProgramCode[420];
        BYTE    sig[2];                 /* 0x55, 0xaa */
    }    fat32;
	BYTE    padding[512*4]; /* for MO drive */
}   BPBlock;

typedef struct DirEntry_t {
	BYTE    name[8];            /* file name */
	BYTE    extension[3];       /* file name extension */
	BYTE    attribute;          /* file attribute
								bit 4    directory flag
								bit 3    volume flag
								bit 2    hidden flag
								bit 1    system flag
								bit 0    read only flag */
	BYTE    reserved;           /* use NT or same OS */
	BYTE    createTimeMs;       /* VFAT で使用するファイル作成時刻の10ミリ秒 (0 ～ 199) */
	BYTE    createTime[2];      /* VFAT で使用するファイル作成時間 */
	BYTE    createDate[2];      /* VFAT で使用するファイル作成日付 */
	BYTE    accessDate[2];      /* VFAT で使用するファイル・アクセス日付 */
	BYTE    clusterHighWord[2]; /* FAT32 で使用するクラスタ番号の上位 16 bits */
	BYTE    updateTime[2];
	BYTE    updateDate[2];
	BYTE    cluster[2];         /* start cluster number */
	BYTE    fileSize[4];        /* file size in bytes (directory is always zero) */
}   DirEntry;

typedef struct LNFEntry_t {
	BYTE    LFNSeqNumber;           /* LNF recode sequence number
									bit 7        削除されると 1 になる
									bit 6        0: LNF は続く、1: LNF の最後のシーケンス
									bit 5 ～ 0   LNF の順番を表す (1 ～ 63) */
	BYTE    name1st[10];            /* Unicode 5 characters */
	BYTE    attribute;              /* file attribute (always 0x0f) */
	BYTE    reserved;               /* always 0x00 */
	BYTE    checkCodeForShortName;  /* DOS 名のチェック・コード */
	BYTE    name2nd[12];            /* Unicode 6 characters */
	BYTE    cluster[2];             /* always 0x0000 */
	BYTE    name3rd[4];             /* Unicode 2 characters */
}   LNFEntry;


class CFATDrive
{
protected:
	BYTE	m_byDriveNo;
	BPBlock m_BPBlock;
	DirEntry m_DirEntry;
	LNFEntry m_LNFEntry;
	CListCtrl *m_pList;
	CEdit *m_pEditDelFiles;
	CProgressCtrl *m_pProgSearch;
	CString m_cstrFileNamePart;
	bool m_bIsFAT16;
	bool m_bIsFAT12;
	bool m_bIsNTandFAT;

	WORD    m_bytesPerSector;
	WORD    m_rootEntries;
	BYTE    m_sectorsPerCluster;
	BYTE    m_numberOfFATs;
	WORD    m_reservedSectors;
	DWORD   m_SectorsPerFAT;
	DWORD   m_rootDirStrtClus;
	DWORD   m_dwBytePerCluster;
	DWORD   m_dwRootDirSize;
	DWORD   m_dwUserDataOffset;
	
	BYTE *m_puchFAT;

	BOOL LockLogicalVolume(BYTE bLockLevel, DWORD wPermissions);
	BOOL UnlockLogicalVolume();
	void ErrorMessage(DWORD dwErrorNo);
public:
	HANDLE	m_hDrive;
	CFATDrive(void);
	~CFATDrive(void);
	void SetDrive(HANDLE hDev, BYTE byDriveNo, bool bIsNTandFAT, CListCtrl* pList, CEdit *pEditDelFiles, CProgressCtrl *pProgSearch);
	BOOL ReadLogicalSectors(DWORD dwStartSector, WORD wSectors, LPBYTE lpSectBuff);
	BOOL ReadLogicalSectors16(DWORD dwStartSector, WORD wSectors, LPBYTE lpSectBuff);
	BOOL WriteLogicalSectors(DWORD dwStartSector, WORD wSectors, LPBYTE lpSectBuff);
	int LoadFAT(void);
	void BeginScan(CString cstrFileNamePart);
	void ScanDirectory32(DWORD dwDirStrtClus, CString &cstrPathName, bool bRootDir = false, bool bDeletedDir = false);
	void ScanDirectory12(WORD dwDirStrtClus, CString &cstrPathName, bool bRootDir = false, bool bDeletedDir = false);
	void ScanDirectory16(WORD dwDirStrtClus, CString &cstrPathName, bool bRootDir = false, bool bDeletedDir = false);
//	int RecoveryFile(HANDLE hNewFile, DWORD dwStartCluster, int iFileSize, CProgressCtrl *pProgDataRecovery);
	int RecoveryFile(HANDLE hNewFile, DWORD dwStartCluster, LONGLONG iFileSize, CProgressCtrl *pProgDataRecovery);
	int DeleteFile(DWORD dwStartCluster, int iFileSize, DWORD dwClusterNo, DWORD dwClusterOffset, CProgressCtrl *pProgDataRecovery);
	void SetAttribute(TCHAR *szBuffer, BYTE attri);
	void ScanMore12(CString &cstrPathName);
	void ScanMore16(CString &cstrPathName);
	void ScanMore32(CString &cstrPathName);
	BOOL ScanInEmptyCluster(CString &cstrPathName, DWORD nClusterNo);

};
