#pragma once


// CLoadMFTDlg �_�C�A���O

class CLoadMFTDlg : public CDialog
{
	DECLARE_DYNAMIC(CLoadMFTDlg)

public:
	CLoadMFTDlg(int nMsgType, CWnd* pParent = NULL);   // �W���R���X�g���N�^
	virtual ~CLoadMFTDlg();

// �_�C�A���O �f�[�^
	enum { IDD = IDD_LOADMFT };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV �T�|�[�g

	DECLARE_MESSAGE_MAP()
	virtual void OnCancel();
	virtual void OnOK();
	virtual void PostNcDestroy();
	int m_nMsgType;
public:
	virtual BOOL OnInitDialog();
};
