// YahooKiyakuDlg.cpp : 実装ファイル
//

#include "stdafx.h"
#include "DataRecovery.h"
#include "YahooKiyakuDlg.h"


// YahooKiyakuDlg ダイアログ

IMPLEMENT_DYNAMIC(YahooKiyakuDlg, CDialog)

YahooKiyakuDlg::YahooKiyakuDlg(CWnd* pParent /*=NULL*/)
	: CDialog(YahooKiyakuDlg::IDD, pParent)
{

}

YahooKiyakuDlg::~YahooKiyakuDlg()
{
}

void YahooKiyakuDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_EDIT1, m_editKiyaku);
}


BEGIN_MESSAGE_MAP(YahooKiyakuDlg, CDialog)
END_MESSAGE_MAP()


// YahooKiyakuDlg メッセージ ハンドラ

BOOL YahooKiyakuDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// TODO:  ここに初期化を追加してください
	BOOL		ret;
	CString		strLine;
	CString		strMessage;
	CStdioFile	cFile;

	ret = cFile.Open(_T("Yahoo!.txt"), CFile::modeRead | CFile::shareDenyNone);

	while(ret)
	{
		ret = cFile.ReadString(strLine);
		strMessage += strLine + _T("\n\r");
		strMessage += _T("\n\r");
	}


	m_editKiyaku.SetWindowText(strMessage);

	return TRUE;  // return TRUE unless you set the focus to a control
	// 例外 : OCX プロパティ ページは必ず FALSE を返します。
}
