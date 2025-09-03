// LinkStatic.cpp : 実装ファイル
//

#include "stdafx.h"
#include "LinkStatic.h"

// CLinkStatic

IMPLEMENT_DYNAMIC(CLinkStatic, CStatic)
CLinkStatic::CLinkStatic()
{
	m_hCursor = CopyCursor(::LoadCursor(NULL, IDC_HAND));
    m_brush.CreateSolidBrush(::GetSysColor(COLOR_3DFACE));
}

CLinkStatic::~CLinkStatic()
{
}


BEGIN_MESSAGE_MAP(CLinkStatic, CStatic)
	ON_WM_SETCURSOR()
	ON_WM_LBUTTONDOWN()
ON_WM_CTLCOLOR_REFLECT()
END_MESSAGE_MAP()

BOOL CLinkStatic::OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message)
{
	::SetCursor(m_hCursor);
    return TRUE;
}

void CLinkStatic::OnLButtonDown(UINT nFlags, CPoint point)
{
	CString cstrJump;
	TCHAR strJump[255];

	GetWindowText(cstrJump);
	DWORD dwVersion = GetVersion();

	if(cstrJump == "おまけ")
		cstrJump = "https://tokiwa.qc-plus.jp/higawari/index.html";

	// Check Windows Version 
	if (dwVersion < 0x80000000)  // Windows NT/2000/XP
		::ShellExecute(NULL, _T("open"), cstrJump, NULL, NULL, SW_SHOWNORMAL); 
	else
	{		
		_stprintf_s(strJump, 255, " %s", cstrJump); // need first white space before URL
		CString strCommand = "C:\\Program Files\\Internet Explorer\\iexplore.exe";

		PROCESS_INFORMATION pi;
		STARTUPINFO si;
		ZeroMemory(&si,sizeof(si));
		si.cb=sizeof(si);

		CreateProcess(strCommand, strJump, NULL,NULL,FALSE,NORMAL_PRIORITY_CLASS,
			NULL,NULL,&si,&pi);

		CloseHandle(pi.hThread);
	}
	CStatic::OnLButtonDown(nFlags, point);
}

HBRUSH CLinkStatic::CtlColor(CDC* pDC, UINT nCtlColor)
{
	// 最初に呼ばれた時だけフォントの設定をする
    if(!m_font.m_hObject)
    {
        // フォントの設定（下線を引く）
        LOGFONT lf;
        GetFont()->GetLogFont(&lf);
        lf.lfUnderline = TRUE;
        m_font.CreateFontIndirect(&lf);
        SetFont(&m_font, FALSE);
        Invalidate();
    }

    // 色を設定
    pDC->SetTextColor(RGB(0, 0, 255));
    pDC->SetBkColor(::GetSysColor(COLOR_3DFACE));
    return m_brush; 
}
