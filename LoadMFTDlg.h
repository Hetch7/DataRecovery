#pragma once


// CLoadMFTDlg ダイアログ

class CLoadMFTDlg : public CDialog
{
	DECLARE_DYNAMIC(CLoadMFTDlg)

public:
	CLoadMFTDlg(int nMsgType, CWnd* pParent = NULL);   // 標準コンストラクタ
	virtual ~CLoadMFTDlg();

// ダイアログ データ
	enum { IDD = IDD_LOADMFT };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV サポート

	DECLARE_MESSAGE_MAP()
	virtual void OnCancel();
	virtual void OnOK();
	virtual void PostNcDestroy();
	int m_nMsgType;
public:
	virtual BOOL OnInitDialog();
};
