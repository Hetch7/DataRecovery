#pragma once

#include "GroupCheck.h"
#include "afxwin.h"
#include "linkstatic.h"
// ToolsDlg �_�C�A���O

class ToolsDlg : public CDialog
{
	DECLARE_DYNAMIC(ToolsDlg)

public:
	ToolsDlg(CWnd* pParent = NULL);   // �W���R���X�g���N�^
	virtual ~ToolsDlg();

// �_�C�A���O �f�[�^
	enum { IDD = IDD_TOOLS_INST };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV �T�|�[�g

	DECLARE_MESSAGE_MAP()
public:
	CGroupCheck m_GCheckYahoo;
	CButton m_GroupYahoo;
	virtual BOOL OnInitDialog();
	CLinkStatic m_lsYahooKiyaku;
	CGroupCheck m_GCheckJword;
	CButton m_GroupJword;
	CLinkStatic m_lsJword;
	CButton m_chkNomore;
	afx_msg void OnBnClickedOk();
	CLinkStatic m_lsYahooURL;
	CLinkStatic m_lsJwordKiyaku;
protected:
	virtual void OnCancel();
public:
	CGroupCheck m_GCheckKISU;
	CLinkStatic m_lsKISU;
	CButton m_GroupKISU;
};
