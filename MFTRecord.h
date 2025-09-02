// MFTRecord.h: interface for the CMFTRecord class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_MFTRECORD_H__04A1B8DF_0EB0_4B72_8587_2703342C5675__INCLUDED_)
#define AFX_MFTRECORD_H__04A1B8DF_0EB0_4B72_8587_2703342C5675__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#pragma pack(push, curAlignment)

#pragma pack(1)

#define FILE_IN_USE 40L
#define TOO_MUCH_MFT 42L
#define MFT_OVERFLOW 43L
#define BROKEN_MFT_RECORD 44L

////////////////////////////MFT record header and attribute header //////////////////////
struct NTFS_MFT_FILE
{
	char		szSignature[4];		// Signature "FILE"
	WORD		wFixupOffset;		// offset to fixup pattern
	WORD		wFixupSize;			// Size of fixup-list +1
	LONGLONG	n64LogSeqNumber;	// log file seq number
	WORD		wSequence;			// sequence nr in MFT
	WORD		wHardLinks;			// Hard-link count
	WORD		wAttribOffset;		// Offset to seq of Attributes
	WORD		wFlags;				// 0x01 = NonRes; 0x02 = Dir
	DWORD		dwRecLength;		// Real size of the record
	DWORD		dwAllLength;		// Allocated size of the record
	LONGLONG	n64BaseMftRec;		// ptr to base MFT rec or 0
	WORD		wNextAttrID;		// Minimum Identificator +1
	WORD		wFixupPattern;		// Current fixup pattern
	DWORD		dwMFTRecNumber;		// Number of this MFT Record
								// followed by resident and
								// part of non-res attributes
};

typedef struct	// if resident then + RESIDENT
{					//  else + NONRESIDENT
	DWORD	dwType;
	DWORD	dwFullLength;
	BYTE	uchNonResFlag;
	BYTE	uchNameLength;
	WORD	wNameOffset;
	WORD	wFlags;
	WORD	wID;

	union ATTR
	{
		struct RESIDENT
		{
			DWORD	dwLength;
			WORD	wAttrOffset;
			BYTE	uchIndexedTag;
			BYTE	uchPadding;
		} Resident;

		struct NONRESIDENT
		{
			LONGLONG	n64StartVCN;
			LONGLONG	n64EndVCN;
			WORD		wDatarunOffset;
			WORD		wCompressionSize; // compression unit size
			BYTE		uchPadding[4];
			LONGLONG	n64AllocSize;
			LONGLONG	n64RealSize;
			LONGLONG	n64StreamSize;
			// data runs...
		}NonResident;
	}Attr;
} NTFS_ATTRIBUTE; // standard attribute header

//////////////////////////////////////////////////////////////////////////////////////////

///////////////////////// Attributes /////////////////////////////////////////////////////
typedef struct
{
	LONGLONG	n64Create;		// Creation time
	LONGLONG	n64Modify;		// Last Modify time
	LONGLONG	n64Modfil;		// Last modify of record
	LONGLONG	n64Access;		// Last Access time
	DWORD		dwFATAttributes;// As FAT + 0x800 = compressed
	DWORD		dwReserved1;	// unknown

} ATTR_STANDARD;   
  
typedef struct
{
	LONGLONG	dwMftParentDir;            // Seq-nr parent-dir MFT entry
	LONGLONG	n64Create;                  // Creation time
	LONGLONG	n64Modify;                  // Last Modify time
	LONGLONG	n64Modfil;                  // Last modify of record
	LONGLONG	n64Access;                  // Last Access time
	LONGLONG	n64Allocated;               // Allocated disk space
	LONGLONG	n64RealSize;                // Size of the file
	DWORD		dwFlags;					// attribute
	DWORD		dwEAsReparsTag;				// Used by EAs and Reparse
	BYTE		chFileNameLength;
	BYTE		chFileNameType;            // 8.3 / Unicode
	WORD		wFilename[512];             // Name (in Unicode ?)

}ATTR_FILENAME; 
//////////////////////////////////////////////////////////////////////////////////////////

///////////////////////// $EFS Attribute /////////////////////////////////////////////////////
typedef struct
{
	BYTE	unknown1[16];
	BYTE	unknown2[16];
	BYTE	unknown3[16];
	BYTE	unknown4[16];
	BYTE	unknown5[2];
}EFS_HEADER;

typedef struct
{
	BYTE	unknown1[6];
	BYTE	unknown2[16];
	BYTE	unknown3[16];
	BYTE	unknown4[4];
	BYTE	token[4];
	BYTE	unknown5[8];
	BYTE	unknown6[16];
	DWORD	filesize1;
	DWORD	filesize2;
	BYTE	unknown7[8];
	DWORD	encryptedsize;
	BYTE	unknown8[12];
	BYTE	unknown9[452];
}EFS_TRAILER;   

typedef struct
{
	BYTE	token[4];
	BYTE	unknown1[8];
	BYTE	unknown2[6];
	WORD	seq_number;
	BYTE	unknown3[8];
	DWORD	filesize1;
	DWORD	filesize2;
	BYTE	unknown4[8];
	DWORD	encryptedsize;
	BYTE	unknown5[12];
	BYTE	unknown6[452];
}EFS_PADDING;   

static const EFS_HEADER g_efsHeader = 
	{{0x00, 0x01, 0x00, 0x00, 0x52, 0x00, 0x4f, 0x00, 0x42, 0x00, 0x53, 0x00, 0x00, 0x00, 0x00, 0x00},
	{0x00, 0x00, 0x00, 0x00, 0x1e, 0x00, 0x00, 0x00, 0x4e, 0x00, 0x54, 0x00, 0x46, 0x00, 0x53, 0x00},
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00},
	{0x10, 0x19, 0x28, 0x02, 0x00, 0x00, 0x47, 0x00, 0x55, 0x00, 0x52, 0x00, 0x45, 0x00, 0x00, 0x00},
	{0x00,0x00}};
static EFS_TRAILER g_efsTrailer = 
	{{0x2a, 0x00, 0x00, 0x00, 0x4e, 0x00},
	{0x54, 0x00, 0x46, 0x00, 0x53, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	{0x00, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x3a, 0x00, 0x3a, 0x00, 0x24, 0x00, 0x44, 0x00, 0x41, 0x00},
	{0x54, 0x00, 0x41, 0x00},{0x00, 0x40, 0x00, 0x00},{0x47, 0x00, 0x55, 0x00, 0x52, 0x00, 0x45, 0x00},
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x01, 0x00, 0x00},
	0,0,{0x00, 0x00, 0x0e, 0x0e, 0x0c, 0x01, 0x01, 0x00},
	0};
static EFS_PADDING g_efsPadding = 
	{{0x00, 0x02, 0x01, 0x00},{0x47, 0x00, 0x55, 0x00, 0x52, 0x00, 0x45, 0x00},
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00},0,{0x00, 0x00, 0x00, 0x00, 0xf0, 0x01, 0x00, 0x00},
	 0x10000,0x10000,{0x00, 0x00, 0x10, 0x10, 0x0c, 0x01, 0x01, 0x00},
	 0x10000};
//////////////////////////////////////////////////////////////////////////////////////////
#pragma pack(pop, curAlignment)

#define MFTCHUNK_MAX 200

typedef struct _MFT_CHUNK_INFO {
	LONGLONG n64MFTClstLen[MFTCHUNK_MAX];
	LONGLONG n64MFTClstOffset[MFTCHUNK_MAX];
} MFT_CHUNK_INFO;

class CMFTRecord  
{

protected:
	HANDLE	m_hDrive;

	BYTE *m_pMFTRecord;
	DWORD m_dwMaxMFTRecSize;
	DWORD m_dwCurPos;
	DWORD m_dwBytesPerCluster;
	LONGLONG m_n64StartPos;	
		
	LONGLONG m_n64MFTClstLen[MFTCHUNK_MAX];
	LONGLONG m_n64MFTClstOffset[MFTCHUNK_MAX];

	int ReadRaw(LONGLONG n64LCN, BYTE *chData, DWORD &dwLen);
	int ReadRawAndWrite(LONGLONG n64LCN, DWORD &dwLen, DWORD dwRemainder, HANDLE nNewFile, BOOL bCompressed, CProgressCtrl *pProgDataRecovery);
	int ExtractData(NTFS_ATTRIBUTE &ntfsAttr, BYTE *&puchData, DWORD &dwDataLen, MFT_CHUNK_INFO *pstMftChunk, int *pnMFTchunkindex);
	int ExtractDataAndWrite(NTFS_ATTRIBUTE ntfsAttr, HANDLE nNewFile, LONGLONG iFileSize, CProgressCtrl *pProgDataRecovery);
	int ExtractFullMFT(NTFS_ATTRIBUTE &ntfsAttr, MFT_CHUNK_INFO *pstMftChunk, int *pnMFTchunkindex);
	void ErrorMessage(DWORD dwErrorNo);

public:
///////// attributes //////////////////////
	ATTR_STANDARD m_attrStandard;
	ATTR_FILENAME m_attrFilename;
	NTFS_ATTRIBUTE	m_ntfsAttr;

	BYTE *m_puchFileData; // collected file data buffer
	DWORD m_dwFileDataSz; // file data size , ie. m_pchFileData buffer length
	DWORD m_dwFileDataRead; // actual file data which has been read

	bool m_bInUse;
	bool m_bMTFLoaded;

	void InsertPadding(BYTE *&puchData, DWORD& nDataSize, LONGLONG nFileSize);

public:
	int SetRecordInfo(LONGLONG n64StartPos, DWORD dwRecSize, DWORD dwBytesPerCluster);
	void SetDriveHandle(HANDLE hDrive);
	
	int ExtractFile(BYTE *puchMFTBuffer, DWORD dwLen, MFT_CHUNK_INFO *pstMftChunk, int *pnMFTchunkindex, bool bExcludeData=false);
	int ExtractDir(BYTE *puchMFTBuffer, DWORD dwLen);
	int ExtractFileAndWrite(BYTE *puchMFTBuffer, DWORD dwLen, HANDLE nNewFile, LONGLONG iFileSize, CProgressCtrl *pProgDataRecovery);

	CMFTRecord();
	virtual ~CMFTRecord();
};

#endif // !defined(AFX_MFTRECORD_H__04A1B8DF_0EB0_4B72_8587_2703342C5675__INCLUDED_)
