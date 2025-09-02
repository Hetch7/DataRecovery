#pragma once
#include "afxwin.h"
#include "Resource.h"

// CLinkStatic

class CLinkStatic : public CStatic
{
	DECLARE_DYNAMIC(CLinkStatic)

public:
	CLinkStatic();
	virtual ~CLinkStatic();

protected:
	DECLARE_MESSAGE_MAP()
private:
	CFont m_font;
	CBrush m_brush;
	HICON m_hCursor;
public:
	afx_msg BOOL OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg HBRUSH CtlColor(CDC* /*pDC*/, UINT /*nCtlColor*/);
};


