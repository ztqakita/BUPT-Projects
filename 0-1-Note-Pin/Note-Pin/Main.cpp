 /*----------------------------------------
   Note Pin on your screen.
				(c) 2015 LJN
  ----------------------------------------*/

//Improve:	RegisterHotKey to Use global Hot Key
//Use Rich Edit Control to improve the Text Box

#include <windows.h>
#include "notepin.h"
#include "resource.h"

LRESULT	CALLBACK WndProc (HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK TempProc (HWND, UINT, WPARAM, LPARAM);

WNDPROC OldProc;
BOOL	fUAC;

int WINAPI WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance,
					PSTR szCmdLine, int iCmdShow)
{
	static TCHAR szAppName[] = TEXT ("Note-Pin");
	HWND		hwnd;
	MSG			msg;
	WNDCLASS	wndclass;
	int			iWidth, iHeight;
	HACCEL		hAccel;

	fUAC = *szCmdLine == '\0' ? FALSE : TRUE;
	hwnd = FindWindow (szAppName, szAppName);
	if (IsWindow (hwnd) && !fUAC)
	{
		SendMessage (hwnd, WM_DISPLAY, 0, 0);
		return 0;
	}

	wndclass.style			= CS_HREDRAW | CS_VREDRAW;
	wndclass.lpfnWndProc	= WndProc;
	wndclass.cbClsExtra		= 0;
	wndclass.cbWndExtra		= 0;
	wndclass.hInstance		= hInstance;
	wndclass.hIcon			= LoadIcon (hInstance, MAKEINTRESOURCE (IDI_ICON));
	wndclass.hCursor		= LoadCursor (NULL, IDC_ARROW);
	wndclass.hbrBackground	= (HBRUSH) (COLOR_ACTIVEBORDER + 1);
	wndclass.lpszMenuName	= NULL;
	wndclass.lpszClassName	= szAppName;
	RegisterClass (&wndclass);

	iWidth = GetSystemMetrics (SM_CXSCREEN);
	iHeight = GetSystemMetrics (SM_CYSCREEN);

	hwnd = CreateWindowEx (WS_EX_TOOLWINDOW | WS_EX_TOPMOST, szAppName, TEXT ("Note-Pin"),
							WS_OVERLAPPED | WS_CAPTION | WS_THICKFRAME, //WS_THICKFRAME for resizable
							352 * iWidth / 512,	309 * iHeight / 512,
							161 * iWidth / 512,	177 * iHeight / 512,
							NULL, NULL, hInstance, NULL);

	ShowWindow (hwnd, iCmdShow);
	UpdateWindow (hwnd);

	hAccel = LoadAccelerators (hInstance,  MAKEINTRESOURCE (IDR_ACCELERATOR));

	while (GetMessage (&msg, NULL, 0, 0))
	{
		if (!TranslateAccelerator (hwnd, hAccel, &msg))
		{
			TranslateMessage (&msg);
			DispatchMessage (&msg);
		}
	}
	return (int) msg.wParam;
}


LRESULT CALLBACK WndProc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	static int	cxChar, cxCaps, cyChar;
	TEXTMETRIC	tm;
	HDC			hdc;
	PAINTSTRUCT	ps;

	static int				cxClient, cyClient;
	static HINSTANCE		hInstance;
	static HWND				hwndEdit;
	static POINT			pt;
	static BOOL				fIsRestore = TRUE, fHide = FALSE , fAuto = FALSE;
	static RECT				rect;
	static HFONT			hFont;
	static HMENU			hMenu;
	static HKEY				hKey;
	static NOTIFYICONDATA	Notify;
	static SYSTEMTIME		st;
	
	static TCHAR			szFullpath[MAX_PATH];
	static SHELLEXECUTEINFO	sei = {0};

	PTSTR	pTmp;
	DWORD	cbSize, dType;
	int		i;

	WIN32_FILE_ATTRIBUTE_DATA	wfad;
	FILETIME					ftLocal;
	TCHAR						szBuffer[1000];

	static TextStack *text;

	switch (message)
	{
	case WM_CREATE:

		hdc = GetDC (hwnd);

		GetTextMetrics (hdc, &tm);
		cxChar = tm.tmAveCharWidth;
		cxCaps = (tm.tmPitchAndFamily & 1 ? 3 : 2) * cxChar / 2;
		cyChar = tm.tmHeight + tm.tmExternalLeading;

		ReleaseDC (hwnd, hdc);

		hInstance = ((LPCREATESTRUCT) lParam) -> hInstance;
		hwndEdit = CreateWindow (TEXT ("edit"), NULL,
								WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL,
								0, 0, 0, 0, hwnd, (HMENU) ID_EDIT, hInstance, NULL);
#ifndef _WIN64
		OldProc = (WNDPROC) SetWindowLong (hwndEdit, GWL_WNDPROC, (LPARAM) TempProc);
#else
		OldProc = (WNDPROC) SetWindowLongPtr (hwndEdit, GWLP_WNDPROC, (LONG_PTR) TempProc);
#endif
		text = new TextStack (hwndEdit);

		GetModuleFileName (NULL, szFullpath, MAX_PATH);
		for (i = 0; i < lstrlen(szFullpath); i++)
			if (szFullpath[i] == '.')
			if ((szFullpath[i+1] == 'e' && szFullpath[i+2] == 'x' && szFullpath[i+3] == 'e') || \
				(szFullpath[i+1] == 'E' && szFullpath[i+2] == 'X' && szFullpath[i+3] == 'E'))
				break;
		for (; i > 0; i--)
			if (szFullpath[i] == '\\')
				break;
		lstrcpyn (szBuffer, szFullpath, i + 1);
		SetCurrentDirectory (szBuffer);

		fIsRestore &= FileRead (hwndEdit, TEXT ("TempStore.txt"));
		fIsRestore &= GetFileAttributesEx (TEXT ("TempStore.txt"), GetFileExInfoStandard, &wfad);
		fIsRestore &= FileTimeToLocalFileTime (&wfad.ftLastWriteTime, &ftLocal);
		fIsRestore &= FileTimeToSystemTime (&ftLocal, &st);

		hFont = CreateFont (-15, 0, 0, 0, 400, 0, 0, 0, 0, 
							OUT_CHARACTER_PRECIS, CLIP_CHARACTER_PRECIS, DEFAULT_QUALITY, FF_SWISS,
							TEXT("微软雅黑"));
		SendMessage (hwndEdit, WM_SETFONT, (WPARAM) hFont, 0);
		SendMessage (hwndEdit, EM_SETSEL, GetWindowTextLength (hwndEdit) + 1, GetWindowTextLength (hwndEdit) + 1);
		SendMessage (hwndEdit, EM_LINESCROLL, 0, SendMessage (hwndEdit, EM_GETLINECOUNT, 0, 0));

		hMenu = GetSystemMenu (hwnd, FALSE);
		DeleteMenu (hMenu, SC_MAXIMIZE,	MF_BYCOMMAND);
		DeleteMenu (hMenu, SC_MINIMIZE,	MF_BYCOMMAND);
		DeleteMenu (hMenu, SC_RESTORE,	MF_BYCOMMAND);
		AppendMenu (hMenu, MF_STRING,	IDM_QUIT,	TEXT ("Quit"));

		hMenu = CreatePopupMenu ();
		if (fUAC)	AppendMenu (hMenu, MF_STRING, IDM_AUTO, TEXT ("Auto Run"));
		else		AppendMenu (hMenu, MF_STRING, IDM_AUTO, TEXT ("Auto Run (UAC)"));
		AppendMenu (hMenu, MF_STRING, IDM_SHOW, TEXT ("Hide"));
		AppendMenu (hMenu, MF_SEPARATOR, 0, 0);
		AppendMenu (hMenu, MF_STRING, IDM_QUIT, TEXT ("Quit"));

		Notify.cbSize = (DWORD) sizeof (NOTIFYICONDATA);
		Notify.hWnd = hwnd;
		Notify.uID = IDN_TRAY;
		Notify.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
		Notify.uCallbackMessage = WM_TRAY;
		Notify.hIcon = LoadIcon (hInstance, MAKEINTRESOURCE (IDI_ICON2));
		wcscpy_s (Notify.szTip, sizeof TEXT ("Note Pin"), TEXT ("Note Pin"));
		Shell_NotifyIcon (NIM_ADD, &Notify);

		if (fUAC)
		{
			RegOpenKey (HKEY_LOCAL_MACHINE, TEXT ("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run"), &hKey);
			RegQueryValueEx (hKey, TEXT ("Note Pin"), 0, &dType, NULL, &cbSize);
			pTmp = (PTSTR) malloc (cbSize);
			RegQueryValueEx (hKey, TEXT ("Note Pin"), 0, &dType, (LPBYTE) pTmp, &cbSize);
			fAuto = lstrcmpi (szFullpath, pTmp) ? FALSE : TRUE;
			free (pTmp);

			if (fAuto)	ModifyMenu (hMenu, IDM_AUTO, MF_BYCOMMAND | MF_CHECKED, IDM_AUTO, TEXT ("Auto Run"));
			else		ModifyMenu (hMenu, IDM_AUTO, MF_BYCOMMAND | MF_UNCHECKED, IDM_AUTO, TEXT ("Auto Run"));
		}
		else
		{
			RegOpenKeyEx (HKEY_LOCAL_MACHINE, TEXT ("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run"),
							0, KEY_QUERY_VALUE, &hKey);
			RegQueryValueEx (hKey, TEXT ("Note Pin"), 0, &dType, NULL, &cbSize);
			pTmp = (PTSTR) malloc (cbSize);
			RegQueryValueEx (hKey, TEXT ("Note Pin"), 0, &dType, (LPBYTE) pTmp, &cbSize);
			fAuto = lstrcmpi (szFullpath, pTmp) ? FALSE : TRUE;
			free (pTmp);

			if (fAuto)	ModifyMenu (hMenu, IDM_AUTO, MF_BYCOMMAND | MF_CHECKED, IDM_AUTO, TEXT ("Auto Run (UAC)"));
			else		ModifyMenu (hMenu, IDM_AUTO, MF_BYCOMMAND | MF_UNCHECKED, IDM_AUTO, TEXT ("Auto Run (UAC)"));
		}
		return 0;

	case WM_PAINT:

		hdc = BeginPaint (hwnd, &ps);
		SetBkMode (hdc, TRANSPARENT);
		SelectObject (hdc, hFont);

		fIsRestore &= GetFileAttributesEx (TEXT ("TempStore.txt"), GetFileExInfoStandard, &wfad);
		fIsRestore &= FileTimeToLocalFileTime (&wfad.ftLastWriteTime, &ftLocal);
		fIsRestore &= FileTimeToSystemTime (&ftLocal, &st);

		if (fIsRestore)
		{
			TextOut (hdc, cxChar, cyClient - cyChar * 17 / 16, szBuffer,
					wsprintf (szBuffer, TEXT ("Last Edited: %4d/%#02d/%#02d  %#02d:%#02d:%#02d"),
					st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond));

			TextOut (hdc, cxClient - (lstrlen (TEXT("© 2015 LJN")) + 2) * cxChar, cyClient - cyChar * 17 / 16,
					TEXT("© 2015 LJN"), lstrlen (TEXT("© 2015 LJN")));

			MoveToEx  (hdc, 0, cyClient - cyChar * 35 / 32, NULL);
			LineTo    (hdc, cxClient, cyClient - cyChar * 35 / 32);
		}
		EndPaint (hwnd, &ps);
		
		if (fIsRestore)	MoveWindow (hwndEdit, 0, 0, cxClient, cyClient - cyChar * 18 / 16, TRUE);
		else			MoveWindow (hwndEdit, 0, 0, cxClient, cyClient, TRUE);
		return 0;

	case WM_SIZE:

		cxClient = LOWORD (lParam);
		cyClient = HIWORD (lParam);
		return 0;

	case WM_SETFOCUS:

		SetFocus (hwndEdit);
		return 0;

	case WM_TRAY:

		switch (lParam)
		{
		case WM_LBUTTONUP:

			SendMessage (hwnd, WM_COMMAND, MAKELONG (IDM_SHOW, 0), 0);
			break;

		case WM_RBUTTONUP:

			if (fHide)	ModifyMenu (hMenu, IDM_SHOW, MF_BYCOMMAND | MF_STRING, IDM_SHOW, TEXT ("Restore"));
			else		ModifyMenu (hMenu, IDM_SHOW, MF_BYCOMMAND | MF_STRING, IDM_SHOW, TEXT ("Hide"));

			SetForegroundWindow (hwnd);
			GetCursorPos (&pt);
			TrackPopupMenu (hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
			break;
		} 
		return 0;

	case WM_COMMAND:
	case WM_SYSCOMMAND:

		if (lParam && LOWORD (wParam) == ID_EDIT)
		{
			switch (HIWORD (wParam))
			{				
			case EN_ERRSPACE:
			case EN_MAXTEXT:

				MessageBox (NULL, TEXT ("Too Many Characters!!!"), NULL, MB_ICONWARNING);
				return 0;

			case EN_UPDATE:

				text -> Update ();
				return 0;
			}
			break;
		}

		switch (LOWORD (wParam))
		{
		case IDM_SELALL:

			SendMessage (hwndEdit, EM_SETSEL, 0, -1);
			return 0;

		case IDM_SAVE:

			FileWrite (hwndEdit, TEXT ("TempStore.txt"));
			fIsRestore = TRUE;
			InvalidateRect (hwnd, NULL, TRUE);
			return 0;

		case IDM_MY_UNDO:

			if (!text -> Undo ()) MessageBeep (MB_ICONWARNING);
			SendMessage (hwndEdit, EM_SETSEL, GetWindowTextLength (hwndEdit) + 1, GetWindowTextLength (hwndEdit) + 1);
			SendMessage (hwndEdit, EM_LINESCROLL, 0, SendMessage (hwndEdit, EM_GETLINECOUNT, 0, 0));
			return 0;

		case IDM_MY_REDO:
			if (!text -> Redo ()) MessageBeep (MB_ICONWARNING);
			SendMessage (hwndEdit, EM_SETSEL, GetWindowTextLength (hwndEdit) + 1, GetWindowTextLength (hwndEdit) + 1);
			SendMessage (hwndEdit, EM_LINESCROLL, 0, SendMessage (hwndEdit, EM_GETLINECOUNT, 0, 0));
			return 0;

		case IDM_SHOW:

			if (fHide)
			{
				ShowWindow (hwnd, SW_SHOW);
				fHide = FALSE;
				SetForegroundWindow (hwnd);
			}
			else
			{
				FileWrite (hwndEdit, TEXT ("TempStore.txt"));
				ShowWindow (hwnd, SW_HIDE);
				fHide = fIsRestore = TRUE;
				text -> Clear();
			}
			return 0;

		case IDM_AUTO:

			if (!fUAC)
			{
				sei.cbSize = sizeof (SHELLEXECUTEINFO);
				sei.lpVerb = TEXT ("runas");
				sei.lpFile = szFullpath;
				sei.lpParameters = TEXT ("runas");
				sei.nShow = SW_SHOW;

				if (ShellExecuteEx (&sei))
					SendMessage (hwnd, WM_DESTROY, 0, 0);
			}
			else
			{
				if (fAuto)
				{
					ModifyMenu (hMenu, IDM_AUTO, MF_BYCOMMAND | MF_UNCHECKED, IDM_AUTO, TEXT ("Auto Run"));
					RegDeleteKeyValue (HKEY_LOCAL_MACHINE,
								TEXT ("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run"), TEXT ("Note Pin"));
					fAuto = FALSE;
				}
				else
				{
					ModifyMenu (hMenu, IDM_AUTO, MF_BYCOMMAND | MF_CHECKED, IDM_AUTO, TEXT ("Auto Run"));
					RegSetValueEx (hKey, TEXT ("Note Pin"), 0, REG_SZ, (CONST BYTE*) szFullpath, (lstrlen (szFullpath) + 1) * 2);
					fAuto = TRUE;
				}
			}
			return 0;

		case IDM_QUIT:

			SendMessage (hwnd, WM_DESTROY, 0, 0);
			return 0;
		}
		break;

	case WM_DISPLAY:  //Switched from another new instance

		ShowWindow (hwnd, SW_SHOW);
		fHide = FALSE;
		SetActiveWindow (hwnd);
		SetForegroundWindow (hwnd);
		return 0;

	case WM_CLOSE:

		SendMessage (hwnd, WM_COMMAND, MAKELONG (IDM_SHOW, 0), 0);
		return 0;

	case WM_QUERYENDSESSION:
	case WM_DESTROY:

		delete text;
		FileWrite (hwndEdit, TEXT ("TempStore.txt"));
		RegCloseKey (hKey);
		DeleteObject (hFont);
		DestroyMenu (hMenu);
		DestroyIcon (Notify.hIcon);
		Shell_NotifyIcon (NIM_DELETE, &Notify);

		PostQuitMessage (0);
		return 0;
	}
	return DefWindowProc (hwnd, message, wParam, lParam);
}

LRESULT CALLBACK TempProc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	static BOOL iOnce = TRUE;

	if (message == WM_PAINT && iOnce)
	{
		SendMessage (GetParent (hwnd), WM_COMMAND, MAKELONG (ID_EDIT, EN_UPDATE), (LPARAM) hwnd);
		iOnce = FALSE;
	}

	return CallWindowProc (OldProc, hwnd, message, wParam, lParam);
}
