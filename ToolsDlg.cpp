// ToolsDlg.cpp : �����t�@�C��
//

#include "stdafx.h"
#include "DataRecovery.h"
#include "ToolsDlg.h"
#include "LinkStatic.h"
#include "AutoUpdater.h"

extern BOOL g_bJWordLiving;
extern BOOL g_bYahooLiving;

// ToolsDlg �_�C�A���O

IMPLEMENT_DYNAMIC(ToolsDlg, CDialog)

ToolsDlg::ToolsDlg(CWnd* pParent /*=NULL*/)
	: CDialog(ToolsDlg::IDD, pParent)
{

}

ToolsDlg::~ToolsDlg()
{
}

void ToolsDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_CHECK_YAHOO, m_GCheckYahoo);
	DDX_Control(pDX, IDC_STATIC_YAHOO, m_GroupYahoo);
	DDX_Control(pDX, IDC_KIYAKU_YAHOO, m_lsYahooKiyaku);
	DDX_Control(pDX, IDC_CHECK_JWORD, m_GCheckJword);
	DDX_Control(pDX, IDC_STATIC_JWORD, m_GroupJword);
	DDX_Control(pDX, IDC_JWORD_URL, m_lsJword);
	DDX_Control(pDX, IDC_NOMORE_TOOLS, m_chkNomore);
	DDX_Control(pDX, IDC_URL_YAHOO, m_lsYahooURL);
	DDX_Control(pDX, IDC_KIYAKU_JWORD, m_lsJwordKiyaku);
	DDX_Control(pDX, IDC_CHECK_KISU, m_GCheckKISU);
	DDX_Control(pDX, IDC_KISU_URL, m_lsKISU);
	DDX_Control(pDX, IDC_STATIC_KISU, m_GroupKISU);
}


BEGIN_MESSAGE_MAP(ToolsDlg, CDialog)
	ON_BN_CLICKED(IDOK, &ToolsDlg::OnBnClickedOk)
END_MESSAGE_MAP()


// ToolsDlg ���b�Z�[�W �n���h��

BOOL ToolsDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// TODO:  �����ɏ�������ǉ����Ă�������
	m_GCheckYahoo.SetGroupbox(&m_GroupYahoo);
	m_GCheckJword.SetGroupbox(&m_GroupJword);
	m_GCheckKISU.SetGroupbox(&m_GroupKISU);

	if(g_bJWordLiving)
		m_GCheckJword.EnableWindow(FALSE);
	if(g_bYahooLiving)
		m_GCheckYahoo.EnableWindow(FALSE);
	if(!g_bJWordLiving)
		m_GCheckJword.SetCheck(1);
	if(!g_bYahooLiving)
		m_GCheckYahoo.SetCheck(1);

	m_GCheckKISU.SetCheck(1);


	CAutoUpdater updater;
	int result = updater.CheckForFile("http://tokiwa.qee.jp/download/DataRecovery/", "Yahoo!.txt");
	switch(result)
	{
	case CAutoUpdater::InternetConnectFailure :
	case CAutoUpdater::InternetSessionFailure :
		AfxMessageBox(IDS_MSG_CONNECT_FAILURE, MB_ICONINFORMATION|MB_OK);
		break;
	case CAutoUpdater::FileDownloadFailure :
		AfxMessageBox(_T("FileDownloadFailure!"), MB_OK|MB_ICONERROR);
		break;
	default :
		break;
	}

	return TRUE;  // return TRUE unless you set the focus to a control
	// ��O : OCX �v���p�e�B �y�[�W�͕K�� FALSE ��Ԃ��܂��B
}

void ToolsDlg::OnBnClickedOk()
{
	// TODO: �����ɃR���g���[���ʒm�n���h�� �R�[�h��ǉ����܂��B
	if(m_chkNomore.GetCheck() == BST_CHECKED)
		AfxGetApp()->WriteProfileInt( "Setting", "nomoreTools", 1);
	else
		AfxGetApp()->WriteProfileInt( "Setting", "nomoreTools", 0);

	int nRet = 0;
	if(m_GCheckYahoo.GetCheck() == BST_CHECKED && m_GCheckJword.GetCheck() == BST_UNCHECKED && m_GCheckKISU.GetCheck() == BST_UNCHECKED)
		nRet = 1;
	if(m_GCheckYahoo.GetCheck() == BST_UNCHECKED && m_GCheckJword.GetCheck() == BST_CHECKED && m_GCheckKISU.GetCheck() == BST_UNCHECKED)
		nRet = 2;
	if(m_GCheckYahoo.GetCheck() == BST_UNCHECKED && m_GCheckJword.GetCheck() == BST_UNCHECKED && m_GCheckKISU.GetCheck() == BST_CHECKED)
		nRet = 3;
	if(m_GCheckYahoo.GetCheck() == BST_CHECKED && m_GCheckJword.GetCheck() == BST_CHECKED && m_GCheckKISU.GetCheck() == BST_UNCHECKED)
		nRet = 4;
	if(m_GCheckYahoo.GetCheck() == BST_CHECKED && m_GCheckJword.GetCheck() == BST_UNCHECKED && m_GCheckKISU.GetCheck() == BST_CHECKED)
		nRet = 5;
	if(m_GCheckYahoo.GetCheck() == BST_UNCHECKED && m_GCheckJword.GetCheck() == BST_CHECKED && m_GCheckKISU.GetCheck() == BST_CHECKED)
		nRet = 6;
	if(m_GCheckYahoo.GetCheck() == BST_CHECKED && m_GCheckJword.GetCheck() == BST_CHECKED && m_GCheckKISU.GetCheck() == BST_CHECKED)
		nRet = 7;

	EndDialog(nRet);
	//OnOK();
}

void ToolsDlg::OnCancel()
{
	// TODO: �����ɓ���ȃR�[�h��ǉ����邩�A�������͊�{�N���X���Ăяo���Ă��������B
	EndDialog(0);

//	CDialog::OnCancel();
}
