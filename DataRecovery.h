// DataRecovery.h : main header file for the DataRecovery application
//

#if !defined(AFX_DataRecovery_H__36D89CCD_73B6_4281_84F3_8BFB6C26776D__INCLUDED_)
#define AFX_DataRecovery_H__36D89CCD_73B6_4281_84F3_8BFB6C26776D__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#ifndef __AFXWIN_H__
	#error include 'stdafx.h' before including this file for PCH
#endif

#include "resource.h"		// main symbols
/////////////////////////////////////////////////////////////////////////////
// CDataRecoveryApp:
// See DataRecovery.cpp for the implementation of this class
//

class CDataRecoveryApp : public CWinApp
{
public:
	CDataRecoveryApp();
	HANDLE m_hEvent;

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CDataRecoveryApp)
	public:
	virtual BOOL InitInstance();
	//}}AFX_VIRTUAL

// Implementation

	//{{AFX_MSG(CDataRecoveryApp)
		// NOTE - the ClassWizard will add and remove member functions here.
		//    DO NOT EDIT what you see in these blocks of generated code !
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
	virtual int ExitInstance();
};


/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_DataRecovery_H__36D89CCD_73B6_4281_84F3_8BFB6C26776D__INCLUDED_)
