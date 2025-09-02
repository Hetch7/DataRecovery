// DataRecoveryDlg.h : header file
//

#if !defined(AFX_DataRecoveryDLG_H__8B418511_9F89_47D7_9F50_B4D15178914B__INCLUDED_)
#define AFX_DataRecoveryDLG_H__8B418511_9F89_47D7_9F50_B4D15178914B__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "NTFSDrive.h"
#include "FATDrive.h"
#include "RoundButton2.h"
#include "RoundButtonStyle.h"

typedef struct
{
	WORD	wCylinder;
	WORD	wHead;
	WORD	wSector;
	DWORD	dwNumSectors;
	WORD	wType;
	DWORD	dwRelativeSector;
	DWORD	dwNTRelativeSector;
	DWORD	dwBytesPerSector;
	BYTE	chDriveNo;   

}DRIVEPACKET;

typedef struct
{
	BYTE	chBootInd;
	BYTE	chHead;
	BYTE	chSector;
	BYTE	chCylinder;
	BYTE	chType;
	BYTE	chLastHead;
	BYTE	chLastSector;
	BYTE	chLastCylinder;
	DWORD	dwRelativeSector;
	DWORD	dwNumberSectors;

}PARTITION;

#define PART_TABLE 0
#define BOOT_RECORD 1
#define EXTENDED_PART 2

#define PART_UNKNOWN 0x00		//Unknown.  
#define PART_DOS2_FAT 0x01		//12-bit FAT.  
#define PART_DOS3_FAT 0x04		//16-bit FAT. Partition smaller than 32MB.  
#define PART_EXTENDED 0x05		//Extended MS-DOS Partition.  
#define PART_DOS4_FAT 0x06		//16-bit FAT. Partition larger than or equal to 32MB.  
#define PART_DOS32 0x0B			//32-bit FAT. Partition up to 2047GB.  
#define PART_DOS32X 0x0C		//Same as PART_DOS32(0Bh), but uses Logical Block Address Int 13h extensions.  
#define PART_DOSX13 0x0E		//Same as PART_DOS4_FAT(06h), but uses Logical Block Address Int 13h extensions.  
#define PART_DOSX13X 0x0F		//Same as PART_EXTENDED(05h), but uses Logical Block Address Int 13h extensions.  

#define FILE_IN_USE 40L
#define SAME_PARTITION 41L
#define TOO_MUCH_MFT 42L
#define BROKEN_MFT_RECORD 44L
#define MEMORY_ALLOCATE_LIMIT 45L
#define TOO_LONG_FILENAME 46L

// for memory leak check!
#define CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
/////////////////////////////////////////////////////////////////////////////
// CDataRecoveryDlg dialog

class CDataRecoveryDlg : public CDialog
{
// Construction
public:
	void AddDrive(char *szDrvTxt, DRIVEPACKET *pstDrive);
	CDataRecoveryDlg(CWnd* pParent = NULL);	// standard constructor

// Dialog Data
	//{{AFX_DATA(CDataRecoveryDlg)
	enum { IDD = IDD_DATARECOVERY_DIALOG };
		// NOTE: the ClassWizard will add data members here
	//}}AFX_DATA

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CDataRecoveryDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	CFont m_font;
	int ScanLogicalDrives();
	HICON m_hIcon;
	HTREEITEM m_hTreeRoot;
	static int CALLBACK MyCompareProc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort);
	void ReNumberItems();
	void DeleteFileNTFS();

	// Generated message map functions
	//{{AFX_MSG(CDataRecoveryDlg)
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg void OnBtnScan();
	afx_msg void OnDestroy();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnLvnColumnclickLstfiles(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnBnClickedBtnDataRecovery();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	CRoundButtonStyle m_tMyButtonStyle;
	CRoundButtonStyle m_tMyButtonStyleD;
	CRoundButton2 m_btnScan;
	CRoundButton2 m_btnRecovery;
	CRoundButton2 m_btnDelete;
	afx_msg void OnBnClickedBtndelete();
	void ErrorMessage(DWORD dwErrorNo);
	void ErrorMessage1(CString cstrWhere, DWORD dwErrorNo);
	void DecryptFile(CString cstrFileName);
	void MakeSubDirFAT(CString cstrDirName, CString cstrParentDirName, CListCtrl *pcList, CProgressCtrl *pProgDataRecovery);
	void MakeSubDirNTFS(CString cstrDirName, CString cstrParentDirName, BOOL bIsCompressed, CListCtrl *pcList, CProgressCtrl *pProgDataRecovery);
	CFATDrive m_cFAT;
	CNTFSDrive m_cNTFS;
	CString m_cstrDriveLetter;
	CString m_cstrDrive;
	bool m_bIsFAT;
	bool m_bIsSystemDrive;
	HANDLE m_hDrive;
	HANDLE m_hScanFilesThread;
	bool m_bStopScanFilesThread;
	afx_msg void OnNMRclickLstfiles(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnLvnEndlabeleditLstfiles(NMHDR *pNMHDR, LRESULT *pResult);
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_DataRecoveryDLG_H__8B418511_9F89_47D7_9F50_B4D15178914B__INCLUDED_)
