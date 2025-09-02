// DataRecoveryDlg.cpp : implementation file
//

#include "stdafx.h"
#include "DataRecovery.h"
#include "DataRecoveryDlg.h"
#include "LinkStatic.h"
#include "GradientStatic.h"
#include "AutoUpdater.h"
#include "LoadMFTDlg.h"
#include <shlwapi.h>
#include <winioctl.h>

#include "jwd_detector.h"
#include "yahoo_detector.h"

#include "ToolsDlg.h"

//#include <Winnetwk.h> //ネットワークドライブ接続


#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define BIF_NEWDIALOGSTYLE 0x0040

// advapi32.dll functions for decryption
#define INIT_ADVAPI_ADDRESS( name )\
    (FARPROC&)g_p##name = GetProcAddress( GetModuleHandle( _T("advapi32") ), #name)

#define DEF_OPENENC_PROC( name )\
    (WINAPI *##name)
#define DEF_CLOSEENC_PROC( name )\
    (WINAPI *##name)
#define DEF_WRITEENC_PROC( name )\
    (WINAPI *##name)

DWORD DEF_OPENENC_PROC(g_pOpenEncryptedFileRawA)(LPCTSTR,ULONG,PVOID*);
void DEF_CLOSEENC_PROC(g_pCloseEncryptedFileRaw)(PVOID);
DWORD DEF_WRITEENC_PROC(g_pWriteEncryptedFileRaw)(PFE_IMPORT_FUNC,PVOID,PVOID);


// global variables 
int g_Sort = -1;
int g_iDirIcon = 0;
TCHAR g_szDirType[80] = "";
bool g_bStop = false;
bool g_bIsNT = false;
bool g_bIsVista = false;
bool g_bFirstShow = true;
BOOL g_bJWordLiving = FALSE;
BOOL g_bYahooLiving = FALSE;
UINT __cdecl RecoverNtfsThread(LPVOID lpVoid);
UINT __cdecl RecoverFatThread(LPVOID lpVoid);
UINT __cdecl DeleteFileFatThread(LPVOID lpVoid);
UINT __cdecl ScanThread(LPVOID lpVoid);

int CALLBACK BrowseCallbackProc(HWND hwnd,UINT uMsg,LPARAM lp, LPARAM pData) 
{
	switch(uMsg) 
	{
	case BFFM_INITIALIZED: 
		SendMessage(hwnd,BFFM_SETSELECTION,TRUE,(LPARAM)pData);
		break;
	default:
		break;
	}
	return 0;
}

BOOL GetDirectory(HWND hWnd, LPTSTR szDir, LPTSTR szDefaultPath) 
{ 
	BOOL			fRet; 
	TCHAR			szPath[MAX_PATH]; 
	LPITEMIDLIST	pidl; 
	LPMALLOC		lpMalloc; 
	CString			cstrMessage;
	cstrMessage.LoadString(IDS_MSG_WHERE_TO_RECOVER);

	BROWSEINFO		bi = {hWnd, NULL, szPath, cstrMessage, BIF_RETURNONLYFSDIRS|BIF_NEWDIALOGSTYLE, NULL, 0L, 0};

	if(szDefaultPath)
	{
		bi.lpfn = BrowseCallbackProc;
		bi.lParam = (LPARAM)szDefaultPath;
	}

	pidl = SHBrowseForFolder(&bi);  
	if (NULL != pidl) 
		fRet = SHGetPathFromIDList(pidl, szDir); 
	else 
		fRet = FALSE; 
	// Get the shell's allocator to free PIDLs 
	if(!SHGetMalloc(&lpMalloc) && (NULL != lpMalloc)) 
	{ 
		if(NULL != pidl) 
		{ 
			lpMalloc->Free(pidl); 
		}  
		lpMalloc->Release(); 
	}  
	return fRet; 
}


/////////////////////////////////////////////////////////////////////////////
// CAboutDlg dialog used for App About

class CAboutDlg : public CDialog
{
	CFont m_font;
public:
	CAboutDlg();

// Dialog Data
	//{{AFX_DATA(CAboutDlg)
	enum { IDD = IDD_ABOUTBOX };
	//}}AFX_DATA

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CAboutDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	//{{AFX_MSG(CAboutDlg)
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
public:
	CLinkStatic m_lsURL;
	virtual BOOL OnInitDialog();
	CGradientStatic m_grAppVersion;
	afx_msg void OnBnClickedOk();
	CLinkStatic m_lsHigawari;
};

CAboutDlg::CAboutDlg() : CDialog(CAboutDlg::IDD)
{
	//{{AFX_DATA_INIT(CAboutDlg)
	//}}AFX_DATA_INIT
	m_grAppVersion.SetColor(RGB(255,0,255));
	m_grAppVersion.SetTextColor(RGB(0,0,0));
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CAboutDlg)
	//}}AFX_DATA_MAP
	DDX_Control(pDX, IDC_URL, m_lsURL);
	DDX_Control(pDX, IDC_APPVERSION, m_grAppVersion);
	DDX_Control(pDX, IDC_HIGAWARI, m_lsHigawari);
}

BOOL CAboutDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	LOGFONT lf;
	memset(&lf, 0, sizeof(LOGFONT));
	lf.lfWeight = FW_SEMIBOLD;
	lf.lfHeight = 12;
	//lf.lfQuality = CLEARTYPE_QUALITY;
	strcpy_s(lf.lfFaceName, 32, "ＭＳ ゴシック");
	m_font.CreateFontIndirect(&lf);

	CString cstrTitle;
	cstrTitle.LoadString(IDS_ABOUTBOX_TITLE);
	SetWindowText(cstrTitle);

	CStatic *pStaticTokiwa = (CStatic *)GetDlgItem(IDC_TOKIWA);
	cstrTitle.LoadString(IDS_ABOUTBOX_TOKIWA);
	pStaticTokiwa->SetWindowText(cstrTitle);
	if(g_bIsNT)
		pStaticTokiwa->SetFont(&m_font);

	CStatic *pStaticTokiwaURL = (CStatic *)GetDlgItem(IDC_URL);
	cstrTitle.LoadString(IDS_ABOUTBOX_TOKIWA_URL);
	pStaticTokiwaURL->SetWindowText(cstrTitle);
	
	if (g_bIsNT)
	{
		CButton *pBtnOK = (CButton *)GetDlgItem(IDOK);
		cstrTitle = _T("Update Check");
		pBtnOK->SetWindowText(cstrTitle);
	}

	return TRUE;  // return TRUE unless you set the focus to a control
	// 例外 : OCX プロパティ ページは必ず FALSE を返します。
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
	//{{AFX_MSG_MAP(CAboutDlg)
		// No message handlers
	//}}AFX_MSG_MAP
	ON_WM_DESTROY()
	ON_BN_CLICKED(IDOK, OnBnClickedOk)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CDataRecoveryDlg dialog

CDataRecoveryDlg::CDataRecoveryDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CDataRecoveryDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CDataRecoveryDlg)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
	// Note that LoadIcon does not require a subsequent DestroyIcon in Win32
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CDataRecoveryDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CDataRecoveryDlg)
	// NOTE: the ClassWizard will add DDX and DDV calls here
	//}}AFX_DATA_MAP
	DDX_Control(pDX, IDC_BTNSCAN, m_btnScan);
	DDX_Control(pDX, IDC_BTNDATARECOVERY, m_btnRecovery);
	DDX_Control(pDX, IDC_BTNDELETE, m_btnDelete);
}

BEGIN_MESSAGE_MAP(CDataRecoveryDlg, CDialog)
	//{{AFX_MSG_MAP(CDataRecoveryDlg)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_BTNSCAN, OnBtnScan)
	ON_WM_DESTROY()
	//}}AFX_MSG_MAP
	ON_NOTIFY(LVN_COLUMNCLICK, IDC_LSTFILES, OnLvnColumnclickLstfiles)
	ON_BN_CLICKED(IDC_BTNDATARECOVERY, OnBnClickedBtnDataRecovery)
//	ON_WM_SIZING()
ON_WM_SIZE()
//ON_WM_ERASEBKGND()
ON_BN_CLICKED(IDC_BTNDELETE, OnBnClickedBtndelete)
ON_NOTIFY(NM_RCLICK, IDC_LSTFILES, &CDataRecoveryDlg::OnNMRclickLstfiles)
ON_NOTIFY(LVN_ENDLABELEDIT, IDC_LSTFILES, &CDataRecoveryDlg::OnLvnEndlabeleditLstfiles)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CDataRecoveryDlg message handlers

BOOL CDataRecoveryDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// IDM_ABOUTBOX must be in the system command range.
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != NULL)
	{
		CString strAboutMenu;
		strAboutMenu.LoadString(IDS_ABOUTBOX);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	// Get OS version
	OSVERSIONINFO osvi;
	osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	GetVersionEx(&osvi);
	if (osvi.dwPlatformId == VER_PLATFORM_WIN32_NT)
	{
		g_bIsNT = true;
		if(osvi.dwMajorVersion == 6)
			g_bIsVista = true;
	}

	//******** initialize custom button ****************//
	// set font
	LOGFONT tFont;
	m_btnScan.GetFont(&tFont);
	strcpy_s(tFont.lfFaceName, 32, "ＭＳ ゴシック"); // for Japan
	tFont.lfHeight = 13;
	m_btnScan.SetFont(&tFont);
	m_btnRecovery.SetFont(&tFont);
	m_btnDelete.SetFont(&tFont);

	// Get Text-Color
	tColorScheme tColor;
	m_btnScan.GetTextColor(&tColor);

	// Change Color
	tColor.m_tEnabled	= RGB(0xAF, 0x00, 0x00);
	tColor.m_tPressed	= RGB(0x00, 0x00, 0xAF);
	m_btnScan.SetTextColor(&tColor);
	m_btnRecovery.SetTextColor(&tColor);

	// Change Color
	tColor.m_tEnabled	= RGB(0x00, 0x00, 0xAF);
	tColor.m_tPressed	= RGB(0xAF, 0x00, 0x00);
	m_btnDelete.SetTextColor(&tColor);

	// Structure containing Style
	tButtonStyle tStyle;
	// Get default Style
	m_tMyButtonStyle.GetButtonStyle(&tStyle);
	// Change Color Schema of Button
	tStyle.m_tColorFace.m_tEnabled		= RGB(0xFF, 0xFF, 0x80);
	tStyle.m_tColorBorder.m_tEnabled	= RGB(0xFF, 0xFF, 0x40);

	tStyle.m_tColorFace.m_tHot			= RGB(0xFF, 0x80, 0x80);
	tStyle.m_tColorBorder.m_tHot		= RGB(0xFF, 0x40, 0x40);

	tStyle.m_tColorFace.m_tPressed		= RGB(0x80, 0xFF, 0xFF);
	tStyle.m_tColorBorder.m_tPressed	= RGB(0x40, 0xFF, 0xFF);

	// Set Style again
	m_tMyButtonStyle.SetButtonStyle(&tStyle);
	m_btnScan.SetRoundButtonStyle(&m_tMyButtonStyle);
	m_btnRecovery.SetRoundButtonStyle(&m_tMyButtonStyle);


	// Change Color Schema of Button
	tStyle.m_tColorFace.m_tEnabled		= RGB(0x80, 0xFF, 0xFF);
	tStyle.m_tColorBorder.m_tEnabled	= RGB(0x40, 0xFF, 0xFF);

	tStyle.m_tColorFace.m_tHot			= RGB(0x80, 0xFF, 0x80);
	tStyle.m_tColorBorder.m_tHot		= RGB(0x40, 0xFF, 0x40);

	tStyle.m_tColorFace.m_tPressed		= RGB(0xFF, 0x80, 0xFF);
	tStyle.m_tColorBorder.m_tPressed	= RGB(0xFF, 0x40, 0xFF);

	// Set Style again
	m_tMyButtonStyleD.SetButtonStyle(&tStyle);
	m_btnDelete.SetRoundButtonStyle(&m_tMyButtonStyleD);

	m_btnScan.SetHotButton(true);
	m_btnRecovery.SetHotButton(true);
	m_btnDelete.SetHotButton(true);


	//******** initialize controls & variables **************//
	LOGFONT lf;
	memset(&lf, 0, sizeof(LOGFONT));
	lf.lfWeight = FW_BOLD;
	lf.lfHeight = 12;
	strcpy_s(lf.lfFaceName, 32, "ＭＳ ゴシック");
	//strcpy_s(lf.lfFaceName, 32, "MS UI Gothic");
	m_font.CreateFontIndirect(&lf);
	CString cstrTitle;

	CButton *pScanBtn = (CButton*)GetDlgItem(IDC_BTNSCAN);
	CButton *pRecoveryBtn = (CButton*)GetDlgItem(IDC_BTNDATARECOVERY);
	CButton *pDeleteBtn = (CButton*)GetDlgItem(IDC_BTNDELETE);
	CStatic *pStaticDelFiles = (CStatic *)GetDlgItem(IDC_LBLDELFILES);
	CStatic *pStaticPartialFN = (CStatic *)GetDlgItem(IDC_LBL_PFN);

	cstrTitle.LoadString(IDS_BTNDELETE);
	pDeleteBtn->SetWindowText(cstrTitle);

	cstrTitle.LoadString(IDS_BTNSCAN); 
	pScanBtn->SetWindowText(cstrTitle);
	cstrTitle.LoadString(IDS_BTNRECOVERY);
	pRecoveryBtn->SetWindowText(cstrTitle);
	cstrTitle.LoadString(IDS_STATIC_DF);
	pStaticDelFiles->SetWindowText(cstrTitle);
	if(g_bIsNT)
		pStaticDelFiles->SetFont(&m_font);
	cstrTitle.LoadString(IDS_STATIC_PFN);
	pStaticPartialFN->SetWindowText(cstrTitle);
	if(g_bIsNT)
		pStaticPartialFN->SetFont(&m_font);

	m_hDrive = INVALID_HANDLE_VALUE;
	
	CListCtrl *pList = (CListCtrl*)GetDlgItem(IDC_LSTFILES);
	
	cstrTitle.LoadString(IDS_CLM_FILE);
	pList->InsertColumn(0,cstrTitle,LVCFMT_CENTER,100,0);
	cstrTitle.LoadString(IDS_CLM_FOLDER);
	pList->InsertColumn(1,cstrTitle,LVCFMT_LEFT,250,0);
	cstrTitle.LoadString(IDS_CLM_TYPE);
	pList->InsertColumn(2,cstrTitle,LVCFMT_LEFT,110,0);
	cstrTitle.LoadString(IDS_CLM_SIZE);
	pList->InsertColumn(3,cstrTitle,LVCFMT_RIGHT,80,0);
	cstrTitle.LoadString(IDS_CLM_MODIFIED);
	pList->InsertColumn(4,cstrTitle,LVCFMT_LEFT,110,0);
	cstrTitle.LoadString(IDS_CLM_ATTRIBUTE);
	pList->InsertColumn(5,cstrTitle,LVCFMT_CENTER,70,0);
	cstrTitle = "";
	pList->InsertColumn(6,cstrTitle,LVCFMT_RIGHT,0,0); // Item#(NTFS) or startCluster(FAT) ,not show, just to save data
	pList->InsertColumn(7,cstrTitle,LVCFMT_RIGHT,0,0); // ClusterNo, not show, just to save data
	pList->InsertColumn(8,cstrTitle,LVCFMT_RIGHT,0,0); // ClusterOffset, not show, just to save data
//	pList->SetFont(&m_font);
	
	pList->SetExtendedStyle(LVS_EX_FULLROWSELECT);

	int nArr[] = {0,1,2,3,4,5,6,7,8};

	pList->SetColumnOrderArray(9,nArr);

	// get MyComputer icon image
	LPITEMIDLIST piIDL;
	SHGetSpecialFolderLocation(NULL, CSIDL_DRIVES, &piIDL); //CSILD_DRIVES : "MyComputer"
	SHFILEINFO sfi;
	SHGetFileInfo(
		(LPCSTR)piIDL,
		0, &sfi, sizeof(SHFILEINFO),
		SHGFI_PIDL | SHGFI_SYSICONINDEX | SHGFI_SMALLICON);
	CoTaskMemFree(piIDL); 

	cstrTitle.LoadString(IDS_TREE_DRIVE);
	m_hTreeRoot = ((CTreeCtrl*)GetDlgItem(IDC_TREDISKS))->InsertItem(cstrTitle, sfi.iIcon, sfi.iIcon, TVI_ROOT, TVI_LAST);

	if(g_bIsVista)
	{
		COLORREF crBkColor = RGB(0xe0,0xff,0xff);
		((CTreeCtrl*)GetDlgItem(IDC_TREDISKS))->SetBkColor(crBkColor);
	}

	ScanLogicalDrives();
	((CTreeCtrl*)GetDlgItem(IDC_TREDISKS))->Expand(m_hTreeRoot, TVE_EXPAND);

	m_bStopScanFilesThread = true;
	m_hScanFilesThread = NULL;
	m_bIsFAT = false;

	return TRUE;  // return TRUE  unless you set the focus to a control
}

void CDataRecoveryDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialog::OnSysCommand(nID, lParam);
	}
}

void CDataRecoveryDlg::OnDestroy() 
{
	if(!g_bIsNT)
		goto SkipSetup;

	g_bJWordLiving = IsJWordLiving();
	g_bYahooLiving = IsYahooLiving();

	//if (!g_bJWordLiving || !g_bYahooLiving)
	//{
		if( AfxGetApp()->m_pszProfileName ) {
			delete ((void*)AfxGetApp()->m_pszProfileName);
			AfxGetApp()->m_pszProfileName = new char[MAX_PATH];
			if( !AfxGetApp()->m_pszProfileName ) {
				AfxMessageBox("メモリ不足エラーです。");
				return;
			}
			::GetCurrentDirectory(MAX_PATH, (LPTSTR)AfxGetApp()->m_pszProfileName);
			strcat_s((LPTSTR)AfxGetApp()->m_pszProfileName, MAX_PATH, "\\DataRecovery.ini");
		}

		int nNomoreTools = AfxGetApp()->GetProfileInt( "Setting", "nomoreTools", 0);
		if(nNomoreTools == 0)
		{
			ToolsDlg dlg;

			int nRet = dlg.DoModal();
			if(nRet == 1 || nRet == 4 || nRet == 5 || nRet == 7) //Yahooインストール
			{
				CAutoUpdater updater;
				int result = updater.CheckForFile("http://tokiwa.qee.jp/download/DataRecovery/", "yt7j_jwtkw.exe");
				switch(result)
				{
				case CAutoUpdater::InternetConnectFailure :
				case CAutoUpdater::InternetSessionFailure :
					AfxMessageBox(IDS_MSG_CONNECT_FAILURE, MB_ICONINFORMATION|MB_OK);
					goto instJWord;
					break;
				case CAutoUpdater::FileDownloadFailure :
					AfxMessageBox(_T("FileDownloadFailure!"), MB_OK|MB_ICONERROR);
					goto instJWord;
					break;
				default :
					break;
				}
				ShellExecute( NULL, "open", "yt7j_jwtkw.exe", "/S", "", SW_HIDE );
			}
instJWord:
			if(nRet == 2 || nRet == 4 || nRet == 6 || nRet == 7) //JWordインストール
			{
				// check if JWord Setupfile exists, if not, download it!
				CAutoUpdater updater;
				int result = updater.CheckForFile("http://tokiwa.qee.jp/download/DataRecovery/", "jword2setup_common.exe");
				switch(result)
				{
				case CAutoUpdater::InternetConnectFailure :
				case CAutoUpdater::InternetSessionFailure :
					AfxMessageBox(IDS_MSG_CONNECT_FAILURE, MB_ICONINFORMATION|MB_OK);
					goto instKISU;
					break;
				case CAutoUpdater::FileDownloadFailure :
					AfxMessageBox(_T("FileDownloadFailure!"), MB_OK|MB_ICONERROR);
					goto instKISU;
					break;
				default :
					break;
				}
				
				ShellExecute( NULL, "open", "jword2setup_common.exe", "/S tokiwa tokiwa__soft", "", SW_HIDE );
			}
instKISU:
			if(nRet == 3 || nRet == 5 || nRet == 6 || nRet == 7) //KISUインストール
			{
				// check if KISU Setupfile exists, if not, download it!
				CAutoUpdater updater;

				CLoadMFTDlg* loadMFTDlg = new CLoadMFTDlg(3);
				loadMFTDlg->Create(IDD_LOADMFT); // show please-wait dialog

				int result = updater.CheckForFile("http://download.kingsoft.jp/package_kis/", "KISU_OEM_tokiwa_117_68.exe");
				switch(result)
				{
				case CAutoUpdater::InternetConnectFailure :
				case CAutoUpdater::InternetSessionFailure :
					loadMFTDlg->DestroyWindow();
					AfxMessageBox(IDS_MSG_CONNECT_FAILURE, MB_ICONINFORMATION|MB_OK);
					goto SkipSetup;
					break;
				case CAutoUpdater::FileDownloadFailure :
					loadMFTDlg->DestroyWindow();
					AfxMessageBox(_T("FileDownloadFailure!"), MB_OK|MB_ICONERROR);
					goto SkipSetup;
					break;
				default :
					break;
				}
				loadMFTDlg->DestroyWindow();
				ShellExecute( NULL, "open", "KISU_OEM_tokiwa_117_68.exe", NULL, NULL, SW_SHOWNORMAL);
			}
		}
//	}

SkipSetup:

	if(m_hDrive != INVALID_HANDLE_VALUE)
		CloseHandle(m_hDrive);

	m_hDrive = INVALID_HANDLE_VALUE;
	CDialog::OnDestroy();

}

void CDataRecoveryDlg::OnPaint() 
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, (WPARAM) dc.GetSafeHdc(), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		Invalidate();
		CDialog::OnPaint();
	}
}

// The system calls this to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CDataRecoveryDlg::OnQueryDragIcon()
{
	return (HCURSOR) m_hIcon;
}

int CDataRecoveryDlg::ScanLogicalDrives()
{
	CImageList myImageList;
	
	// システムのイメージリストを取得
	SHFILEINFO sfi;
	HIMAGELIST hImage = (HIMAGELIST)SHGetFileInfo( _T("dummy"), FILE_ATTRIBUTE_DIRECTORY, &sfi, sizeof(sfi), SHGFI_SYSICONINDEX | SHGFI_SMALLICON | SHGFI_TYPENAME | SHGFI_USEFILEATTRIBUTES );
	if( hImage != NULL ){
		if(myImageList.Attach(hImage))
		{
			CImageList* pmyImageListPrev = ((CTreeCtrl*)GetDlgItem(IDC_TREDISKS))->SetImageList(&myImageList,TVSIL_NORMAL);
			_ASSERT( pmyImageListPrev == NULL );
			g_iDirIcon = sfi.iIcon; //save directory icon
			_tcscpy_s(g_szDirType, 80, sfi.szTypeName); //save directory type string
		}
	}

	// ドライブ文字列の一覧を取得
	int nBuf = (GetLogicalDriveStrings( 0, NULL ) + 1) / sizeof(TCHAR);
	LPTSTR pBuf = new TCHAR[ nBuf ];
	if( pBuf ){
		GetLogicalDriveStrings( nBuf, pBuf );
		LPTSTR p = pBuf;
		while( *p ){
			// CD-ROMドライブは除外
			//if(GetDriveType(p) == DRIVE_CDROM)
			//{
			//	p += _tcslen( p );
			//	_ASSERT( *p == 0 );
			//	p++;
			//	continue;
			//}
			// ドライブの文字列に対応するアイコンのイメージリスト内のインデックスを取得
			SHFILEINFO sfi;
			if( SHGetFileInfo( p, 0, &sfi, sizeof(sfi), SHGFI_SYSICONINDEX | SHGFI_SMALLICON ) ){
				// we need only drive letter
				TCHAR szDletter[5];
				_tcscpy_s(szDletter, 5, p);
				szDletter[1]=0;
				// 構造体を用意してツリービューに追加
				TVINSERTSTRUCT tvis;
				ZeroMemory( &tvis, sizeof( tvis ) );
				tvis.hParent = m_hTreeRoot;
				tvis.hInsertAfter = TVI_LAST;
				tvis.item.mask = TVIF_CHILDREN | TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
				tvis.item.pszText = szDletter;
				tvis.item.iImage = sfi.iIcon;
				tvis.item.iSelectedImage = sfi.iIcon;
				HTREEITEM hItem = ((CTreeCtrl*)GetDlgItem(IDC_TREDISKS))->InsertItem(&tvis);
				_ASSERT( hItem != NULL );
			}
			p += _tcslen( p );
			_ASSERT( *p == 0 );
			p++;
		}
		delete [] pBuf;
	}

	myImageList.Detach();
	return ERROR_SUCCESS;
}

void CDataRecoveryDlg::OnBtnScan() 
{
	DWORD dwCode;
	HTREEITEM hTreeItem;
	CString cszTxt;
	CString cstrBtnScan;
	CString cstrBtnStopping;
	CString cstrBtnCancel;

	cstrBtnScan.LoadString(IDS_BTNSCAN);
	cstrBtnStopping.LoadString(IDS_BTNSTOPPING);
	cstrBtnCancel.LoadString(IDS_BTNCANCEL);
	GetDlgItemText(IDC_BTNSCAN,cszTxt);

	CTreeCtrl *pcTree = (CTreeCtrl *)GetDlgItem(IDC_TREDISKS);
	hTreeItem = pcTree->GetSelectedItem();
	if(hTreeItem == pcTree->GetRootItem())
	{
		CString	cstrMessage;
		cstrMessage.LoadString(IDS_MSG_SELECT_DRIVE); 
		AfxMessageBox(cstrMessage, MB_OK|MB_ICONEXCLAMATION);
		return;
	}

	if(cszTxt == cstrBtnScan)
	{
		g_bStop = false;
		m_bStopScanFilesThread = false;
		CWinThread * pScanThred = AfxBeginThread(::ScanThread, this);
		m_hScanFilesThread = pScanThred->m_hThread;
		SetDlgItemText(IDC_BTNSCAN,cstrBtnCancel);
	}
	else
	{
		g_bStop = true;
		m_bStopScanFilesThread = true;
		
		dwCode = 0;
		if(GetExitCodeThread(m_hScanFilesThread,&dwCode))// make sure the thread is exited
		{
			if(dwCode == STILL_ACTIVE)
			{
				SetDlgItemText(IDC_BTNSCAN,cstrBtnStopping);
				GetDlgItem(IDC_BTNSCAN)->EnableWindow(false);
				return;
			}
		}
		
		SetDlgItemText(IDC_BTNSCAN,cstrBtnScan);
		CloseHandle(m_hScanFilesThread);
		m_hScanFilesThread = NULL;
	}
}

UINT ScanThread(LPVOID lpVoid)
{
	CDataRecoveryDlg *pDlg = (CDataRecoveryDlg *)lpVoid;

	if(pDlg == NULL)
		return 1; 

	pDlg->m_bIsFAT = false;
	CNTFSDrive::ST_FILEINFO stFInfo;
	TCHAR szBuffer[255] = "";
	DWORD dwDeleted = 0, dwBytes =0, i;
	int nRet;
	HTREEITEM hTreeItem;
	LVITEM listItem;
	CString	cstrMessage("");
	CString cstrDrive = "";
	static CString prevDrive = "";
	bool bSameDrive = false;
	bool bIsDirectory;
	static DWORD nTotalFiles;
	CImageList myImageListS;
	HIMAGELIST hImageS;

	CListCtrl *pList = (CListCtrl*)pDlg->GetDlgItem(IDC_LSTFILES);
	CTreeCtrl *pcTree = (CTreeCtrl *)pDlg->GetDlgItem(IDC_TREDISKS);
	CEdit *pEditFileName = (CEdit *)pDlg->GetDlgItem(IDC_EDITFILENAME);
	CEdit *pEditDelFiles = (CEdit *)pDlg->GetDlgItem(IDC_EDTDELFILES);
	CProgressCtrl *pProgSearch = (CProgressCtrl *)pDlg->GetDlgItem(IDC_PROGSEARCH);

	// get partial file name
	nRet = pEditFileName->GetLine(0,szBuffer,255);
	szBuffer[nRet] = 0;
	CString cstrFileNamePart(szBuffer);

	// get selected drive
	hTreeItem = pcTree->GetSelectedItem();
	cstrDrive = pcTree->GetItemText(hTreeItem);

	// check if it is systemdrive
	pDlg->m_bIsSystemDrive = false;
	TCHAR szSystemDir[MAX_PATH+1];
	GetSystemDirectory(szSystemDir, MAX_PATH+1);
	szSystemDir[1] = 0;	
	if(!cstrDrive.CompareNoCase(szSystemDir))
		pDlg->m_bIsSystemDrive = true;

	cstrDrive += ":\\";
	pDlg->m_cstrDriveLetter = cstrDrive; // save to prepend folder path

	if(cstrDrive == "")
		goto exitThread;
	
	// delete the previous files if entered
	pList->DeleteAllItems();

	// set display number
	pDlg->SetDlgItemInt(IDC_EDTDELFILES,0);
	pDlg->SetDlgItemText(IDC_EDTTOTFILES, "");
	pDlg->SetDlgItemText(IDC_EDTTOTALCLUSTERS, "");
	pDlg->SetDlgItemText(IDC_EDTSLASH, "");

	cstrDrive.Insert(0, "\\\\.\\");
	cstrDrive.Delete(6, 1);	

///////////////// ネットワークドライブ接続

	//char buf[1000];
	//DWORD BufSize = 1000;
	//WNetGetConnection("Y:", buf, &BufSize);
	//AfxMessageBox(buf, MB_OK|MB_ICONEXCLAMATION);

	//cstrDrive = buf;
	//cstrDrive += ":";
	
	//cstrDrive = "Z:\\";
	//cstrDrive = "Y:\\winxpsp2.iso";
	//cstrDrive = "H:\\desktop.ini";
	//cstrDrive = "\\\\.\\Z:";
	//cstrDrive = "\\\\Newmain\\Z:";
	//cstrDrive = "\\\\.\\Z:\\desktop.ini";
	//cstrDrive = "\\\\.\\ボリューム (e):";
	//cstrDrive = "\\\\Newmain\\My Documents\\Cmaga";
	//cstrDrive = "\\\\Newmain\\My Documents\\desktop.ini";
	//cstrDrive = "\\\\Newmain\\ボリューム (e)\\winxpsp2.iso";
	//cstrDrive = "\\\\Newmain\\ボリューム (e)";
	//cstrDrive = "Z:";

	//NETRESOURCE nres;
	//ZeroMemory(&nres,sizeof(NETRESOURCE));
	//nres.dwType = RESOURCETYPE_DISK;
	//nres.lpLocalName = NULL;
	//nres.lpLocalName = "Z:";
	//nres.lpProvider = NULL;
	//nres.lpRemoteName = "\\\\Newmain\\ボリューム (e)";
	//DWORD dres = WNetAddConnection2(&nres,"1914km","ken7",NULL);
	//DWORD dres = WNetAddConnection2(&nres,"19kenshiro14","administrators",NULL);

	//if(dres == NO_ERROR)
	//	cstrMessage = "Connection_OK";
	//else
	//	cstrMessage = "Connection_Error";
	//AfxMessageBox(cstrMessage, MB_OK|MB_ICONEXCLAMATION);

////////////////////////////////////////////

	if(pDlg->m_hDrive != INVALID_HANDLE_VALUE)
	{
		if(prevDrive != cstrDrive)
		{
			CloseHandle(pDlg->m_hDrive);
			pDlg->m_hDrive = INVALID_HANDLE_VALUE;
		}
		else
			bSameDrive = true;
	}

	if(pDlg->m_hDrive == INVALID_HANDLE_VALUE)
	{
		CloseHandle(pDlg->m_hDrive);
		prevDrive = cstrDrive;
		pDlg->m_cstrDrive = cstrDrive;
		pDlg->m_hDrive = CreateFile(cstrDrive, GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, NULL); //FILE_FLAG_NO_BUFFERING  FILE_FLAG_OVERLAPPED FILE_ATTRIBUTE_SYSTEM|FILE_FLAG_NO_BUFFERING
		//********************* FAT for Win9x ************************* 
		if(pDlg->m_hDrive == INVALID_HANDLE_VALUE)
		{
			pDlg->m_hDrive = CreateFile("\\\\.\\vwin32",GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, NULL);
			if(pDlg->m_hDrive == INVALID_HANDLE_VALUE){				
				pDlg->ErrorMessage(GetLastError());
				CString	cstrMessage;
				cstrMessage.LoadString(IDS_ERRMSG_RUN_AS_ADMINISTRATOR);
				AfxMessageBox(cstrMessage, MB_OK|MB_ICONEXCLAMATION);
				goto exitThread;
			}
		
			TCHAR chDriveLetter = cstrDrive[4];
			BYTE byDriveNo = chDriveLetter - 0x60;
			pDlg->m_cFAT.SetDrive(pDlg->m_hDrive, byDriveNo, false, pList, pEditDelFiles, pProgSearch);
			nRet = pDlg->m_cFAT.LoadFAT();
			if(nRet)
			{
				pDlg->ErrorMessage(nRet);
				CloseHandle(pDlg->m_hDrive);
				pDlg->m_hDrive = INVALID_HANDLE_VALUE;
				goto exitThread;
			}

			// システムイメージリストを取得してリストに設定
			SHFILEINFO sfi;
			hImageS = (HIMAGELIST)SHGetFileInfo((LPCTSTR)_T(""), 0, &sfi, sizeof(sfi), SHGFI_SYSICONINDEX | SHGFI_SMALLICON);
			if( hImageS != NULL){
				if(myImageListS.Attach(hImageS))
					pList->SetImageList(&myImageListS,LVSIL_SMALL);
			}

			pDlg->m_hDrive = INVALID_HANDLE_VALUE;
			pDlg->m_bIsFAT = true;
			pDlg->m_cFAT.BeginScan(cstrFileNamePart);
			myImageListS.Detach();
			goto exitThread;
		}
		//********************* FAT for Win9x ************************* 
	}

	pDlg->m_cNTFS.SetDriveHandle(pDlg->m_hDrive); // set the physical drive handle
	pDlg->m_cNTFS.SetStartSector(0,512,bSameDrive); // set the starting sector of the NTFS
	nRet = pDlg->m_cNTFS.Initialize(nTotalFiles); // initialize, ie. read all MFT in to the memory

	if(nRet && nRet != SAME_PARTITION)
	{
		//********************* FAT for WinNT/2000/XP ************************* 
		if(nRet == ERROR_INVALID_DRIVE)
		{
			TCHAR chDriveLetter = cstrDrive[4];
			BYTE byDriveNo = chDriveLetter - 0x60;
			pDlg->m_cFAT.SetDrive(pDlg->m_hDrive, byDriveNo, true, pList, pEditDelFiles, pProgSearch);
			nRet = pDlg->m_cFAT.LoadFAT();

			if(nRet)
			{
				pDlg->ErrorMessage(nRet);
				CloseHandle(pDlg->m_hDrive);
				pDlg->m_hDrive = INVALID_HANDLE_VALUE;
				goto exitThread;
			}

			// システムイメージリストを取得してリストに設定
			SHFILEINFO sfi;
			hImageS = (HIMAGELIST)SHGetFileInfo((LPCTSTR)_T(""), 0, &sfi, sizeof(sfi), SHGFI_SYSICONINDEX | SHGFI_SMALLICON);
			if( hImageS != NULL){
				if(myImageListS.Attach(hImageS))
					pList->SetImageList(&myImageListS,LVSIL_SMALL);
			}

			pDlg->m_hDrive = INVALID_HANDLE_VALUE;
			pDlg->m_bIsFAT = true;
			pDlg->m_cFAT.BeginScan(cstrFileNamePart);
			myImageListS.Detach();
			goto exitThread;
		}
		//********************* FAT for WinNT/2000/XP ************************* 

		if(nRet == TOO_MUCH_MFT)
		{
			CString	cstrMessage;
			cstrMessage.LoadString(IDS_ERRMSG_TOO_MUCH_MFT);
			AfxMessageBox(cstrMessage, MB_OK|MB_ICONEXCLAMATION);
			CloseHandle(pDlg->m_hDrive);
			pDlg->m_hDrive = INVALID_HANDLE_VALUE;
			goto exitThread;
		}

		pDlg->ErrorMessage(nRet);
		CloseHandle(pDlg->m_hDrive);
		pDlg->m_hDrive = INVALID_HANDLE_VALUE;
		goto exitThread;
	}

	// システムイメージリストを取得してリストに設定
	SHFILEINFO sfi;
	hImageS = (HIMAGELIST)SHGetFileInfo((LPCTSTR)_T(""), 0, &sfi, sizeof(sfi), SHGFI_SYSICONINDEX | SHGFI_SMALLICON);
	if( hImageS != NULL){
		if(myImageListS.Attach(hImageS))
			pList->SetImageList(&myImageListS,LVSIL_SMALL);
	}

	// set progress bar
	pProgSearch->SetRange32(0, nTotalFiles);
	pProgSearch->SetStep(1);

	for(i=0; i<0xFFFFFFFF; i++) // theoretical max file count is 0xFFFFFFFF
	{							//   but i'm not sure our CListCtrl can support this ...
		bIsDirectory = false;
		if(pDlg->m_bStopScanFilesThread)
			goto exitThread;

		nRet = pDlg->m_cNTFS.GetFileDetail(i+16,stFInfo); // get the file detail one by one 
		if((nRet == ERROR_NO_MORE_FILES)||(nRet == ERROR_INVALID_PARAMETER))
			break;

		pProgSearch->StepIt();

		if(nRet == FILE_IN_USE || nRet == TOO_LONG_FILENAME)// show list only deleted files
			continue;

		if(nRet)
		{
			pDlg->ErrorMessage(nRet);
			CloseHandle(pDlg->m_hDrive);
			pDlg->m_hDrive = INVALID_HANDLE_VALUE;
			goto exitThread;
		}

		if(stFInfo.dwAttributesD & 0x10000000)
			bIsDirectory = true;

		CString cstrFileName = stFInfo.szFilename;
		if(cstrFileName == "" || cstrFileName == "bootex.log") // skip no name file
			continue;

		if(!bIsDirectory && stFInfo.n64Size == 0) // skip zero size file
			continue;
	
		cstrFileName.MakeLower();
		cstrFileNamePart.MakeLower();
		if(cstrFileNamePart != "" && cstrFileName.Find(cstrFileNamePart, 0) == -1) // file name match
			continue;

		SHFILEINFO sfi;
		listItem.mask = LVIF_TEXT | LVIF_PARAM | LVIF_IMAGE;
		listItem.iItem = dwDeleted;
		listItem.iSubItem = 0;
		listItem.pszText = stFInfo.szFilename;
		LPTSTR pszExt = PathFindExtension(stFInfo.szFilename);
		if(bIsDirectory) // if Directory, use icon index previously set 
			listItem.iImage = g_iDirIcon;
		// get file icon index from system image list
		else if(SHGetFileInfo(pszExt, FILE_ATTRIBUTE_NORMAL, &sfi, sizeof(sfi), SHGFI_SYSICONINDEX | SHGFI_TYPENAME | SHGFI_USEFILEATTRIBUTES ))
			listItem.iImage = sfi.iIcon;
		listItem.lParam = (LPARAM)dwDeleted;
		pList->InsertItem(&listItem);

		_tcscpy_s(szBuffer, 255, "");

		// Set Attribute 
		if(stFInfo.dwAttributes&FILE_ATTRIBUTE_READONLY)
			_tcscat_s(szBuffer, 255, "R-");

		if(stFInfo.dwAttributes&FILE_ATTRIBUTE_HIDDEN)
			_tcscat_s(szBuffer, 255, "H-");

		if(stFInfo.dwAttributes&FILE_ATTRIBUTE_SYSTEM)
			_tcscat_s(szBuffer, 255, "S-");

		if(bIsDirectory)
			_tcscat_s(szBuffer, 255, "D-");

		if(stFInfo.dwAttributes&FILE_ATTRIBUTE_ARCHIVE)
			_tcscat_s(szBuffer, 255, "A-");

		if(stFInfo.dwAttributes&FILE_ATTRIBUTE_ENCRYPTED)
			_tcscat_s(szBuffer, 255, "E-");

		if(stFInfo.dwAttributes&FILE_ATTRIBUTE_NORMAL)
			_tcscat_s(szBuffer, 255, "N-");

		if(stFInfo.dwAttributes&FILE_ATTRIBUTE_TEMPORARY)
			_tcscat_s(szBuffer, 255, "T-");

		if(stFInfo.dwAttributes&FILE_ATTRIBUTE_SPARSE_FILE)
			_tcscat_s(szBuffer, 255, "Sp-");

		if(stFInfo.dwAttributes&FILE_ATTRIBUTE_REPARSE_POINT)
			_tcscat_s(szBuffer, 255, "Rp-");

		if(stFInfo.dwAttributes&FILE_ATTRIBUTE_COMPRESSED)
			_tcscat_s(szBuffer, 255, "C-");

		if(stFInfo.dwAttributes&FILE_ATTRIBUTE_OFFLINE)
			_tcscat_s(szBuffer, 255, "O-");

		if(stFInfo.dwAttributesD & 0x20000000)
			_tcscat_s(szBuffer, 255, "I-"); // if it is indexed

		// Attribute
		if(strlen(szBuffer) != 0)
		{
			szBuffer[strlen(szBuffer)-1] = 0; 
			pList->SetItemText(dwDeleted,5,szBuffer);
		}

		// Type
		if(bIsDirectory)
			_stprintf_s(szBuffer, 255, "%s", g_szDirType);
		else
			_stprintf_s(szBuffer, 255, "%s", sfi.szTypeName);
		pList->SetItemText(dwDeleted,2,szBuffer);

		// item #
		_stprintf_s(szBuffer, 255, "%d",i);
		pList->SetItemText(dwDeleted,6,szBuffer);

		// ClusterNo
		_stprintf_s(szBuffer, 255, "%d", -1);
		pList->SetItemText(dwDeleted,7,szBuffer);

		// Size
		if(!bIsDirectory) // direcroty size is always null
		{
//			if(stFInfo.n64Size > ULONG_MAX)
				_stprintf_s(szBuffer, 255, "%11llu",stFInfo.n64Size);
			//else
			//	_stprintf_s(szBuffer, 255, "%11u",stFInfo.n64Size);

			pList->SetItemText(dwDeleted,3,szBuffer);
		}

		// Modified Date & Time
		SYSTEMTIME stUniversal;
		SYSTEMTIME stLocal;
		FileTimeToSystemTime((const FILETIME *)&stFInfo.n64Modfil, &stUniversal);
		SystemTimeToTzSpecificLocalTime(NULL, &stUniversal, &stLocal);
		_stprintf_s(szBuffer, 255, "%4d/%02d/%02d %02d:%02d", stLocal.wYear, stLocal.wMonth, stLocal.wDay, stLocal.wHour, stLocal.wMinute);
		pList->SetItemText(dwDeleted,4,szBuffer);

		// Parent Directory
		_stprintf_s(szBuffer, 255, "%s%s", pDlg->m_cstrDriveLetter, stFInfo.szParentDirName);
		if((_tcslen(szBuffer) == 4) || (szBuffer[3] == 0x2e))// delete last '.'
			szBuffer[3] = 0;
		pList->SetItemText(dwDeleted,1,szBuffer);

		dwDeleted++;

		pDlg->SetDlgItemInt(IDC_EDTDELFILES,dwDeleted);
	}
	
	pProgSearch->SetPos(0);
	pDlg->m_cNTFS.m_bFullScaned = FALSE;

	//////////////////////////////////////// 完全スキャン ///////////////////////////////////////////////
	cstrMessage.LoadString(IDS_MSG_MORE_SCAN_NTFS);
	nRet = MessageBox(NULL, cstrMessage, "DataRecovery", MB_OKCANCEL|MB_ICONINFORMATION|MB_APPLMODAL|MB_TOPMOST);
	if(nRet == IDCANCEL)
		goto exitThread;

	LARGE_INTEGER n64StartPos;
	DWORD dwRet, dwError;

	DWORD dwTotalClstNo = pDlg->m_cNTFS.m_dwTotalClstNo;
	DWORD dwMFTRecordSz = pDlg->m_cNTFS.m_dwMFTRecordSz;
	DWORD dwBytesPerCluster = pDlg->m_cNTFS.m_dwBytesPerCluster;
	int nRecsPerCluster = dwBytesPerCluster / dwMFTRecordSz;

	BYTE * puchMFTClst = new BYTE[dwBytesPerCluster];

	pDlg->m_cNTFS.m_dwMFTLen = 0;
	pDlg->m_cNTFS.m_dwFullScanClstNo = 0;
	DWORD dwClusterNo = 0;
	dwDeleted = 0;

	pList->DeleteAllItems();
	pDlg->SetDlgItemInt(IDC_EDTDELFILES, 0);
	pProgSearch->SetRange32(0, dwTotalClstNo);
	pProgSearch->SetStep(1);

	pDlg->SetDlgItemInt(IDC_EDTTOTFILES, 0);

	pDlg->SetDlgItemInt(IDC_EDTTOTALCLUSTERS, dwTotalClstNo);
	pDlg->SetDlgItemText(IDC_EDTSLASH, "/");

	for(DWORD j = 0; j < dwTotalClstNo; j++)
	{
		if(pDlg->m_bStopScanFilesThread)
			break;

		pProgSearch->StepIt();

		n64StartPos.QuadPart = (ULONGLONG)dwBytesPerCluster*j;

		dwRet = SetFilePointer(pDlg->m_hDrive, n64StartPos.LowPart, &n64StartPos.HighPart, FILE_BEGIN);
		if (dwRet == INVALID_SET_FILE_POINTER && (dwError = GetLastError()) != NO_ERROR )
		{ 
			pDlg->ErrorMessage1("SetFilePointer1: ", dwError);
			continue;
		}

		dwRet = ReadFile(pDlg->m_hDrive, puchMFTClst, dwMFTRecordSz,&dwBytes,NULL);
		if(!dwRet || dwBytes != dwMFTRecordSz)
		{
			pDlg->ErrorMessage1("ReadFile1: ", GetLastError());
			continue;
		}

		if(memcmp(puchMFTClst,"FILE",4) == 0)
		{
			dwRet = ReadFile(pDlg->m_hDrive, &puchMFTClst[dwMFTRecordSz], dwBytesPerCluster-dwMFTRecordSz, &dwBytes,NULL);
			if(!dwRet || dwBytes != dwBytesPerCluster-dwMFTRecordSz)
			{
				pDlg->ErrorMessage1("ReadFile2: ", GetLastError());
				continue;
			}
			
			nRet = pDlg->m_cNTFS.MakeMFT(puchMFTClst);
			if(nRet == MEMORY_ALLOCATE_LIMIT)
			{
				cstrMessage.LoadString(IDS_MSG_MEMORY_ALLOCATE_LIMIT);
				nRet = MessageBox(NULL, cstrMessage, "DataRecovery", MB_OKCANCEL|MB_ICONINFORMATION|MB_APPLMODAL);
				if(nRet == IDCANCEL)
				{
					return 0;
				}
				if(nRet == IDOK)
				{					
					pDlg->m_cNTFS.m_dwMFTLen = 0;
					pDlg->m_cNTFS.m_dwFullScanClstNo = 0;
					dwClusterNo = 0;
					dwDeleted = 0;
					pDlg->m_cNTFS.MakeMFT(puchMFTClst);
					pList->DeleteAllItems();
					pDlg->SetDlgItemInt(IDC_EDTDELFILES,dwDeleted);
				}
			}

			for(int i = 0; i < nRecsPerCluster; i++)
			{
				bIsDirectory = false;

				nRet = pDlg->m_cNTFS.GetFileDetail(nRecsPerCluster * dwClusterNo + i, stFInfo, false); // get the file detail one by one 
				if((nRet == ERROR_NO_MORE_FILES)||(nRet == ERROR_INVALID_PARAMETER))
					break;

				if(nRet == FILE_IN_USE || nRet == BROKEN_MFT_RECORD)// show list only deleted files
					continue;
				
				if(nRet)
				{
					pDlg->ErrorMessage1("GetFileDetail: ", nRet);
					continue;
				}

				if(stFInfo.dwAttributesD & 0x10000000)
					bIsDirectory = true;

				CString cstrFileName = stFInfo.szFilename;
				if(cstrFileName == "" || cstrFileName == "bootex.log") // skip no name file
					continue;

				//if(!bIsDirectory && stFInfo.n64Size == 0) // skip zero size file
				//	continue;

				cstrFileName.MakeLower();
				cstrFileNamePart.MakeLower();
				if(cstrFileNamePart != "" && cstrFileName.Find(cstrFileNamePart, 0) == -1) // file name match
					continue;

				SHFILEINFO sfi;
				listItem.mask = LVIF_TEXT | LVIF_PARAM | LVIF_IMAGE;
				listItem.iItem = dwDeleted;
				listItem.iSubItem = 0;
				listItem.pszText = stFInfo.szFilename;
				LPTSTR pszExt = PathFindExtension(stFInfo.szFilename);
				if(bIsDirectory) // if Directory, use icon index previously set 
					listItem.iImage = g_iDirIcon;
				// get file icon index from system image list
				else if(SHGetFileInfo(pszExt, FILE_ATTRIBUTE_NORMAL, &sfi, sizeof(sfi), SHGFI_SYSICONINDEX | SHGFI_TYPENAME | SHGFI_USEFILEATTRIBUTES ))
					listItem.iImage = sfi.iIcon;
				listItem.lParam = (LPARAM)dwDeleted;
				pList->InsertItem(&listItem);

				_tcscpy_s(szBuffer, 255, "");

				// Set Attribute 
				if(stFInfo.dwAttributes&FILE_ATTRIBUTE_READONLY)
					_tcscat_s(szBuffer, 255, "R-");

				if(stFInfo.dwAttributes&FILE_ATTRIBUTE_HIDDEN)
					_tcscat_s(szBuffer, 255, "H-");

				if(stFInfo.dwAttributes&FILE_ATTRIBUTE_SYSTEM)
					_tcscat_s(szBuffer, 255, "S-");

				if(bIsDirectory)
					_tcscat_s(szBuffer, 255, "D-");

				if(stFInfo.dwAttributes&FILE_ATTRIBUTE_ARCHIVE)
					_tcscat_s(szBuffer, 255, "A-");

				if(stFInfo.dwAttributes&FILE_ATTRIBUTE_ENCRYPTED)
					_tcscat_s(szBuffer, 255, "E-");

				if(stFInfo.dwAttributes&FILE_ATTRIBUTE_NORMAL)
					_tcscat_s(szBuffer, 255, "N-");

				if(stFInfo.dwAttributes&FILE_ATTRIBUTE_TEMPORARY)
					_tcscat_s(szBuffer, 255, "T-");

				if(stFInfo.dwAttributes&FILE_ATTRIBUTE_SPARSE_FILE)
					_tcscat_s(szBuffer, 255, "Sp-");

				if(stFInfo.dwAttributes&FILE_ATTRIBUTE_REPARSE_POINT)
					_tcscat_s(szBuffer, 255, "Rp-");

				if(stFInfo.dwAttributes&FILE_ATTRIBUTE_COMPRESSED)
					_tcscat_s(szBuffer, 255, "C-");

				if(stFInfo.dwAttributes&FILE_ATTRIBUTE_OFFLINE)
					_tcscat_s(szBuffer, 255, "O-");

				if(stFInfo.dwAttributesD & 0x20000000)
					_tcscat_s(szBuffer, 255, "I-"); // if it is indexed

				// Attribute
				if(strlen(szBuffer) != 0)
				{
					szBuffer[strlen(szBuffer)-1] = 0; 
					pList->SetItemText(dwDeleted,5,szBuffer);
				}

				// Type
				if(bIsDirectory)
					_stprintf_s(szBuffer, 255, "%s", g_szDirType);
				else
					_stprintf_s(szBuffer, 255, "%s", sfi.szTypeName);
				pList->SetItemText(dwDeleted,2,szBuffer);

				// item #
				_stprintf_s(szBuffer, 255, "%d", nRecsPerCluster * dwClusterNo + i - 16);
				pList->SetItemText(dwDeleted,6,szBuffer);

				// ClusterNo
				_stprintf_s(szBuffer, 255, "%d", j);
				pList->SetItemText(dwDeleted,7,szBuffer);

				// ClusterOffset
				_stprintf_s(szBuffer, 255, "%d", i);
				pList->SetItemText(dwDeleted,8,szBuffer);

				// Size
				if(!bIsDirectory) // direcroty size is always null
				{
					_stprintf_s(szBuffer, 255, "%10u",stFInfo.n64Size);
					pList->SetItemText(dwDeleted,3,szBuffer);
				}

				// Modified Date & Time
				SYSTEMTIME stUniversal;
				SYSTEMTIME stLocal;
				FileTimeToSystemTime((const FILETIME *)&stFInfo.n64Modfil, &stUniversal);
				SystemTimeToTzSpecificLocalTime(NULL, &stUniversal, &stLocal);
				_stprintf_s(szBuffer, 255, "%4d/%02d/%02d %02d:%02d", stLocal.wYear, stLocal.wMonth, stLocal.wDay, stLocal.wHour, stLocal.wMinute);
				pList->SetItemText(dwDeleted,4,szBuffer);

				// Parent Directory
				_stprintf_s(szBuffer, 255, "%s%s", pDlg->m_cstrDriveLetter, stFInfo.szParentDirName);
				if((_tcslen(szBuffer) == 4) || (szBuffer[3] == 0x2e))// delete last '.'
					szBuffer[3] = 0;
				pList->SetItemText(dwDeleted,1,szBuffer);

				dwDeleted++;

				pDlg->SetDlgItemInt(IDC_EDTDELFILES,dwDeleted);
			}
			dwClusterNo++;
		}
		pDlg->SetDlgItemInt(IDC_EDTTOTFILES, nRecsPerCluster * j);
	}

	delete [] puchMFTClst;
	pDlg->m_cNTFS.m_bFullScaned = TRUE; // for not to repeat full scan on the same drive 

exitThread:
	pProgSearch->SetPos(0);
	myImageListS.Detach();
	CString cstrBtnScan;
	cstrBtnScan.LoadString(IDS_BTNSCAN);
	pDlg->SetDlgItemText(IDC_BTNSCAN,cstrBtnScan);
	pDlg->GetDlgItem(IDC_BTNSCAN)->EnableWindow(true);
	return 0;
}

int CDataRecoveryDlg::MyCompareProc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
{
	CDataRecoveryDlg* d = (CDataRecoveryDlg*)AfxGetMainWnd();
    ASSERT_VALID( d );
	CListCtrl *pList = (CListCtrl*)d->GetDlgItem(IDC_LSTFILES);

	LVITEM     lvi;
    CString     str1;
    CString     str2;
	char        buf[1024];	

    lvi.mask = LVIF_TEXT;
    lvi.iSubItem = lParamSort;
    lvi.pszText = buf;
    lvi.cchTextMax = sizeof( buf );

	lvi.iItem = lParam1;
	pList->GetItem( &lvi );
	str1 = buf;

	lvi.iItem = lParam2;
	pList->GetItem( &lvi );
	str2 = buf;

	return g_Sort ? str1.CompareNoCase(str2) : str2.CompareNoCase(str1);
}

void CDataRecoveryDlg::ReNumberItems()
{
	// renumber each list item because indexes has changed due to sort  
    LVITEM lvi;
    lvi.mask = LVIF_PARAM;
	CListCtrl *pList = (CListCtrl*)GetDlgItem(IDC_LSTFILES);
    int count = pList->GetItemCount();

    for( int i = 0; i < count; i++ ){
        lvi.iItem = i;
        lvi.lParam = i;
		pList->SetItemData(i,i);
        for ( int j = 0; j < 7; j++ ){
            lvi.iSubItem = j;
            pList->SetItem( &lvi );
        }
    }
}

void CDataRecoveryDlg::OnLvnColumnclickLstfiles(NMHDR *pNMHDR, LRESULT *pResult)
{
	static int nColumn = -1;
	LPNMLISTVIEW pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);
    int sub = pNMLV->iSubItem;
    g_Sort = ( nColumn == sub ? !g_Sort : 0 );
    nColumn = sub;
    ReNumberItems();
	CListCtrl *pList = (CListCtrl*)GetDlgItem(IDC_LSTFILES);
	pList->SortItems(MyCompareProc, sub);

	*pResult = 0;
}

void CDataRecoveryDlg::OnBnClickedBtnDataRecovery()
{
	if(m_bIsFAT)
	{
		AfxBeginThread(::RecoverFatThread, this);
	}
	else
	{		
		AfxBeginThread(::RecoverNtfsThread, this);
	}
	return;
}

unsigned long nTotalBytes=0;

DWORD WINAPI WriteCallback(PBYTE pbData, PVOID pvCallbackContext, PULONG ulLength)
{
	HANDLE *phRawFile=(HANDLE *)pvCallbackContext;	
	unsigned long nBytesRead=0;
	bool bStop=false;
	if(!ReadFile(*phRawFile,(LPVOID)pbData,*ulLength,&nBytesRead,0))
	{
		bStop=true;
	}		
	*ulLength=nBytesRead;		
	nTotalBytes+=nBytesRead;
	printf("\r %u", nTotalBytes);
	return false;
}

void CDataRecoveryDlg::DecryptFile(CString cstrFileName)
{
	PVOID pvContext;	
	HANDLE hRawFile;

	INIT_ADVAPI_ADDRESS(OpenEncryptedFileRawA);
	INIT_ADVAPI_ADDRESS(CloseEncryptedFileRaw);
	INIT_ADVAPI_ADDRESS(WriteEncryptedFileRaw);

	hRawFile=CreateFile(cstrFileName,GENERIC_READ,0,0,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);
	if(hRawFile==INVALID_HANDLE_VALUE)
	{
		ErrorMessage(GetLastError());
		return;
	}

	// cutting last ".tmp" string from file name to make original one
	cstrFileName.Delete(cstrFileName.ReverseFind('.'), 4);
	if(g_pOpenEncryptedFileRawA(cstrFileName,CREATE_FOR_IMPORT,&pvContext))
	{
		ErrorMessage(GetLastError());
		return;
	}	
	g_pWriteEncryptedFileRaw(&WriteCallback, &hRawFile, pvContext);
	g_pCloseEncryptedFileRaw(pvContext);
	CloseHandle(hRawFile);
}

void CDataRecoveryDlg::MakeSubDirNTFS(CString cstrDirName, CString cstrParentDirName, BOOL bIsCompressed, CListCtrl *pcList, CProgressCtrl *pProgDataRecovery)
{
	CString cstrParentDirTmp, cstrFileName, cstrTargetPath;
	CNTFSDrive::ST_FILEINFO stFInfo;
	DWORD nIdxSelLst; 
	LONGLONG iFileSize;
	HANDLE hNewFile;
	int nRet;

	if(!CreateDirectory(cstrDirName, NULL))
	{
		ErrorMessage(GetLastError());
		return;
	}

	if(bIsCompressed)
	{
		hNewFile = CreateFile(cstrDirName,GENERIC_WRITE|GENERIC_READ,0,NULL,OPEN_ALWAYS,FILE_FLAG_BACKUP_SEMANTICS,0); //FILE_ATTRIBUTE_NORMAL

		DWORD	cb;
		USHORT  usInBuff = COMPRESSION_FORMAT_DEFAULT;			
		nRet = DeviceIoControl(hNewFile, FSCTL_SET_COMPRESSION,
			&usInBuff, sizeof(USHORT),
			0, 0, &cb, 0);
		if(!nRet)
			return;

		CloseHandle(hNewFile);
	}

	m_btnScan.EnableWindow(FALSE);
	m_btnRecovery.EnableWindow(FALSE);
	m_btnDelete.EnableWindow(FALSE);

	int iItemCnt = pcList->GetItemCount();
	for(int i = 0; i < iItemCnt; i++) // recover all files in the folder
	{
		cstrParentDirTmp = pcList->GetItemText(i, 1);
		nIdxSelLst = _ttoi(pcList->GetItemText(i, 6));

		if(cstrParentDirTmp == cstrParentDirName)
		{
			cstrFileName = pcList->GetItemText(i, 0);
			cstrTargetPath = cstrDirName + "\\" + cstrFileName;
			iFileSize = _atoi64(pcList->GetItemText(i, 3));

			nRet = m_cNTFS.GetFileDetail(nIdxSelLst+16,stFInfo); // not get the file detail for FileName
			if(nRet)
			{
				ErrorMessage(nRet);
				return;
			}

			if(stFInfo.dwAttributesD & 0x10000000) // Sub Directory!!
			{
				CString cstrParentDir = pcList->GetItemText(i, 1);
				if(cstrParentDir.ReverseFind('\\') == 2) // cut last \ in "a:\"
					cstrParentDir.TrimRight('\\');
				cstrParentDir = cstrParentDir + "\\" + cstrFileName;

				if(stFInfo.dwAttributes & FILE_ATTRIBUTE_COMPRESSED)
					bIsCompressed = TRUE;
				else
					bIsCompressed = FALSE;

				MakeSubDirNTFS(cstrTargetPath, cstrParentDir, bIsCompressed, pcList, pProgDataRecovery);
				continue;
			}

			//create temporary raw file
			if(stFInfo.dwAttributes&FILE_ATTRIBUTE_ENCRYPTED)
			cstrTargetPath += ".tmp";

			/// create the new file and save the contents
			cstrTargetPath.Remove('?');
			HANDLE hNewFile = CreateFile(cstrTargetPath,GENERIC_WRITE|GENERIC_READ,0,NULL,CREATE_ALWAYS,stFInfo.dwAttributes,0); //FILE_ATTRIBUTE_NORMAL
			if(hNewFile == INVALID_HANDLE_VALUE)
			{
				ErrorMessage(GetLastError());
				return;
			}

			pProgDataRecovery->SetRange32(0, iFileSize > INT_MAX ? INT_MAX : iFileSize);
			nRet = m_cNTFS.Read_File_Write(nIdxSelLst+16, hNewFile, iFileSize, pProgDataRecovery); // read the file content and write to new file
			if(nRet)                     
			{
				ErrorMessage(nRet);
				CloseHandle(hNewFile);
				return;
			}
			CloseHandle(hNewFile);

			if(stFInfo.dwAttributes&FILE_ATTRIBUTE_ENCRYPTED)
			{
				DecryptFile(cstrTargetPath);
				//delete temporary raw file
				DeleteFile(cstrTargetPath);
				cstrTargetPath.Delete(cstrTargetPath.ReverseFind('.'), 4);
			}

			// 更新日時を削除前のそれに戻す
			hNewFile = CreateFile(cstrTargetPath, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
			SetFileTime(hNewFile, NULL, NULL, (const FILETIME *)&stFInfo.n64Modfil);
			CloseHandle(hNewFile);

			pProgDataRecovery->SetPos(0);
		}
	}
	m_btnScan.EnableWindow(TRUE);
	m_btnRecovery.EnableWindow(TRUE);
	m_btnDelete.EnableWindow(TRUE);
}

UINT RecoverNtfsThread(LPVOID lpVoid)
{
	CDataRecoveryDlg *pDlg = (CDataRecoveryDlg *)lpVoid;
	
	if(pDlg == NULL)
		return 1; 

	CListCtrl *pcList = (CListCtrl*)pDlg->GetDlgItem(IDC_LSTFILES);
	CProgressCtrl *pProgDataRecovery = (CProgressCtrl *)pDlg->GetDlgItem(IDC_PROGDATARECOVERY);

	TCHAR szPath[_MAX_PATH];
	DWORD nIdxSelLst, iIndex;
	LONGLONG iFileSize;
	CString cstrFileName, cstrParenDir;
	int nRet;
	CNTFSDrive::ST_FILEINFO stFInfo;
	BOOL bIsCompressed;

	POSITION pos = pcList->GetFirstSelectedItemPosition();
	if(!pos)
	{
		CString	cstrMessage;
		cstrMessage.LoadString(IDS_MSG_SELECT_FILE);
		AfxMessageBox(cstrMessage, MB_OK|MB_ICONEXCLAMATION);
		return 1;
	}

	CString cstrDriveLetterU = pDlg->m_cstrDriveLetter.Left(1);
	CString cstrDriveLetterL = pDlg->m_cstrDriveLetter.Left(1);
	cstrDriveLetterU.MakeUpper();
	cstrDriveLetterL.MakeLower();

changeDrive:
	if(!GetDirectory(pDlg->m_hWnd, szPath, NULL))
		return 1;

	CString cstrTargetPath = szPath;
	if(cstrTargetPath.Left(1) == cstrDriveLetterU || cstrTargetPath.Left(1) == cstrDriveLetterL)
	{
		nRet = AfxMessageBox(IDS_CHANGEFOLDERMSG, MB_OKCANCEL|MB_ICONEXCLAMATION);
		if(nRet == IDCANCEL)
			goto changeDrive;
	}

	while(pos)
	{
		cstrTargetPath = szPath;
		iIndex = pcList->GetNextSelectedItem(pos);
		iFileSize = _atoi64(pcList->GetItemText(iIndex, 3));
		nIdxSelLst = _ttoi(pcList->GetItemText(iIndex, 6));

		////////// フォルダ構造も復元
		//CString cstrFileName = pcList->GetItemText(iIndex, 0);
		//CString cstrParentDir1 = pcList->GetItemText(iIndex, 1);
		//cstrParentDir1.Delete(0, 2);
		//if(cstrParentDir1 == "\\<不明>") 
		//	cstrTargetPath = cstrTargetPath + "\\" + cstrFileName; 
		//else
		//{
		//	cstrTargetPath = cstrTargetPath = cstrTargetPath + cstrParentDir1;
		//	if(!CreateDirectory(cstrTargetPath, NULL))
		//	{
		//		pDlg->ErrorMessage(GetLastError());
		//		return 1;
		//	}
		//	cstrTargetPath = cstrTargetPath + "\\" + cstrFileName; 
		//}

		CString cstrFileName = pcList->GetItemText(iIndex, 0);
		cstrTargetPath = cstrTargetPath + "\\" + cstrFileName; 

		nRet = pDlg->m_cNTFS.GetFileDetail(nIdxSelLst+16,stFInfo); // not get the file detail for FileName
		if(nRet)
		{
			pDlg->ErrorMessage(nRet);
			return 1;
		}

		if(stFInfo.dwAttributesD & 0x10000000) // folder!! 
		{
			CString cstrParentDir = pcList->GetItemText(iIndex, 1);
			if(cstrParentDir.ReverseFind('\\') == 2) // cut last \ in "a:\"
				cstrParentDir.TrimRight('\\');
			cstrParentDir = cstrParentDir + "\\" + cstrFileName;
			
			if(stFInfo.dwAttributes & FILE_ATTRIBUTE_COMPRESSED)
				bIsCompressed = TRUE;
			else
				bIsCompressed = FALSE;

			pDlg->MakeSubDirNTFS(cstrTargetPath, cstrParentDir, bIsCompressed, pcList, pProgDataRecovery);
			continue;
		}

		//create temporary raw file
		if(stFInfo.dwAttributes&FILE_ATTRIBUTE_ENCRYPTED)
			cstrTargetPath += ".tmp";

		/// create the new file and save the contents
		cstrTargetPath.Remove('?');

		HANDLE hNewFile = CreateFile(cstrTargetPath,GENERIC_WRITE|GENERIC_READ,0,NULL,CREATE_ALWAYS,stFInfo.dwAttributes,0); //GENERIC_WRITE FILE_SHARE_READ FILE_ATTRIBUTE_NORMAL 
		if(hNewFile == INVALID_HANDLE_VALUE)
		{
			pDlg->ErrorMessage(GetLastError());
			cstrTargetPath += " CreateFile Failure!!";
			AfxMessageBox(cstrTargetPath, MB_OK|MB_ICONEXCLAMATION);
			return 1;
		}

		// if file already exists, ask user if overwrite or not
		if(GetLastError() == ERROR_ALREADY_EXISTS)
		{
			CString	cstrMessage;
			cstrMessage.LoadString(IDS_MSG_FILE_ALREADY_EXIST);
			cstrMessage.Append(PathFindFileName(cstrTargetPath));
			if(AfxMessageBox(cstrMessage, MB_OKCANCEL|MB_ICONEXCLAMATION) == IDCANCEL)
			{
				CloseHandle(hNewFile);
				return 1;
			}
		}

		pDlg->m_btnScan.EnableWindow(FALSE);
		pDlg->m_btnRecovery.EnableWindow(FALSE);
		pDlg->m_btnDelete.EnableWindow(FALSE);
		pProgDataRecovery->SetRange32(0, iFileSize > INT_MAX ? INT_MAX : iFileSize);

		nRet = pDlg->m_cNTFS.Read_File_Write(nIdxSelLst+16, hNewFile, iFileSize, pProgDataRecovery); // read the file content and write to new file
		if(nRet)                     
		{
			pDlg->ErrorMessage(nRet);
			AfxMessageBox(cstrFileName + " failed!!", MB_OK|MB_ICONEXCLAMATION);
			CloseHandle(hNewFile);
			continue;
//			return 1;
		}

		CloseHandle(hNewFile);

		if(stFInfo.dwAttributes&FILE_ATTRIBUTE_ENCRYPTED)
		{
			pDlg->DecryptFile(cstrTargetPath);
			//delete temporary raw file
			DeleteFile(cstrTargetPath);
			cstrTargetPath.Delete(cstrTargetPath.ReverseFind('.'), 4);
		}

		// 更新日時を削除前のそれに戻す
		hNewFile = CreateFile(cstrTargetPath, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        SetFileTime(hNewFile, NULL, NULL, (const FILETIME *)&stFInfo.n64Modfil);
        CloseHandle(hNewFile);

		pProgDataRecovery->SetPos(0);
	}

	pDlg->m_btnScan.EnableWindow(TRUE);
	pDlg->m_btnRecovery.EnableWindow(TRUE);
	pDlg->m_btnDelete.EnableWindow(TRUE);

	AfxMessageBox(IDS_MSG_RECOVERY_DONE, MB_ICONINFORMATION);
	ShellExecute(HWND_DESKTOP, "explore", szPath, NULL, NULL, SW_SHOWNORMAL);
	return 0;
}

void CDataRecoveryDlg::MakeSubDirFAT(CString cstrDirName, CString cstrParentDirName, CListCtrl *pcList, CProgressCtrl *pProgDataRecovery)
{
	CString cstrParentDirTmp, cstrFileName, cstrTargetPath;
	int iFileSize;
	DWORD dwStartCluster;
	int nRet;

	if(!CreateDirectory(cstrDirName, NULL))
	{
		ErrorMessage(GetLastError());
		return;
	}

	m_btnScan.EnableWindow(FALSE);
	m_btnRecovery.EnableWindow(FALSE);
	m_btnDelete.EnableWindow(FALSE);

	int iItemCnt = pcList->GetItemCount();
	for(int i = 0; i < iItemCnt; i++) // recover all files in the folder
	{
		cstrParentDirTmp = pcList->GetItemText(i, 1);

		if(cstrParentDirTmp == cstrParentDirName)
		{
			cstrFileName = pcList->GetItemText(i, 0);
			iFileSize = atoi(pcList->GetItemText(i, 3));
			dwStartCluster = _ttoi(pcList->GetItemText(i,6));
			cstrTargetPath = cstrDirName + "\\" + cstrFileName;

			if(iFileSize == 0) // Sub Directory!!
			{
				CString cstrParentDir = pcList->GetItemText(i, 1);
				if(cstrParentDir.ReverseFind('\\') == 2) // cut last \ in "a:\"
					cstrParentDir.TrimRight('\\');
				cstrParentDir = cstrParentDir + "\\" + cstrFileName;

				MakeSubDirFAT(cstrTargetPath, cstrParentDir, pcList, pProgDataRecovery);
				continue;
			}

			/// create the new file and save the contents
			cstrTargetPath.Remove('?');
			HANDLE hNewFile = CreateFile(cstrTargetPath,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,0);
			if(hNewFile == INVALID_HANDLE_VALUE)
			{
				ErrorMessage(GetLastError());
				return;
			}

			pProgDataRecovery->SetRange32(0, iFileSize);
			nRet = m_cFAT.RecoveryFile(hNewFile, dwStartCluster, iFileSize, pProgDataRecovery); // read the file content and write to new file
			if(nRet)                     
			{
				ErrorMessage(nRet);
				CloseHandle(hNewFile);
				return;
			}
			CloseHandle(hNewFile);

			// 更新日時を削除前のそれに戻す
			FILETIME ft, uft;
			SYSTEMTIME st;
			CString cstrModifiedTime = pcList->GetItemText(i, 4);
			st.wYear = atoi(cstrModifiedTime.Mid(0, 4));	//年を指定します。
			st.wMonth = atoi(cstrModifiedTime.Mid(5, 2));		//月を指定します。(1月 = 1､ ...)
			//st.wDayOfWeek = 4;	//曜日を指定します。(日曜日 = 0､ 月曜日 = 1､ ...)
			st.wDay = atoi(cstrModifiedTime.Mid(8, 2));		//日を指定します。
			st.wHour = atoi(cstrModifiedTime.Mid(11, 2));		//時を指定します。
			st.wMinute = atoi(cstrModifiedTime.Mid(14, 2));		//分を指定します。
			st.wSecond = 0;		//秒を指定します。
			st.wMilliseconds = 0;	//ミリ秒を指定します。
			SystemTimeToFileTime(&st, &ft);
			LocalFileTimeToFileTime(&ft, &uft);
			hNewFile = CreateFile(cstrTargetPath, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
			SetFileTime(hNewFile, NULL, NULL, &uft);
			CloseHandle(hNewFile);

			pProgDataRecovery->SetPos(0);
		}
	}
	m_btnScan.EnableWindow(TRUE);
	m_btnRecovery.EnableWindow(TRUE);
	m_btnDelete.EnableWindow(TRUE);
}

UINT RecoverFatThread(LPVOID lpVoid)
{
	CDataRecoveryDlg *pDlg = (CDataRecoveryDlg *)lpVoid;
	
	if(pDlg == NULL)
		return 1; 

	CListCtrl *pcList = (CListCtrl*)pDlg->GetDlgItem(IDC_LSTFILES);
	CProgressCtrl *pProgDataRecovery = (CProgressCtrl *)pDlg->GetDlgItem(IDC_PROGDATARECOVERY);

	TCHAR szPath[_MAX_PATH];
	DWORD dwStartCluster;
	//int nRet, iIndex, iFileSize;
	DWORD nRet, iIndex;
	LONGLONG iFileSize;

	POSITION pos = pcList->GetFirstSelectedItemPosition();
	if(!pos)
	{
		CString	cstrMessage;
		cstrMessage.LoadString(IDS_MSG_SELECT_FILE);
		AfxMessageBox(cstrMessage, MB_OK|MB_ICONEXCLAMATION);
		return 1;
	}

	CString cstrDriveLetterU = pDlg->m_cstrDriveLetter.Left(1);
	CString cstrDriveLetterL = pDlg->m_cstrDriveLetter.Left(1);
	cstrDriveLetterU.MakeUpper();
	cstrDriveLetterL.MakeLower();

changeDrive:
	if(!GetDirectory(pDlg->m_hWnd, szPath, NULL))
		return 1;

	CString cstrTargetPath = szPath;

	if(cstrTargetPath.Left(1) == cstrDriveLetterU || cstrTargetPath.Left(1) == cstrDriveLetterL)
	{
		nRet = AfxMessageBox(IDS_CHANGEFOLDERMSG, MB_OKCANCEL|MB_ICONEXCLAMATION);
		if(nRet == IDCANCEL)
			goto changeDrive;
	}
		
	while(pos) // loop all selected items in list
	{
		cstrTargetPath = szPath;
		iIndex = pcList->GetNextSelectedItem(pos);
		CString cstrFileName = pcList->GetItemText(iIndex, 0);
		iFileSize = _atoi64(pcList->GetItemText(iIndex, 3));
		dwStartCluster = _ttoi(pcList->GetItemText(iIndex,6));

		////////// フォルダ構造も復元
		//CString cstrParentDir1 = pcList->GetItemText(iIndex, 1);
		//cstrParentDir1.Delete(0, 2);
		//if(cstrParentDir1 == "\\<不明>") 
		//	cstrTargetPath = cstrTargetPath + "\\" + cstrFileName; 
		//else
		//{
		//	cstrTargetPath = cstrTargetPath = cstrTargetPath + cstrParentDir1;
		//	if(!CreateDirectory(cstrTargetPath, NULL))
		//	{
		//		pDlg->ErrorMessage(GetLastError());
		//		return 1;
		//	}
		//	cstrTargetPath = cstrTargetPath + "\\" + cstrFileName; 
		//}

		cstrTargetPath = cstrTargetPath + "\\" + cstrFileName; 

		if(iFileSize == 0) // folder!! 
		{
			CString cstrParentDir = pcList->GetItemText(iIndex, 1);
			if(cstrParentDir.ReverseFind('\\') == 2) // cut last \ in "a:\"
				cstrParentDir.TrimRight('\\');
			cstrParentDir = cstrParentDir + "\\" + cstrFileName;

			pDlg->MakeSubDirFAT(cstrTargetPath, cstrParentDir, pcList, pProgDataRecovery);
			continue;
		}

		/// create the new file and save the contents
		cstrTargetPath.Remove('?');
		HANDLE hNewFile = CreateFile(cstrTargetPath,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,0);
		if(hNewFile == INVALID_HANDLE_VALUE)
		{
			pDlg->ErrorMessage(GetLastError());
			return 1;
		}

		// if file already exists, ask user if overwrite or not
		if(GetLastError() == ERROR_ALREADY_EXISTS)
		{
			CString	cstrMessage;
			cstrMessage.LoadString(IDS_MSG_FILE_ALREADY_EXIST);
			cstrMessage.Append(PathFindFileName(cstrTargetPath));
			if(AfxMessageBox(cstrMessage, MB_OKCANCEL|MB_ICONEXCLAMATION) == IDCANCEL)
			{
				CloseHandle(hNewFile);
				return 1;
			}
		}

		pDlg->m_btnScan.EnableWindow(FALSE);
		pDlg->m_btnRecovery.EnableWindow(FALSE);
		pDlg->m_btnDelete.EnableWindow(FALSE);
		pProgDataRecovery->SetRange32(0, iFileSize > INT_MAX ? INT_MAX : iFileSize);

		nRet = pDlg->m_cFAT.RecoveryFile(hNewFile, dwStartCluster, iFileSize, pProgDataRecovery); // read the file content and write to new file
		if(nRet)                     
		{
			pDlg->ErrorMessage(nRet);
			return 1;
		}
		CloseHandle(hNewFile);

		// 更新日時を削除前のそれに戻す
		FILETIME ft, uft;
		SYSTEMTIME st;
		CString cstrModifiedTime = pcList->GetItemText(iIndex, 4);
		st.wYear = atoi(cstrModifiedTime.Mid(0, 4));	//年を指定します。
		st.wMonth = atoi(cstrModifiedTime.Mid(5, 2));		//月を指定します。(1月 = 1､ ...)
		//st.wDayOfWeek = 4;	//曜日を指定します。(日曜日 = 0､ 月曜日 = 1､ ...)
		st.wDay = atoi(cstrModifiedTime.Mid(8, 2));		//日を指定します。
		st.wHour = atoi(cstrModifiedTime.Mid(11, 2));		//時を指定します。
		st.wMinute = atoi(cstrModifiedTime.Mid(14, 2));		//分を指定します。
		st.wSecond = 0;		//秒を指定します。
		st.wMilliseconds = 0;	//ミリ秒を指定します。
		SystemTimeToFileTime(&st, &ft);
		LocalFileTimeToFileTime(&ft, &uft);
		hNewFile = CreateFile(cstrTargetPath, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        SetFileTime(hNewFile, NULL, NULL, &uft);
        CloseHandle(hNewFile);

		pProgDataRecovery->SetPos(0);
	}

	pDlg->m_btnScan.EnableWindow(TRUE);
	pDlg->m_btnRecovery.EnableWindow(TRUE);
	pDlg->m_btnDelete.EnableWindow(TRUE);
	AfxMessageBox(IDS_MSG_RECOVERY_DONE, MB_ICONINFORMATION);
	ShellExecute(HWND_DESKTOP, "explore", szPath, NULL, NULL, SW_SHOWNORMAL);
	return 0;
}

void CDataRecoveryDlg::OnBnClickedBtndelete()
{
	if(m_bIsFAT)
		AfxBeginThread(::DeleteFileFatThread, this);
	else
		DeleteFileNTFS();
	return;
}

void CDataRecoveryDlg::DeleteFileNTFS()
{
	CString	cstrMessage;
	CListCtrl *pcList = (CListCtrl*)GetDlgItem(IDC_LSTFILES);
	DWORD nRet,iIndex,nIdxSelLst, nClusterNo, nClusterOffset;

	POSITION pos = pcList->GetFirstSelectedItemPosition();
	if(!pos)
	{		
		cstrMessage.LoadString(IDS_MSG_SELECT_FILE_DELETE);
		AfxMessageBox(cstrMessage, MB_OK|MB_ICONEXCLAMATION);
		return;
	}

	if(g_bIsVista && m_bIsSystemDrive)
	{
		cstrMessage.LoadString(IDS_MSG_NOT_WIPE_VISTA_SYSTEM);
		AfxMessageBox(cstrMessage, MB_OK|MB_ICONEXCLAMATION);
		return;
	}

	cstrMessage.LoadString(IDS_MSG_CONFIRM_DELETE);
	if(AfxMessageBox(cstrMessage, MB_OKCANCEL|MB_ICONINFORMATION) == IDCANCEL)
		return;

	DWORD dwBytesReturned;
	if(g_bIsVista && !DeviceIoControl(
		m_cNTFS.m_hDrive,              // handle to device
		FSCTL_LOCK_VOLUME,  // dwIoControlCode
		NULL,                          // lpInBuffer
		0,                             // nInBufferSize
		NULL,                          // lpOutBuffer
		0,                             // nOutBufferSize
		&dwBytesReturned,     // number of bytes returned
		NULL   // OVERLAPPED structure
		))
	{
		if(GetLastError() == ERROR_NOT_READY)
		{
			CloseHandle(m_cNTFS.m_hDrive);
			m_hDrive = CreateFile(m_cstrDrive, GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, NULL);
			m_cNTFS.m_hDrive = m_hDrive;
			DeviceIoControl(
				m_cNTFS.m_hDrive,              // handle to device
				FSCTL_LOCK_VOLUME,  // dwIoControlCode
				NULL,                          // lpInBuffer
				0,                             // nInBufferSize
				NULL,                          // lpOutBuffer
				0,                             // nOutBufferSize
				&dwBytesReturned,     // number of bytes returned
				NULL   // OVERLAPPED structure
				);
		}
		if(GetLastError() == ERROR_ACCESS_DENIED)
		{
			cstrMessage.LoadString(IDS_MSG_CLOSE_ALL_FILE);
			AfxMessageBox(cstrMessage, MB_OK|MB_ICONEXCLAMATION);
			return;
		}
		else
		{
			ErrorMessage(GetLastError());
			return;
		}
	}

	CLoadMFTDlg* loadMFTDlg = new CLoadMFTDlg(2);
	loadMFTDlg->Create(IDD_LOADMFT); // show please-wait dialog

	iIndex = 0;
	while ((iIndex = pcList->GetNextItem(iIndex-1, LVNI_SELECTED)) != -1)
	{
		nIdxSelLst = _ttoi(pcList->GetItemText(iIndex, 6));
		nClusterNo = _ttoi(pcList->GetItemText(iIndex, 7));
		nClusterOffset = _ttoi(pcList->GetItemText(iIndex, 8));
		nRet = m_cNTFS.DeleteFile(nIdxSelLst+16, nClusterNo, nClusterOffset);
		if(nRet == TOO_MUCH_MFT)
		{
			cstrMessage.LoadString(IDS_ERRMSG_TOO_MUCH_MFT);
			AfxMessageBox(cstrMessage, MB_OK|MB_ICONEXCLAMATION);
			break;
		}
		else if(nRet)
		{
			ErrorMessage(nRet);
			break;
		}
		pcList->DeleteItem(iIndex);
	}

	loadMFTDlg->DestroyWindow();

	if(g_bIsVista && !DeviceIoControl(
		m_cNTFS.m_hDrive,              // handle to device
		FSCTL_UNLOCK_VOLUME,  // dwIoControlCode
		NULL,                          // lpInBuffer
		0,                             // nInBufferSize
		NULL,                          // lpOutBuffer
		0,                             // nOutBufferSize
		&dwBytesReturned,     // number of bytes returned
		NULL   // OVERLAPPED structure
		))
	{
		ErrorMessage(GetLastError());
		return;
	}

	return;
}

UINT DeleteFileFatThread(LPVOID lpVoid)
{
	CDataRecoveryDlg *pDlg = (CDataRecoveryDlg *)lpVoid;
	
	if(pDlg == NULL)
		return 1; 

	CListCtrl *pcList = (CListCtrl*)pDlg->GetDlgItem(IDC_LSTFILES);
	CProgressCtrl *pProgDataRecovery = (CProgressCtrl *)pDlg->GetDlgItem(IDC_PROGDATARECOVERY);

	CString	cstrMessage;
	DWORD dwStartCluster, dwClusterNo, dwClusterOffset;
	int nRet, iIndex, iFileSize;

	POSITION pos = pcList->GetFirstSelectedItemPosition();
	if(!pos)
	{		
		cstrMessage.LoadString(IDS_MSG_SELECT_FILE_DELETE);
		AfxMessageBox(cstrMessage, MB_OK|MB_ICONEXCLAMATION);
		return 1;
	}
	
	cstrMessage.LoadString(IDS_MSG_CONFIRM_DELETE);
	if(AfxMessageBox(cstrMessage, MB_OKCANCEL|MB_ICONINFORMATION) == IDCANCEL)
		return 1;

	DWORD dwBytesReturned;
	if(g_bIsVista && !DeviceIoControl(
		pDlg->m_cFAT.m_hDrive,              // handle to device
		FSCTL_LOCK_VOLUME,  // dwIoControlCode
		NULL,                          // lpInBuffer
		0,                             // nInBufferSize
		NULL,                          // lpOutBuffer
		0,                             // nOutBufferSize
		&dwBytesReturned,     // number of bytes returned
		NULL   // OVERLAPPED structure
		))
	{
		if(GetLastError() == ERROR_ACCESS_DENIED)
		{
			cstrMessage.LoadString(IDS_MSG_CLOSE_ALL_FILE);
			AfxMessageBox(cstrMessage, MB_OK|MB_ICONEXCLAMATION);
			return 1;
		}
		else
		{
			pDlg->ErrorMessage(GetLastError());
			return 1;
		}
	}

	pDlg->m_btnScan.EnableWindow(FALSE);
	pDlg->m_btnRecovery.EnableWindow(FALSE);
	pDlg->m_btnDelete.EnableWindow(FALSE);

	iIndex = 0;
	while ((iIndex = pcList->GetNextItem(iIndex-1, LVNI_SELECTED)) != -1)
	{
		iFileSize = _ttoi(pcList->GetItemText(iIndex, 3));
		dwStartCluster = _ttoi(pcList->GetItemText(iIndex,6));
		dwClusterNo = _ttoi(pcList->GetItemText(iIndex,7));
		dwClusterOffset = _ttoi(pcList->GetItemText(iIndex,8));

		pProgDataRecovery->SetRange32(0, iFileSize);

		nRet = pDlg->m_cFAT.DeleteFile(dwStartCluster, iFileSize, dwClusterNo, dwClusterOffset, pProgDataRecovery);
		if(nRet)                     
		{
			pDlg->ErrorMessage(nRet);
			break;
		}
		pcList->DeleteItem(iIndex);

		pProgDataRecovery->SetPos(0);
	}

	pDlg->m_btnScan.EnableWindow(TRUE);
	pDlg->m_btnRecovery.EnableWindow(TRUE);
	pDlg->m_btnDelete.EnableWindow(TRUE);

	if(g_bIsVista && !DeviceIoControl(
		pDlg->m_cFAT.m_hDrive,              // handle to device
		FSCTL_UNLOCK_VOLUME,  // dwIoControlCode
		NULL,                          // lpInBuffer
		0,                             // nInBufferSize
		NULL,                          // lpOutBuffer
		0,                             // nOutBufferSize
		&dwBytesReturned,     // number of bytes returned
		NULL   // OVERLAPPED structure
		))
	{
		pDlg->ErrorMessage(GetLastError());
		return 1;
	}

	return 0;
}

void CDataRecoveryDlg::OnSize(UINT nType, int cx, int cy)
{
	CDialog::OnSize(nType, cx, cy);

	CButton *pScanBtn = (CButton*)GetDlgItem(IDC_BTNSCAN);
	CButton *pRecoveryBtn = (CButton*)GetDlgItem(IDC_BTNDATARECOVERY);
	CButton *pDeleteBtn = (CButton*)GetDlgItem(IDC_BTNDELETE);
	CListCtrl *pList = (CListCtrl*)GetDlgItem(IDC_LSTFILES);
	CTreeCtrl *pTree = (CTreeCtrl *)GetDlgItem(IDC_TREDISKS);
	CEdit *pEditFileName = (CEdit *)GetDlgItem(IDC_EDITFILENAME);
	CEdit *pEditDelFiles = (CEdit *)GetDlgItem(IDC_EDTDELFILES);
	CEdit *pEditTotFiles = (CEdit *)GetDlgItem(IDC_EDTTOTFILES);
	CEdit *pEditTotClusters = (CEdit *)GetDlgItem(IDC_EDTTOTALCLUSTERS);
	CEdit *pEditSlash = (CEdit *)GetDlgItem(IDC_EDTSLASH);
	CStatic *pStaticDelFiles = (CStatic *)GetDlgItem(IDC_LBLDELFILES);
	CStatic *pStaticPartialFN = (CStatic *)GetDlgItem(IDC_LBL_PFN);
	CProgressCtrl *pProgSearch = (CProgressCtrl *)GetDlgItem(IDC_PROGSEARCH);
	CProgressCtrl *pProgRecovery = (CProgressCtrl *)GetDlgItem(IDC_PROGDATARECOVERY);

	static CRect rList;
	static CRect rProgSearch;
	static CRect rRecoveryBtn;
	static CRect rDeleteBtn;
	static CRect rProgRecovery;
	static CRect rStaticDelFiles;
	static CRect rEditDelFiles;
	static CRect rEditTotFiles;
	static CRect rEditTotClusters;
	static CRect rEditSlash;
	static CRect rTree;
	static CRect rStaticPartialFN;
	static CRect rEditFileName;
	static CRect rScanBtn;

	switch(nType)
	{
	case SIZE_MAXIMIZED:
		if(g_bFirstShow)
		{
			// save previous position and size
			pScanBtn->GetWindowRect(rScanBtn);
			ScreenToClient(rScanBtn);
			pRecoveryBtn->GetWindowRect(rRecoveryBtn);
			ScreenToClient(rRecoveryBtn);
			pDeleteBtn->GetWindowRect(rDeleteBtn);
			ScreenToClient(rDeleteBtn);
			pProgRecovery->GetWindowRect(rProgRecovery);
			ScreenToClient(rProgRecovery);
			pStaticDelFiles->GetWindowRect(rStaticDelFiles);
			ScreenToClient(rStaticDelFiles);
			pEditDelFiles->GetWindowRect(rEditDelFiles);
			ScreenToClient(rEditDelFiles);
			pEditTotFiles->GetWindowRect(rEditTotFiles);
			ScreenToClient(rEditTotFiles);
			pEditTotClusters->GetWindowRect(rEditTotClusters);
			ScreenToClient(rEditTotClusters);
			pEditSlash->GetWindowRect(rEditSlash);
			ScreenToClient(rEditSlash);
			pTree->GetWindowRect(rTree);
			ScreenToClient(rTree);
			pStaticPartialFN->GetWindowRect(rStaticPartialFN);
			ScreenToClient(rStaticPartialFN);
			pEditFileName->GetWindowRect(rEditFileName);
			ScreenToClient(rEditFileName);
			pList->GetWindowRect(rList);
			ScreenToClient(rList);
			pProgSearch->GetWindowRect(rProgSearch);
			ScreenToClient(rProgSearch);
			g_bFirstShow = false;
		}
		// set controls
		pList->MoveWindow(rScanBtn.right+8, rScanBtn.bottom+7,cx-rScanBtn.right-20,cy-rScanBtn.bottom-rRecoveryBtn.Height()-25);
		pTree->MoveWindow(rTree.left, rTree.top, rTree.Width(), rTree.Height()+50);
		pStaticPartialFN->MoveWindow(rStaticPartialFN.left, rTree.bottom+70, rStaticPartialFN.Width(), rStaticPartialFN.Height());
		pEditFileName->MoveWindow(rEditFileName.left, rTree.bottom+85, rEditFileName.Width(), rEditFileName.Height());
		pProgSearch->MoveWindow(rScanBtn.right+8, 21 ,cx-rScanBtn.right-20,rProgSearch.Height());

		pEditTotClusters->MoveWindow(cx-rEditTotClusters.Width()-10,21-rEditTotClusters.Height(),rEditTotClusters.Width(),rEditTotClusters.Height());
		pEditSlash->MoveWindow(cx-rEditTotClusters.Width()-10-rEditSlash.Width(),21-rEditSlash.Height(),rEditSlash.Width(),rEditSlash.Height());
		pEditTotFiles->MoveWindow(cx-rEditTotClusters.Width()-10-rEditSlash.Width()-rEditTotFiles.Width()-10,21-rEditTotFiles.Height(),rEditTotFiles.Width(),rEditTotFiles.Height());

		pDeleteBtn->MoveWindow(rDeleteBtn.left, cy-rRecoveryBtn.Height()-60, rDeleteBtn.Width(), rDeleteBtn.Height());
		pRecoveryBtn->MoveWindow(rRecoveryBtn.left, cy-rRecoveryBtn.Height()-10, rRecoveryBtn.Width(), rRecoveryBtn.Height());
		pProgRecovery->MoveWindow(rRecoveryBtn.right+8, cy-rRecoveryBtn.Height()-9, cx-rRecoveryBtn.right-rStaticDelFiles.Width()-rEditDelFiles.Width()-35, rProgRecovery.Height());
		pStaticDelFiles->MoveWindow(cx-rStaticDelFiles.Width()-rEditDelFiles.Width()-20, cy-rRecoveryBtn.Height()-6, rStaticDelFiles.Width(), rStaticDelFiles.Height());
		pEditDelFiles->MoveWindow(cx-rEditDelFiles.Width()-10, cy-rRecoveryBtn.Height()-9, rEditDelFiles.Width(), rEditDelFiles.Height());

		pList->SetColumnWidth(0,300);
		pList->SetColumnWidth(1,600);
		pList->SetColumnWidth(2,220);
		pList->SetColumnWidth(3,100);
		pList->SetColumnWidth(4,130);

		Invalidate();
		break;

	case SIZE_MINIMIZED:
	case SIZE_RESTORED:
		if(pList)
		{
			if(g_bFirstShow)
			{
				// save previous position and size
				pScanBtn->GetWindowRect(rScanBtn);
				ScreenToClient(rScanBtn);
				pRecoveryBtn->GetWindowRect(rRecoveryBtn);
				ScreenToClient(rRecoveryBtn);
				pDeleteBtn->GetWindowRect(rDeleteBtn);
				ScreenToClient(rDeleteBtn);
				pProgRecovery->GetWindowRect(rProgRecovery);
				ScreenToClient(rProgRecovery);
				pStaticDelFiles->GetWindowRect(rStaticDelFiles);
				ScreenToClient(rStaticDelFiles);
				pEditDelFiles->GetWindowRect(rEditDelFiles);
				ScreenToClient(rEditDelFiles);
				pEditTotFiles->GetWindowRect(rEditTotFiles);
				ScreenToClient(rEditTotFiles);
				pEditTotClusters->GetWindowRect(rEditTotClusters);
				ScreenToClient(rEditTotClusters);
				pEditSlash->GetWindowRect(rEditSlash);
				ScreenToClient(rEditSlash);
				pTree->GetWindowRect(rTree);
				ScreenToClient(rTree);
				pStaticPartialFN->GetWindowRect(rStaticPartialFN);
				ScreenToClient(rStaticPartialFN);
				pEditFileName->GetWindowRect(rEditFileName);
				ScreenToClient(rEditFileName);
				pList->GetWindowRect(rList);
				ScreenToClient(rList);
				pProgSearch->GetWindowRect(rProgSearch);
				ScreenToClient(rProgSearch);
				g_bFirstShow = false;
			}
			if(nType == SIZE_MINIMIZED)
				break;

			CRect ClientRect;
			GetClientRect(&ClientRect);
			
			int ControlHeight = rProgSearch.Height();
			rProgSearch.left = rScanBtn.right + 10;
			rProgSearch.right = ClientRect.right - 10;
			rProgSearch.bottom = rScanBtn.top + ControlHeight;
			rProgSearch.top = rScanBtn.top;

			int ControlWidth = rEditTotClusters.Width();
			ControlHeight = rEditTotClusters.Height();
			rEditTotClusters.left = ClientRect.right - 10 - ControlWidth;
			rEditTotClusters.right = ClientRect.right - 10;
			rEditTotClusters.bottom = ControlHeight;
			rEditTotClusters.top = 0;

			ControlWidth = rEditSlash.Width();
			ControlHeight = rEditSlash.Height();
			rEditSlash.left = rEditTotClusters.left - ControlWidth;
			rEditSlash.right = rEditTotClusters.left;
			rEditSlash.bottom = ControlHeight;
			rEditSlash.top = 0;

			ControlWidth = rEditTotFiles.Width();
			ControlHeight = rEditTotFiles.Height();
			rEditTotFiles.left = rEditSlash.left - 10 - ControlWidth;
			rEditTotFiles.right = rEditSlash.left - 10;
			rEditTotFiles.bottom = ControlHeight;
			rEditTotFiles.top = 0;
 
			ControlWidth = rRecoveryBtn.Width();
			ControlHeight = rRecoveryBtn.Height();
			rRecoveryBtn.left = ClientRect.left + 10;
			rRecoveryBtn.right = ClientRect.left + 10 + ControlWidth;
			rRecoveryBtn.bottom = ClientRect.bottom - 10;
			rRecoveryBtn.top = ClientRect.bottom - 10 - ControlHeight;

			ControlWidth = rEditDelFiles.Width();
			ControlHeight = rEditDelFiles.Height();
			rEditDelFiles.left = ClientRect.right - 10 - ControlWidth;
			rEditDelFiles.right = ClientRect.right - 10;
			rEditDelFiles.bottom = rRecoveryBtn.top + ControlHeight;
			rEditDelFiles.top = rRecoveryBtn.top;

			ControlWidth = rStaticDelFiles.Width();
			ControlHeight = rStaticDelFiles.Height();
			rStaticDelFiles.left = rEditDelFiles.left - 10 - ControlWidth;
			rStaticDelFiles.right = rEditDelFiles.left - 10;
			rStaticDelFiles.bottom = rRecoveryBtn.top + 3 + ControlHeight;
			rStaticDelFiles.top = rRecoveryBtn.top + 3;
			
			ControlHeight = rProgRecovery.Height();
			rProgRecovery.left = rRecoveryBtn.right + 10;
			rProgRecovery.right = rStaticDelFiles.left - 10;
			rProgRecovery.bottom = rRecoveryBtn.top + ControlHeight;
			rProgRecovery.top = rRecoveryBtn.top;

			ControlWidth = rTree.Width();
			rTree.left = rScanBtn.left;
			rTree.right = rScanBtn.left + ControlWidth;
			rTree.bottom = ClientRect.bottom / 2 ;
			rTree.top = rScanBtn.bottom + 10;

			ControlWidth = rStaticPartialFN.Width();
			ControlHeight = rStaticPartialFN.Height();
			rStaticPartialFN.left = rScanBtn.left;
			rStaticPartialFN.right = rScanBtn.left + ControlWidth;
			rStaticPartialFN.bottom = rTree.bottom + 15 + ControlHeight;
			rStaticPartialFN.top = rTree.bottom + 15;

			ControlWidth = rEditFileName.Width();
			ControlHeight = rEditFileName.Height();
			rEditFileName.left = rScanBtn.left;
			rEditFileName.right = rScanBtn.left + ControlWidth;
			rEditFileName.bottom = rStaticPartialFN.bottom + 2 + ControlHeight;
			rEditFileName.top = rStaticPartialFN.bottom + 2;

			ControlWidth = rDeleteBtn.Width();
			ControlHeight = rDeleteBtn.Height();
			rDeleteBtn.left = rScanBtn.right - ControlWidth;
			rDeleteBtn.right = rScanBtn.right;
			rDeleteBtn.bottom = rRecoveryBtn.top - 20;
			rDeleteBtn.top = rRecoveryBtn.top - 20 - ControlHeight;

			rList.left = rTree.right + 10;
			rList.right = ClientRect.right - 10;
			rList.bottom = rRecoveryBtn.top - 10;
			rList.top = rTree.top;

			// restore previous position and size
			pList->MoveWindow(rList);
			pProgSearch->MoveWindow(rProgSearch);
			pRecoveryBtn->MoveWindow(rRecoveryBtn);
			pDeleteBtn->MoveWindow(rDeleteBtn);
			pProgRecovery->MoveWindow(rProgRecovery);
			pStaticDelFiles->MoveWindow(rStaticDelFiles);
			pEditDelFiles->MoveWindow(rEditDelFiles);
			pEditTotClusters->MoveWindow(rEditTotClusters);
			pEditTotFiles->MoveWindow(rEditTotFiles);
			pEditSlash->MoveWindow(rEditSlash);
			pTree->MoveWindow(rTree);
			pStaticPartialFN->MoveWindow(rStaticPartialFN);
			pEditFileName->MoveWindow(rEditFileName);

			pList->SetColumnWidth(0,100);
			pList->SetColumnWidth(1,250);
			pList->SetColumnWidth(2,110);
			pList->SetColumnWidth(3,80);
			pList->SetColumnWidth(4,110);

			Invalidate();
		}
		break;
	}
}


void CAboutDlg::OnBnClickedOk()
{
	if(g_bIsNT)
	{
		CAutoUpdater updater;
		int result = updater.CheckForUpdate("http://tokiwa.qee.jp/download/DataRecovery/");
		switch(result)
		{
		case CAutoUpdater::Success :
			AfxMessageBox(IDS_MSG_UPDATE_SUCCESS, MB_ICONINFORMATION|MB_OK);
			break;
		case CAutoUpdater::InternetConnectFailure :
		case CAutoUpdater::InternetSessionFailure :
			AfxMessageBox(IDS_MSG_CONNECT_FAILURE, MB_ICONINFORMATION|MB_OK);
			break;
		case CAutoUpdater::ConfigDownloadFailure :
			AfxMessageBox(_T("ConfigDownloadFailure!"), MB_OK|MB_ICONERROR);
			break;
		case CAutoUpdater::NoExecutableVersion :
			AfxMessageBox(_T("NoExecutableVersion!"), MB_OK|MB_ICONERROR);
			break;
		case CAutoUpdater::UpdateNotRequired :
			AfxMessageBox(IDS_MSG_ALREADY_UPDATED, MB_ICONINFORMATION|MB_OK);
			break;
		case CAutoUpdater::UpdateCancelled :
			break;
		case CAutoUpdater::FileDownloadFailure :
			AfxMessageBox(_T("FileDownloadFailure!"), MB_OK|MB_ICONERROR);
			break;
		case CAutoUpdater::FileSwitchFailure :
			AfxMessageBox(_T("FileSwitchFailure!"), MB_OK|MB_ICONERROR);
			break;
		default :
			break;
		}
	}
}

void CDataRecoveryDlg::ErrorMessage(DWORD dwErrorNo)
{
	TCHAR *lpMsgBuf;
	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM,NULL,dwErrorNo, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),(LPTSTR) &lpMsgBuf, 0, NULL);
	AfxMessageBox((LPTSTR)lpMsgBuf, MB_OK|MB_ICONERROR);
	LocalFree(lpMsgBuf);
}

void CDataRecoveryDlg::ErrorMessage1(CString cstrWhere, DWORD dwErrorNo)
{
	TCHAR *lpMsgBuf;
	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM,NULL,dwErrorNo, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),(LPTSTR) &lpMsgBuf, 0, NULL);
	AfxMessageBox(cstrWhere + (LPTSTR)lpMsgBuf, MB_OK|MB_ICONERROR);
	LocalFree(lpMsgBuf);
}

void CDataRecoveryDlg::OnNMRclickLstfiles(NMHDR *pNMHDR, LRESULT *pResult)
{
	CListCtrl *pList = (CListCtrl*)GetDlgItem(IDC_LSTFILES);
	if(pList->GetSelectedCount() != 1) // 単一ファイル選択時のみコンテキストメニュー表示
		return;

	NM_LISTVIEW* pNMListView = (NM_LISTVIEW*)pNMHDR;
	CPoint pt(pNMListView->ptAction);
	CMenu menu;
	menu.LoadMenu(IDR_MENU_RENAME);
	CMenu* pContextMenu = menu.GetSubMenu(0);
	ASSERT(pContextMenu);

	POINT p;
	p.x = pt.x;
	p.y = pt.y;
	::ClientToScreen(pNMHDR->hwndFrom, &p);
	int nID = pContextMenu->TrackPopupMenu( TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_RETURNCMD, p.x, p.y, this);

	POSITION pos = pList->GetFirstSelectedItemPosition();
	while(pos)
	{
		pList->EditLabel(pList->GetNextSelectedItem(pos));
	}

	*pResult = 0;
}

void CDataRecoveryDlg::OnLvnEndlabeleditLstfiles(NMHDR *pNMHDR, LRESULT *pResult)
{
	NMLVDISPINFO *pDispInfo = reinterpret_cast<NMLVDISPINFO*>(pNMHDR);
	// TODO: ここにコントロール通知ハンドラ コードを追加します。
	*pResult = TRUE;
}
