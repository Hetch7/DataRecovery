// LoadMFTDlg.cpp : 実装ファイル
//

#include "stdafx.h"
#include "DataRecovery.h"
#include "LoadMFTDlg.h"


// CLoadMFTDlg ダイアログ

IMPLEMENT_DYNAMIC(CLoadMFTDlg, CDialog)
CLoadMFTDlg::CLoadMFTDlg(int nMsgType, CWnd* pParent /*=NULL*/)
	: CDialog(CLoadMFTDlg::IDD, pParent)
{
	m_nMsgType = nMsgType;
}

CLoadMFTDlg::~CLoadMFTDlg()
{
}

void CLoadMFTDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}


BEGIN_MESSAGE_MAP(CLoadMFTDlg, CDialog)
END_MESSAGE_MAP()


// CLoadMFTDlg メッセージ ハンドラ

void CLoadMFTDlg::OnCancel()
{
	//CDialog::OnCancel();
}

void CLoadMFTDlg::OnOK()
{
	//CDialog::OnOK();
}

void CLoadMFTDlg::PostNcDestroy()
{
	delete this;
}

BOOL CLoadMFTDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	CString cstrTitle;
	cstrTitle.LoadString(IDS_WAITBOX_TITLE);
	SetWindowText(cstrTitle);

	CStatic *pStaticMsg = (CStatic *)GetDlgItem(IDC_WAITMSG);

	if(m_nMsgType == 1)
		cstrTitle.LoadString(IDS_WAITBOX_MSG1);
	if(m_nMsgType == 2)
		cstrTitle.LoadString(IDS_WAITBOX_MSG2);
	if(m_nMsgType == 3)
		cstrTitle.LoadString(IDS_WAITBOX_MSG3);

	pStaticMsg->SetWindowText(cstrTitle);

	return TRUE;  // return TRUE unless you set the focus to a control
	// 例外 : OCX プロパティ ページは必ず FALSE を返します。
}
