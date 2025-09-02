#pragma once
#include "afxwin.h"


// YahooKiyakuDlg ダイアログ

class YahooKiyakuDlg : public CDialog
{
	DECLARE_DYNAMIC(YahooKiyakuDlg)

public:
	YahooKiyakuDlg(CWnd* pParent = NULL);   // 標準コンストラクタ
	virtual ~YahooKiyakuDlg();

// ダイアログ データ
	enum { IDD = IDD_YAHOO_KIYAKU };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV サポート

	DECLARE_MESSAGE_MAP()
public:
	virtual BOOL OnInitDialog();
	CEdit m_editKiyaku;
};
