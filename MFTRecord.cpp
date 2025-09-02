// MFTRecord.cpp: implementation of the CMFTRecord class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "MFTRecord.h"
#include <winioctl.h>

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

// ntdll.dll function for decompression
#define INIT_NTDLL_ADDRESS( name )\
    (FARPROC&)##name = GetProcAddress( GetModuleHandle( _T("ntdll") ), #name)

#define DEF_NTDLL_PROC( name )\
    (WINAPI *##name)

VOID DEF_NTDLL_PROC(RtlDecompressBuffer)(ULONG,PVOID,ULONG,PVOID,ULONG,PULONG);

#define COMPRESSION_FORMAT_NONE		(0x0000)		// [result:STATUS_INVALID_PARAMETER]
#define COMPRESSION_FORMAT_DEFAULT	(0x0001)		// [result:STATUS_INVALID_PARAMETER]
#define COMPRESSION_FORMAT_LZNT1	(0x0002)
#define COMPRESSION_FORMAT_NS3		(0x0003)		// STATUS_NOT_SUPPORTED
#define COMPRESSION_FORMAT_NS15		(0x000F)		// STATUS_NOT_SUPPORTED
#define COMPRESSION_FORMAT_SPARSE	(0x4000)		// ??? [result:STATUS_INVALID_PARAMETER]

#define COMPRESSION_RATIO	(20)
#define INVALID_SET_FILE_POINTER ((DWORD)-1)
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CMFTRecord::CMFTRecord()
{
	m_hDrive = 0;

	m_dwMaxMFTRecSize = 1024; // usual size
	m_pMFTRecord = 0;
	m_dwCurPos = 0;

	m_puchFileData = 0; // collected file data buffer
	m_dwFileDataSz = 0; // file data size , ie. m_pchFileData buffer length
	m_dwFileDataRead = 0; // actual file data which has been read

	memset(&m_attrStandard,0,sizeof(ATTR_STANDARD));
	memset(&m_attrFilename,0,sizeof(ATTR_FILENAME));

	m_bInUse = false;
	m_bMTFLoaded = false;
}

CMFTRecord::~CMFTRecord()
{
	m_puchFileData = 0;
	m_dwFileDataSz = 0;
}

// set the drive handle
void CMFTRecord::SetDriveHandle(HANDLE hDrive)
{
	m_hDrive = hDrive;
}

// set the detail
//  n64StartPos is the byte from the starting of the physical disk
//  dwRecSize is the record size in the MFT table
//  dwBytesPerCluster is the bytes per cluster
int CMFTRecord::SetRecordInfo(LONGLONG  n64StartPos, DWORD dwRecSize, DWORD dwBytesPerCluster)
{
	if(!dwRecSize)
		return ERROR_INVALID_PARAMETER;

	if(dwRecSize%2)
		return ERROR_INVALID_PARAMETER;

	if(!dwBytesPerCluster)
		return ERROR_INVALID_PARAMETER;

	if(dwBytesPerCluster%2)
		return ERROR_INVALID_PARAMETER;

	m_dwMaxMFTRecSize = dwRecSize;
	m_dwBytesPerCluster = dwBytesPerCluster;
	m_n64StartPos =  n64StartPos;
	return ERROR_SUCCESS;
}


/// puchMFTBuffer is the MFT record buffer itself (normally 1024 bytes)
//  dwLen is the MFT record buffer length
//  bExcludeData = if true the file data will not be extracted
//                 This is useful for only file browsing
int CMFTRecord::ExtractFile(BYTE *puchMFTBuffer, DWORD dwLen, MFT_CHUNK_INFO *pstMftChunk, int *pnMFTchunkindex, bool bExcludeData)
{
	if(m_dwMaxMFTRecSize > dwLen)
		return ERROR_INVALID_PARAMETER;
	if(!puchMFTBuffer)
		return ERROR_INVALID_PARAMETER;

	NTFS_MFT_FILE	ntfsMFT;
	NTFS_ATTRIBUTE	ntfsAttr;

	BYTE *puchTmp = 0;
	BYTE *uchTmpData =0;
	DWORD dwTmpDataLen;
	int nRet, nMFTfragments = 0; // MFT‚Ì×•ª‰»”
	
	m_pMFTRecord = puchMFTBuffer;
	m_dwCurPos = 0;

	if(m_puchFileData)
		delete[] m_puchFileData;
	m_puchFileData = 0;
	m_dwFileDataSz = 0;

	// read the record header in MFT table
	memcpy(&ntfsMFT,&m_pMFTRecord[m_dwCurPos],sizeof(NTFS_MFT_FILE));

	m_bInUse = (ntfsMFT.wFlags&0x01); //0x01  	Record is in use
									  //0x02 	Record is a directory

	if(m_bMTFLoaded && m_bInUse)
		return FILE_IN_USE;

	m_dwCurPos = ntfsMFT.wAttribOffset;

	if(m_dwCurPos > 0x400)
		return FILE_IN_USE;

	// in case there is no Data attribute
	ZeroMemory(&m_ntfsAttr, sizeof(NTFS_ATTRIBUTE));

	do
	{
		// extract the attribute header
		memcpy(&ntfsAttr,&m_pMFTRecord[m_dwCurPos],sizeof(NTFS_ATTRIBUTE));

		switch(ntfsAttr.dwType) // extract the attribute data 
		{
			// here I haven' implemented the processing of all the attributes.
			//  I have implemented attributes necessary for file & file data extraction
		case 0://UNUSED
			break;

		case 0x10: //STANDARD_INFORMATION
			nRet = ExtractData(ntfsAttr,uchTmpData,dwTmpDataLen,NULL,NULL);
			if(nRet)
				return nRet;
			memcpy(&m_attrStandard,uchTmpData,sizeof(ATTR_STANDARD));

			delete[] uchTmpData;
			uchTmpData = 0;
			dwTmpDataLen = 0;
			break;

		case 0x20: //ATTRIBUTE_LIST
			nMFTfragments = (ntfsAttr.dwFullLength - ntfsAttr.Attr.Resident.wAttrOffset) / 0x20 - 0x03;
			break;

		case 0x30: //FILE_NAME
			nRet = ExtractData(ntfsAttr,uchTmpData,dwTmpDataLen,NULL,NULL);
			if(nRet)
				return nRet;

			if(uchTmpData[65] == 0x02) // short file name! so throw it away 
			{
				delete[] uchTmpData;
				break;
			}

			memcpy(&m_attrFilename,uchTmpData,dwTmpDataLen);

			delete[] uchTmpData;
			uchTmpData = 0;
			dwTmpDataLen = 0;
		
			if(ntfsMFT.wFlags&0x02 && m_attrFilename.chFileNameType == 0x01) //if directory and Unicode file name
				return ERROR_SUCCESS;

			break;

		case 0x40: //OBJECT_ID
			break;
		case 0x50: //SECURITY_DESCRIPTOR
			break;
		case 0x60: //VOLUME_NAME
			break;
		case 0x70: //VOLUME_IN	FORMATION
			break;
		case 0x80: //DATA
			if(!bExcludeData) 
			{
				nRet = ExtractData(ntfsAttr,uchTmpData,dwTmpDataLen,pstMftChunk,pnMFTchunkindex);
				if(nRet)
					return nRet;

				if(!m_puchFileData)
				{
					m_dwFileDataSz = dwTmpDataLen;
					m_puchFileData = uchTmpData;
				}
				else
				{
					puchTmp = new BYTE[m_dwFileDataSz+dwTmpDataLen];
					memcpy(puchTmp,m_puchFileData,m_dwFileDataSz);
					memcpy(puchTmp+m_dwFileDataSz,uchTmpData,dwTmpDataLen);

					m_dwFileDataSz += dwTmpDataLen;
					delete m_puchFileData;
					m_puchFileData = puchTmp;
				}
				dwTmpDataLen = 0;
			}
			else
				memcpy(&m_ntfsAttr,&ntfsAttr,sizeof(NTFS_ATTRIBUTE));
			break;

		case 0x90: //INDEX_ROOT
		case 0xa0: //INDEX_ALLOCATION
			// todo: not implemented to read the index mapped records
			return ERROR_SUCCESS;
			continue;
			break;
		case 0xb0: //BITMAP
			break;
		case 0xc0: //REPARSE_POINT
			break;
		case 0xd0: //EA_INFORMATION
			break;
		case 0xe0: //EA
			break;
		case 0xf0: //PROPERTY_SET
			break;
		case 0x100: //LOGGED_UTILITY_STREAM
			break;
		case 0x1000: //FIRST_USER_DEFINED_ATTRIBUTE
			break;

		case 0xFFFFFFFF: // END 
			if(!bExcludeData && nMFTfragments != 0)
			{
				if(nMFTfragments < 0)
					nMFTfragments = 2;
				for(int n = 0; n < nMFTfragments - 1; n++)
				{
					memcpy(&ntfsMFT,&m_puchFileData[m_dwMaxMFTRecSize * (0x0f + n)],sizeof(NTFS_MFT_FILE));
					m_dwCurPos = m_dwMaxMFTRecSize * (0x0f + n) + ntfsMFT.wAttribOffset;
					memcpy(&ntfsAttr,&m_puchFileData[m_dwCurPos],sizeof(NTFS_ATTRIBUTE));
					nRet = ExtractFullMFT(ntfsAttr, pstMftChunk, pnMFTchunkindex);
					if(nRet)
					{
						//AfxMessageBox("CMFTRecord::ExtractFile#ExtractFullMFT Failed!!"); 
						//return nRet;
						break;
					}
				}
				//pstMftChunk->n64MFTClstLen[0] = 0;
			}
			dwTmpDataLen = 0;
			return ERROR_SUCCESS;

		default:
			break;
		};

		m_dwCurPos += ntfsAttr.dwFullLength; // go to the next location of attribute

		// not to exceed the MFT record size
		if(m_dwMaxMFTRecSize < m_dwCurPos + sizeof(NTFS_ATTRIBUTE))
			break;
	}
	while(ntfsAttr.dwFullLength);

	dwTmpDataLen = 0;
	return ERROR_SUCCESS;
}

int CMFTRecord::ExtractFileAndWrite(BYTE *puchMFTBuffer, DWORD dwLen, HANDLE hNewFile, LONGLONG iFileSize, CProgressCtrl *pProgDataRecovery)
{
	if(m_dwMaxMFTRecSize > dwLen)
		return ERROR_INVALID_PARAMETER;
	if(!puchMFTBuffer)
		return ERROR_INVALID_PARAMETER;

	NTFS_MFT_FILE	ntfsMFT;
	NTFS_ATTRIBUTE	ntfsAttr;
	int nRet;	
	m_pMFTRecord = puchMFTBuffer;
	m_dwCurPos = 0;

	// read the record header in MFT table
	memcpy(&ntfsMFT,&m_pMFTRecord[m_dwCurPos],sizeof(NTFS_MFT_FILE));

	if(memcmp(ntfsMFT.szSignature,"FILE",4))
		return ERROR_INVALID_PARAMETER; // not the right signature

	m_dwCurPos = ntfsMFT.wAttribOffset;

	do
	{	// extract the attribute header
		memcpy(&ntfsAttr,&m_pMFTRecord[m_dwCurPos],sizeof(NTFS_ATTRIBUTE));

		switch(ntfsAttr.dwType) // extract the attribute data 
		{
		case 0x80: //DATA
			nRet = ExtractDataAndWrite(ntfsAttr,hNewFile,iFileSize,pProgDataRecovery);
			if(nRet)
				return nRet;
			break;
		case 0x100: //$EFS attribute including DDF and DRF
			nRet = ExtractDataAndWrite(ntfsAttr,hNewFile,iFileSize,pProgDataRecovery);
			if(nRet)
				return nRet;
			break;
		case 0xFFFFFFFF: // END 
			return ERROR_SUCCESS;
		default:
			break;
		};
		m_dwCurPos += ntfsAttr.dwFullLength; // go to the next location of attribute
		// not to exceed the MFT record size
		if(m_dwMaxMFTRecSize < m_dwCurPos + sizeof(NTFS_ATTRIBUTE))
			break;
	}
	while(ntfsAttr.dwFullLength);
	return ERROR_SUCCESS;
}

int CMFTRecord::ExtractDir(BYTE *puchMFTBuffer, DWORD dwLen)
{
	if(m_dwMaxMFTRecSize > dwLen)
		return ERROR_INVALID_PARAMETER;
	if(!puchMFTBuffer)
		return ERROR_INVALID_PARAMETER;

	NTFS_MFT_FILE	ntfsMFT;
	NTFS_ATTRIBUTE	ntfsAttr;

	BYTE *puchTmp = 0;
	BYTE *uchTmpData =0;
	DWORD dwTmpDataLen;
	int nRet;
	
	m_pMFTRecord = puchMFTBuffer;
	m_dwCurPos = 0;

	// read the record header in MFT table
	memcpy(&ntfsMFT,&m_pMFTRecord[m_dwCurPos],sizeof(NTFS_MFT_FILE));

	if(memcmp(ntfsMFT.szSignature,"FILE",4))
		return ERROR_INVALID_PARAMETER; // not the right signature

	m_dwCurPos = ntfsMFT.wAttribOffset;

	do
	{	// extract the attribute header
		memcpy(&ntfsAttr,&m_pMFTRecord[m_dwCurPos],sizeof(NTFS_ATTRIBUTE));

		switch(ntfsAttr.dwType) // extract the attribute data 
		{
		case 0x30: //FILE_NAME
			nRet = ExtractData(ntfsAttr,uchTmpData,dwTmpDataLen,NULL,NULL);
			if(nRet)
				return nRet;
			memcpy(&m_attrFilename,uchTmpData,dwTmpDataLen);

			delete[] uchTmpData;
			uchTmpData = 0;
			dwTmpDataLen = 0;

			if(m_attrFilename.chFileNameType == 0x01) //Unicode file name
				return ERROR_SUCCESS;
			break;
		};
		m_dwCurPos += ntfsAttr.dwFullLength; // go to the next location of attribute
		// not to exceed the MFT record size
		if(m_dwMaxMFTRecSize < m_dwCurPos + sizeof(NTFS_ATTRIBUTE))
			break;
	}
	while(ntfsAttr.dwFullLength);
	return ERROR_SUCCESS;
}

// extract the attribute data from the MFT table
//   Data can be Resident & non-resident
int CMFTRecord::ExtractData(NTFS_ATTRIBUTE &ntfsAttr, BYTE *&puchData, DWORD &dwDataLen, MFT_CHUNK_INFO *pstMftChunk, int *pnMFTchunkindex)
{
	DWORD dwCurPos = m_dwCurPos;
	m_dwFileDataRead = 0;

	if(!ntfsAttr.uchNonResFlag)
	{// residence attribute, this always resides in the MFT table itself
		if(ntfsAttr.Attr.Resident.dwLength > m_dwMaxMFTRecSize || ntfsAttr.Attr.Resident.wAttrOffset > m_dwMaxMFTRecSize)
			return BROKEN_MFT_RECORD;

		puchData = new BYTE[ntfsAttr.Attr.Resident.dwLength];
		dwDataLen = ntfsAttr.Attr.Resident.dwLength;	
		memcpy(puchData,&m_pMFTRecord[dwCurPos+ntfsAttr.Attr.Resident.wAttrOffset],dwDataLen);
	}
	else
	{// non-residence attribute, this resides in the other part of the physical drive

		if(!ntfsAttr.Attr.NonResident.n64AllocSize) // i don't know Y, but fails when its zero
			ntfsAttr.Attr.NonResident.n64AllocSize = (ntfsAttr.Attr.NonResident.n64EndVCN - ntfsAttr.Attr.NonResident.n64StartVCN) + 1;

		// ATTR_STANDARD size may not be correct
		dwDataLen = (DWORD)ntfsAttr.Attr.NonResident.n64RealSize;

		// allocate for reading data
		puchData = new BYTE[(UINT)ntfsAttr.Attr.NonResident.n64AllocSize];

		BYTE chLenOffSz; // length & offset sizes
		BYTE chLenSz; // length size
		BYTE chOffsetSz; // offset size
		LONGLONG n64Len, n64Offset; // the actual lenght & offset
		LONGLONG n64LCN =0; // the pointer pointing the actual data on a physical disk
		BYTE *pTmpBuff = puchData;
		int nRet;
		LONGLONG n64TatalLen =0;////bugfix

		dwCurPos += ntfsAttr.Attr.NonResident.wDatarunOffset;;

		for(;;)
		{
			///// read the length of LCN/VCN and length ///////////////////////
			chLenOffSz = 0;

			memcpy(&chLenOffSz,&m_pMFTRecord[dwCurPos],1);

			dwCurPos += 1;

			if(!chLenOffSz)
			{
				m_dwFileDataRead = (DWORD)n64TatalLen;
				break;
			}

			chLenSz		= chLenOffSz & 0x0F;
			chOffsetSz	= (chLenOffSz & 0xF0) >> 4;

			if(*pnMFTchunkindex >= MFTCHUNK_MAX)
				return TOO_MUCH_MFT;

			//sparse part
			if(chOffsetSz == 0 && chLenSz != 0)
			{
				dwCurPos += chLenSz;
				continue;
			}

			///// read the data length ////////////////////////////////////////
	
			n64Len = 0;

			memcpy(&n64Len,&m_pMFTRecord[dwCurPos],chLenSz);
			pstMftChunk->n64MFTClstLen[*pnMFTchunkindex] = n64Len;

			dwCurPos += chLenSz;

			///// read the LCN/VCN offset //////////////////////////////////////
	
			n64Offset = 0;

			memcpy(&n64Offset,&m_pMFTRecord[dwCurPos],chOffsetSz);
			
			dwCurPos += chOffsetSz;

			////// if the last bit of n64Offset is 1 then its -ve so u got to make it -ve /////
			if((((char*)&n64Offset)[chOffsetSz-1])&0x80)
				for(int i=sizeof(LONGLONG)-1;i>(chOffsetSz-1);i--)
					((char*)&n64Offset)[i] = (char)0xff;

			pstMftChunk->n64MFTClstOffset[*pnMFTchunkindex] = n64Offset;

			n64LCN += n64Offset;			
			n64Len *= m_dwBytesPerCluster;

			// avoid buffer overflow
			if(ntfsAttr.Attr.NonResident.n64AllocSize < n64TatalLen)
			{
				m_dwFileDataRead = (DWORD)n64TatalLen;
				break;
			}
						
			///// read the actual data /////////////////////////////////////////
			/// since the data is available out side the MFT table, physical drive should be accessed
			nRet = ReadRaw(n64LCN,pTmpBuff,(DWORD&)n64Len);
			if(nRet)
			{
				ErrorMessage(nRet);
				m_dwFileDataRead = (DWORD)n64TatalLen;
				break;
			}

			n64TatalLen += n64Len;
			pTmpBuff += n64Len;
			(*pnMFTchunkindex)++;
		}
	}
	return ERROR_SUCCESS;
}

// extract fragmented  MFT 
int CMFTRecord::ExtractFullMFT(NTFS_ATTRIBUTE &ntfsAttr, MFT_CHUNK_INFO *pstMftChunk, int *pnMFTchunkindex)
{
	DWORD dwCurPos = m_dwCurPos;

	BYTE chLenOffSz; // length & offset sizes
	BYTE chLenSz; // length size
	BYTE chOffsetSz; // offset size
	LONGLONG n64Len, n64Offset; // the actual lenght & offset
	LONGLONG n64LCN =0; // the pointer pointing the actual data on a physical disk
	int nRet;

	dwCurPos += ntfsAttr.Attr.NonResident.wDatarunOffset;;

	for(;;)
	{
		///// read the length of LCN/VCN and length ///////////////////////
		chLenOffSz = 0;

		memcpy(&chLenOffSz,&m_puchFileData[dwCurPos],1);

		dwCurPos += 1;

		if(!chLenOffSz)
			break;

		chLenSz		= chLenOffSz & 0x0F;
		chOffsetSz	= (chLenOffSz & 0xF0) >> 4;

		if(*pnMFTchunkindex >= MFTCHUNK_MAX)
			return TOO_MUCH_MFT;

		//sparse part
		if(chOffsetSz == 0 && chLenSz != 0)
		{
			dwCurPos += chLenSz;
			continue;
		}

		///// read the data length ////////////////////////////////////////

		n64Len = 0;

		memcpy(&n64Len,&m_puchFileData[dwCurPos],chLenSz);
		pstMftChunk->n64MFTClstLen[*pnMFTchunkindex] = n64Len;

		dwCurPos += chLenSz;

		///// read the LCN/VCN offset //////////////////////////////////////

		n64Offset = 0;

		memcpy(&n64Offset,&m_puchFileData[dwCurPos],chOffsetSz);

		dwCurPos += chOffsetSz;

		////// if the last bit of n64Offset is 1 then its -ve so u got to make it -ve /////
		if((((char*)&n64Offset)[chOffsetSz-1])&0x80)
			for(int i=sizeof(LONGLONG)-1;i>(chOffsetSz-1);i--)
				((char*)&n64Offset)[i] = (char)0xff;

		pstMftChunk->n64MFTClstOffset[*pnMFTchunkindex] = n64Offset;

		n64LCN += n64Offset;			
		n64Len *= m_dwBytesPerCluster;

		if(m_dwFileDataRead + n64Len > m_dwFileDataSz)
		{
			AfxMessageBox("MFT overflow!!");  //for debug
			return MFT_OVERFLOW;
		}

		///// read the actual data /////////////////////////////////////////
		nRet = ReadRaw(n64LCN, &m_puchFileData[m_dwFileDataRead], (DWORD&)n64Len);
		if(nRet)
		{
			AfxMessageBox("CMFTRecord::ExtractFullMFT#ReadRaw Failed!!");  //for debug
			return nRet;
		}
		m_dwFileDataRead += (DWORD)n64Len;
		(*pnMFTchunkindex)++;
	}
	return ERROR_SUCCESS;
}

void CMFTRecord::InsertPadding(BYTE *&puchData, DWORD &nDataSize, LONGLONG nFileSize)
{
	// initialize efs_padding
	g_efsPadding.token[1] = 0x02;
	g_efsPadding.token[2] = 0x01;
	g_efsPadding.filesize1 = g_efsPadding.filesize2 = g_efsPadding.encryptedsize = 0x10000;

	DWORD nInsertCnt = nDataSize / 0x10000;
	DWORD nRemainder = nDataSize % 0x10000;
	DWORD nRemainder1 = (DWORD)nFileSize % 0x10000;
	nDataSize = nDataSize + 0x200 * nInsertCnt;
	BYTE *puchNewData = new BYTE[nDataSize];

	for(DWORD i = 0; i < nInsertCnt; i++)
	{
		memcpy(&puchNewData[i*0x10200], &puchData[i*0x10000], 0x10000);
		g_efsPadding.seq_number = (WORD)i+1;
		// insert efs_padding
		if(i == nInsertCnt - 1)
		{
			g_efsPadding.token[1] = 0xae;
			g_efsPadding.token[2] = 0x00;
			g_efsPadding.filesize1 = g_efsPadding.filesize2 = g_efsPadding.encryptedsize = nRemainder1;
		}
		memcpy(&puchNewData[0x10000 + i * 0x10200], &g_efsPadding, sizeof(g_efsPadding));
	}
	//memcpy(&puchNewData[i*0x10200], &puchData[i*0x10000], nRemainder);
	memcpy(&puchNewData[nInsertCnt*0x10200], &puchData[nInsertCnt*0x10000], nRemainder);

	delete[] puchData;
	puchData = puchNewData;
}	

int CMFTRecord::ExtractDataAndWrite(NTFS_ATTRIBUTE ntfsAttr, HANDLE hNewFile, LONGLONG iFileSize, CProgressCtrl *pProgDataRecovery)
{
	int nRet;
	BYTE *puchData;
	DWORD dwDataLen;
	DWORD dwBytes;
	DWORD dwCurPos = m_dwCurPos;
	BOOL bCompressed = FALSE;
	static LONGLONG nFileSize;
	LONGLONG prevFileSize;

	if(!ntfsAttr.uchNonResFlag)
	{// residence attribute, this always resides in the MFT table itself

		// $EFS attribute! so make efs-header and preprocess encrypted data
		if(ntfsAttr.dwType == 0x100)
		{
			bool bPaddingNeeded = false;

			DWORD dwEncryptedFilesize = ((DWORD)(nFileSize / m_dwBytesPerCluster) + 1) * m_dwBytesPerCluster;
			puchData = new BYTE[dwEncryptedFilesize];
			SetFilePointer(hNewFile,0,NULL,FILE_BEGIN);
			nRet = ReadFile(hNewFile,puchData,dwEncryptedFilesize,&dwBytes,NULL);
			if(!nRet)
				return GetLastError();

			if(nFileSize > 0x10000)
			{
				bPaddingNeeded = true;
				InsertPadding(puchData, dwEncryptedFilesize, nFileSize);
			}

			// insert efs_header
			SetFilePointer(hNewFile,0,NULL,FILE_BEGIN);
			nRet = WriteFile(hNewFile,&g_efsHeader,sizeof(g_efsHeader),&dwBytes,NULL);
			if(!nRet)
				return GetLastError();
			// insert extracted efs DDF & DRF
			m_pMFTRecord[dwCurPos + 16*8 + 6] = 0x00;
			nRet = WriteFile(hNewFile,&m_pMFTRecord[dwCurPos+32],ntfsAttr.dwFullLength-32,&dwBytes,NULL);
			if(!nRet)
				return GetLastError();
			// insert efs_trailer
			if(bPaddingNeeded)
			{
				g_efsTrailer.token[1] = 0x02;
				g_efsTrailer.token[2] = 0x01;
				g_efsTrailer.filesize1 = g_efsTrailer.filesize2 = g_efsTrailer.encryptedsize = 0x10000;
			}
			else
				g_efsTrailer.filesize1 = g_efsTrailer.filesize2 = g_efsTrailer.encryptedsize = (DWORD)nFileSize;
			nRet = WriteFile(hNewFile,&g_efsTrailer,sizeof(g_efsTrailer),&dwBytes,NULL);
			if(!nRet)
				return GetLastError();
			// append encrypted data
			nRet = WriteFile(hNewFile,puchData,dwEncryptedFilesize,&dwBytes,NULL);
			if(!nRet)
				return GetLastError();

			delete[] puchData;		
		}
		else
		{
			puchData = &m_pMFTRecord[dwCurPos+ntfsAttr.Attr.Resident.wAttrOffset];
			dwDataLen = ntfsAttr.Attr.Resident.dwLength;
			nRet = WriteFile(hNewFile,puchData,dwDataLen,&dwBytes,NULL);
			if(!nRet)
				return GetLastError();
			if(ntfsAttr.wFlags == 0x0001)
				bCompressed = TRUE;
		}
	}
	else
	{// non-residence attribute, this resides in the other part of the physical drive
		bool bPaddingNeeded = false;
		DWORD dwEncryptedFilesize;

		// $EFS attribute! so preprocess encrypted data
		if(ntfsAttr.dwType == 0x100)
		{			
			dwEncryptedFilesize = ((DWORD)(nFileSize / m_dwBytesPerCluster) + 1) * m_dwBytesPerCluster;
			puchData = new BYTE[dwEncryptedFilesize];
			SetFilePointer(hNewFile,0,NULL,FILE_BEGIN);
			nRet = ReadFile(hNewFile,puchData,dwEncryptedFilesize,&dwBytes,NULL);
			if(!nRet)
				return GetLastError();

			if(nFileSize > 0x10000)
			{
				bPaddingNeeded = true;
				InsertPadding(puchData, dwEncryptedFilesize, nFileSize);
			}

			// insert efs_header
			SetFilePointer(hNewFile,0,NULL,FILE_BEGIN);
			nRet = WriteFile(hNewFile,&g_efsHeader,sizeof(g_efsHeader),&dwBytes,NULL);
			if(!nRet)
				return GetLastError();
		}

		BYTE chLenOffSz; // length & offset sizes
		BYTE chLenSz; // length size
		BYTE chOffsetSz; // offset size
		LONGLONG n64Len, n64Offset, prev64Len; // the actual lenght & offset & previous length
		LONGLONG n64LCN =0; // the pointer pointing the actual data on a physical disk
		dwCurPos += ntfsAttr.Attr.NonResident.wDatarunOffset;

		DWORD dwRemainder;
		LONGLONG nTatalBytesToRead = 0;

		prevFileSize = nFileSize;
		//nFileSize = ntfsAttr.Attr.NonResident.n64RealSize;
		nFileSize = iFileSize;

		if(ntfsAttr.Attr.NonResident.wCompressionSize == 4) //compressed file
		{
			bCompressed = TRUE;
		}
		
		for(;;)
		{
			///// read the length of LCN/VCN and length ///////////////////////
			chLenOffSz = 0;

			memcpy(&chLenOffSz,&m_pMFTRecord[dwCurPos],1);

			dwCurPos += 1;

			if(!chLenOffSz)
				break;

			chLenSz		= chLenOffSz & 0x0F;
			chOffsetSz	= (chLenOffSz & 0xF0) >> 4;

			if((ntfsAttr.wFlags & FILE_ATTRIBUTE_ENCRYPTED) && chOffsetSz == 0)
				return ERROR_SUCCESS; 

			///// read the data length ////////////////////////////////////////
	
			n64Len = 0;

			memcpy(&n64Len,&m_pMFTRecord[dwCurPos],chLenSz);

			dwCurPos += chLenSz;

			if(bCompressed && n64Len > 16 && chOffsetSz == 0)
			{
				bCompressed = FALSE;
				n64LCN += 0x10;
				n64Len = n64Len - (0x10 - prev64Len); //adjust uncompressed data length
				goto UncompressedData;
			}

			if(bCompressed && chOffsetSz == 0)
			{
				continue;
			}
			///// read the LCN/VCN offset //////////////////////////////////////
	
			n64Offset = 0;

			memcpy(&n64Offset,&m_pMFTRecord[dwCurPos],chOffsetSz);

			if(bCompressed  && n64Offset == 0)
			{
				n64Offset = 0x10;
				n64Len = 0x10;
			}			

			dwCurPos += chOffsetSz;

			////// if the last bit of n64Offset is 1 then its -ve so u got to make it -ve /////
			if(chOffsetSz != 0)
			{
				if((((char*)&n64Offset)[chOffsetSz-1])&0x80)
					for(int i=sizeof(LONGLONG)-1;i>(chOffsetSz-1);i--)
						((char*)&n64Offset)[i] = (char)0xff;
			}
			
			n64LCN += n64Offset;

UncompressedData:
			prev64Len = n64Len;
			n64Len *= m_dwBytesPerCluster;
			nTatalBytesToRead += n64Len;
			
			if(nTatalBytesToRead < nFileSize)
				dwRemainder = 0;
			else
			{
				if(!(ntfsAttr.wFlags & FILE_ATTRIBUTE_ENCRYPTED) || ntfsAttr.dwType == 0x100)
					dwRemainder = (DWORD)(nFileSize % m_dwBytesPerCluster);
				else
					dwRemainder = 0;
			}
			///// read the actual data and write/////////////////////////////////////////
			/// since the data is available out side the MFT table, physical drive should be accessed
			nRet = ReadRawAndWrite(n64LCN, (DWORD&)n64Len, dwRemainder, hNewFile, bCompressed, pProgDataRecovery);
			if(nRet)
				return nRet;

			// $EFS attribute! so add header and append data
			if(ntfsAttr.dwType == 0x100)
			{
				// insert efs_trailer
				if(bPaddingNeeded)
				{
					g_efsTrailer.token[1] = 0x02;
					g_efsTrailer.token[2] = 0x01;
					g_efsTrailer.filesize1 = g_efsTrailer.filesize2 = g_efsTrailer.encryptedsize = 0x10000;
				}
				else
					g_efsTrailer.filesize1 = g_efsTrailer.filesize2 = g_efsTrailer.encryptedsize = (DWORD)prevFileSize;
				nRet = WriteFile(hNewFile,&g_efsTrailer,sizeof(g_efsTrailer),&dwBytes,NULL);
				if(!nRet)
					return GetLastError();
				// append preprocessed encrypted data
				nRet = WriteFile(hNewFile,puchData,dwEncryptedFilesize,&dwBytes,NULL);
				if(!nRet)
					return GetLastError();

				delete[] puchData;		
			}

			if((n64Len > 16*m_dwBytesPerCluster) && chOffsetSz == 0 && !bCompressed)
			{
				n64LCN -= 0x10;
				bCompressed = TRUE;
			}
		}
	}

	if(bCompressed)
	{
		BOOL	bRet;
		DWORD	cb;

		USHORT  usInBuff = COMPRESSION_FORMAT_DEFAULT;			
		bRet = DeviceIoControl(hNewFile, FSCTL_SET_COMPRESSION,
			&usInBuff, sizeof(USHORT),
			0, 0, &cb, 0);
		if(!bRet)
			return GetLastError();
	}
	return ERROR_SUCCESS;
}

// read the data from the physical drive
int CMFTRecord::ReadRaw(LONGLONG n64LCN, BYTE *chData, DWORD &dwLen)
{
	int nRet;
	DWORD dwRet, dwError;
	LARGE_INTEGER n64Pos;
	
	n64Pos.QuadPart = (n64LCN)*m_dwBytesPerCluster;
	n64Pos.QuadPart += m_n64StartPos;

	dwRet = SetFilePointer(m_hDrive,n64Pos.LowPart,&n64Pos.HighPart,FILE_BEGIN);
	if (dwRet == INVALID_SET_FILE_POINTER && (dwError = GetLastError()) != NO_ERROR ) //changed!!
	{ 
		AfxMessageBox(_T("CMFTRecord::ReadRaw#SetFilePointer Failed!!"), MB_OKCANCEL|MB_ICONEXCLAMATION); //for debug
		return dwError;
	}

	short const sClustersToReadOneTime = 128;
	BYTE *pTmp			= chData;
	DWORD dwBytesToRead	= m_dwBytesPerCluster * sClustersToReadOneTime;
	DWORD dwBytes		=0;
	DWORD dwTotRead		=0;
	DWORD dwTimesToRead = (dwLen / m_dwBytesPerCluster) / sClustersToReadOneTime + 1;
	DWORD dwRemainder = ((dwLen / m_dwBytesPerCluster) % sClustersToReadOneTime) * m_dwBytesPerCluster;

	for(DWORD i=0; i < dwTimesToRead; i++)
	{
		if(i == dwTimesToRead - 1)
			dwBytesToRead = dwRemainder;
		nRet = ReadFile(m_hDrive,pTmp,dwBytesToRead,&dwBytes,NULL);
		if(!nRet)
			return GetLastError();

		dwTotRead += dwBytes;
		pTmp += dwBytes;
	}

	dwLen = dwTotRead;
	return ERROR_SUCCESS;
}

//int CMFTRecord::ReadRaw1(LONGLONG n64Offset, LONGLONG n64LCN, BYTE *chData, DWORD &dwLen)
//{
//	int nRet;
//	DWORD dwRet, dwError;
//	LARGE_INTEGER n64Pos;
//	
//	n64Pos.QuadPart = (n64LCN)*m_dwBytesPerCluster;
//	n64Pos.QuadPart += m_n64StartPos;
//
//	TCHAR szBuffer[255];
//	_stprintf(szBuffer,"CMFTRecord::ReadRaw#SetFilePointer Failed!!\n n64Offset = %X\nn64LCN = %X", n64Offset , n64LCN);
//
//	dwRet = SetFilePointer(m_hDrive,n64Pos.LowPart,&n64Pos.HighPart,FILE_BEGIN);
//	if (dwRet == INVALID_SET_FILE_POINTER && (dwError = GetLastError()) != NO_ERROR ) //changed!!
//	{ 
//		AfxMessageBox(szBuffer, MB_OKCANCEL|MB_ICONEXCLAMATION); //for debug
//		return dwError;
//	}
//
//	short const sClustersToReadOneTime = 128;
//	BYTE *pTmp			= chData;
//	DWORD dwBytesToRead	= m_dwBytesPerCluster * sClustersToReadOneTime;
//	DWORD dwBytes		=0;
//	DWORD dwTotRead		=0;
//	DWORD dwTimesToRead = (dwLen / m_dwBytesPerCluster) / sClustersToReadOneTime + 1;
//	DWORD dwRemainder = ((dwLen / m_dwBytesPerCluster) % sClustersToReadOneTime) * m_dwBytesPerCluster;
//
//	for(DWORD i=0; i < dwTimesToRead; i++)
//	{
//		if(i == dwTimesToRead - 1)
//			dwBytesToRead = dwRemainder;
//		nRet = ReadFile(m_hDrive,pTmp,dwBytesToRead,&dwBytes,NULL);
//		if(!nRet)
//			return GetLastError();
//
//		dwTotRead += dwBytes;
//		pTmp += dwBytes;
//	}
//
//	dwLen = dwTotRead;
//	return ERROR_SUCCESS;
//}

int CMFTRecord::ReadRawAndWrite(LONGLONG n64LCN, DWORD &dwLen, DWORD dwRemainder, HANDLE hNewFile, BOOL bCompressed, CProgressCtrl *pProgDataRecovery)
{
	INIT_NTDLL_ADDRESS(RtlDecompressBuffer);

	BOOL bRet;
	DWORD dwRet;

	LARGE_INTEGER n64Pos;

	n64Pos.QuadPart = (n64LCN)*m_dwBytesPerCluster;
	n64Pos.QuadPart += m_n64StartPos;

	dwRet = SetFilePointer(m_hDrive,n64Pos.LowPart,&n64Pos.HighPart,FILE_BEGIN);
	if(dwRet == INVALID_SET_FILE_POINTER)
		return GetLastError();

	DWORD dwBytes		=0;
	DWORD dwTotRead		=0;
	DWORD dwClustersToRead;
	DWORD dwBytesRead;
	BYTE *puchData;
	BYTE *puchDataDeCompressed;
	ULONG ulDeCompressedSize;

	if(bCompressed)
	{
		dwClustersToRead = 1;
		dwBytesRead = dwLen;
		puchData = new BYTE[dwLen];
		puchDataDeCompressed = new BYTE[dwLen * COMPRESSION_RATIO];
	}
	else
	{
		dwClustersToRead = dwLen / m_dwBytesPerCluster;;
		dwBytesRead = m_dwBytesPerCluster;
		puchData = new BYTE[m_dwBytesPerCluster];
	}
	
	for(DWORD i = 0; i < dwClustersToRead; i++)
	{
		// this can not read partial sectors
		bRet = ReadFile(m_hDrive,puchData,dwBytesRead,&dwBytes,NULL);
		if(!bRet)
			return GetLastError();

		if(bCompressed)
		{
			RtlDecompressBuffer(COMPRESSION_FORMAT_LZNT1,puchDataDeCompressed,dwLen*COMPRESSION_RATIO,puchData,dwLen,&ulDeCompressedSize);
			if(ulDeCompressedSize > 0x10000) // Decompress failed! it means data uncompressed, then write as it is. 
				goto UnCompressed;
			bRet = WriteFile(hNewFile,puchDataDeCompressed,ulDeCompressedSize,&dwBytes,NULL);
			if(!bRet)
				return GetLastError();
			dwTotRead += dwBytes;
			pProgDataRecovery->OffsetPos(dwBytes);
			break;
		}

		if(dwRemainder != 0 && i == dwClustersToRead - 1)//Last cluster for the file! So write only remainder
			dwBytesRead = dwRemainder;

UnCompressed:
		bRet = WriteFile(hNewFile,puchData,dwBytesRead,&dwBytes,NULL);
		if(!bRet)
			return GetLastError();

		dwTotRead += dwBytes;
		pProgDataRecovery->OffsetPos(dwBytes);
	}

	delete[] puchData;
	if(bCompressed)
		delete[] puchDataDeCompressed;
	dwLen = dwTotRead;
	return ERROR_SUCCESS;
}

void CMFTRecord::ErrorMessage(DWORD dwErrorNo)
{
	TCHAR *lpMsgBuf;
	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM,NULL,dwErrorNo, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),(LPTSTR) &lpMsgBuf, 0, NULL);
	AfxMessageBox((LPTSTR)lpMsgBuf, MB_OK|MB_ICONERROR);
	LocalFree(lpMsgBuf);
}