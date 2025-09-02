// DataRecovery.cpp : Defines the class behaviors for the application.
//

#include "stdafx.h"
#include "DataRecovery.h"
#include "DataRecoveryDlg.h"
#include "AutoUpdater.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CDataRecoveryApp

BEGIN_MESSAGE_MAP(CDataRecoveryApp, CWinApp)
	//{{AFX_MSG_MAP(CDataRecoveryApp)
		// NOTE - the ClassWizard will add and remove mapping macros here.
		//    DO NOT EDIT what you see in these blocks of generated code!
	//}}AFX_MSG
	ON_COMMAND(ID_HELP, CWinApp::OnHelp)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CDataRecoveryApp construction

CDataRecoveryApp::CDataRecoveryApp()
{
	// TODO: add construction code here,
	// Place all significant initialization in InitInstance
}

/////////////////////////////////////////////////////////////////////////////
// The one and only CDataRecoveryApp object

CDataRecoveryApp theApp;

#define EVENT_SINGLE _T("I LOVE TOKYO, DATARECOVERY")
/////////////////////////////////////////////////////////////////////////////
// CDataRecoveryApp initialization

BOOL CDataRecoveryApp::InitInstance()
{
	//To Run Only One instance
	m_hEvent = ::CreateEvent(NULL, TRUE, FALSE, EVENT_SINGLE);
	DWORD dwErr = GetLastError();
	if(dwErr == ERROR_ALREADY_EXISTS)
		return FALSE;

	AfxEnableControlContainer();

	// Standard initialization
	// If you are not using these features and wish to reduce the size
	//  of your final executable, you should remove from the following
	//  the specific initialization routines you do not need.

#ifdef _AFXDLL
	Enable3dControls();			// Call this when using MFC in a shared DLL
#else
//	Enable3dControlsStatic();	// Call this when linking to MFC statically
#endif
	CDataRecoveryDlg dlg;
	m_pMainWnd = &dlg;
	int nResponse = dlg.DoModal();
	if (nResponse == IDOK)
	{
		// TODO: Place code here to handle when the dialog is
		//  dismissed with OK
	}
	else if (nResponse == IDCANCEL)
	{
		// TODO: Place code here to handle when the dialog is
		//  dismissed with Cancel
	}

	// Since the dialog has been closed, return FALSE so that we exit the
	//  application, rather than start the application's message pump.
	return FALSE;
}

int CDataRecoveryApp::ExitInstance()
{
	if( m_pszProfileName ) 
	{
		delete ((void*)m_pszProfileName);
		m_pszProfileName = NULL;
	}

	return CWinApp::ExitInstance();
}
