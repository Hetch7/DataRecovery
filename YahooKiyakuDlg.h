#pragma once
#include "afxwin.h"


// YahooKiyakuDlg �_�C�A���O

class YahooKiyakuDlg : public CDialog
{
	DECLARE_DYNAMIC(YahooKiyakuDlg)

public:
	YahooKiyakuDlg(CWnd* pParent = NULL);   // �W���R���X�g���N�^
	virtual ~YahooKiyakuDlg();

// �_�C�A���O �f�[�^
	enum { IDD = IDD_YAHOO_KIYAKU };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV �T�|�[�g

	DECLARE_MESSAGE_MAP()
public:
	virtual BOOL OnInitDialog();
	CEdit m_editKiyaku;
};
