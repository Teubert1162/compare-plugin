#pragma comment (lib, "comctl32")


#include "ProgressDlg.h"
#include "Compare.h"
#include <windowsx.h>
#include <stdlib.h>


const TCHAR ProgressDlg::cClassName[]     = TEXT("CompareProgressClass");
const int ProgressDlg::cBackgroundColor   = COLOR_3DFACE;
const int ProgressDlg::cPBwidth           = 600;
const int ProgressDlg::cPBheight          = 10;
const int ProgressDlg::cBTNwidth          = 80;
const int ProgressDlg::cBTNheight         = 25;


std::unique_ptr<ProgressDlg> ProgressDlg::Inst;


void ProgressDlg::Open(const TCHAR* msg)
{
	if ((bool)Inst)
		return;

	Inst.reset(new ProgressDlg);

	if (Inst->create() == NULL)
	{
		Inst.reset();
		return;
	}

	Inst->setInfo(msg);

	::EnableWindow(nppData._nppHandle, FALSE);
}


bool ProgressDlg::Update(int mid)
{
	if (!Inst)
		return false;

	if (Inst->cancelled())
		return false;

	if (mid > Inst->_max)
		Inst->_max = mid;

	if (Inst->_max)
	{
		int perc = (++Inst->_count * 100) / (Inst->_max * 4);
		Inst->setPercent(perc);
	}

	return true;
}


bool ProgressDlg::IsCancelled()
{
	if (!Inst)
		return true;

	if (Inst->cancelled())
	{
		Close();
		return true;
	}

	return false;
}


void ProgressDlg::Close()
{
	if (!Inst)
		return;

	::EnableWindow(nppData._nppHandle, TRUE);
	::SetForegroundWindow(nppData._nppHandle);

	Inst.reset();
}


ProgressDlg::ProgressDlg() : _hwnd(NULL),  _hKeyHook(NULL), _max(0), _count(0)
{
	::GetModuleHandleEx(
		GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
		GET_MODULE_HANDLE_EX_FLAG_PIN, cClassName, &_hInst);

	WNDCLASSEX wcex;

	::SecureZeroMemory(&wcex, sizeof(wcex));
	wcex.cbSize           = sizeof(wcex);
	wcex.style            = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc      = wndProc;
	wcex.hInstance        = _hInst;
	wcex.hCursor          = ::LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground    = ::GetSysColorBrush(cBackgroundColor);
	wcex.lpszClassName    = cClassName;

	::RegisterClassEx(&wcex);

	INITCOMMONCONTROLSEX icex;

	::SecureZeroMemory(&icex, sizeof(icex));
	icex.dwSize = sizeof(icex);
	icex.dwICC  = ICC_STANDARD_CLASSES | ICC_PROGRESS_CLASS;

	::InitCommonControlsEx(&icex);
}


ProgressDlg::~ProgressDlg()
{
    if (_hKeyHook)
        ::UnhookWindowsHookEx(_hKeyHook);

    destroy();

	::UnregisterClass(cClassName, _hInst);
}


HWND ProgressDlg::create()
{
    // Create manually reset non-signaled event
    _hActiveState = ::CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!_hActiveState)
        return NULL;

	for (HWND hwnd = nppData._nppHandle; hwnd; hwnd = ::GetParent(hwnd))
		::UpdateWindow(hwnd);

    _hThread = ::CreateThread(NULL, 0, threadFunc, this, 0, NULL);
    if (!_hThread)
    {
        ::CloseHandle(_hActiveState);
        return NULL;
    }

    // Wait for the progress window to be created
    ::WaitForSingleObject(_hActiveState, INFINITE);

    // On progress window create fail
    if (!_hwnd)
    {
        ::WaitForSingleObject(_hThread, INFINITE);
        ::CloseHandle(_hThread);
		::CloseHandle(_hActiveState);
    }

    return _hwnd;
}


void ProgressDlg::cancel()
{
	::ResetEvent(_hActiveState);
	::EnableWindow(_hBtn, FALSE);
	setInfo(TEXT("Cancelling operation, please wait..."));
}


void ProgressDlg::destroy()
{
    if (_hwnd)
    {
		::PostMessage(_hwnd, WM_CLOSE, 0, 0);
		_hwnd = NULL;

        ::WaitForSingleObject(_hThread, INFINITE);
        ::CloseHandle(_hThread);
        ::CloseHandle(_hActiveState);
    }
}


DWORD WINAPI ProgressDlg::threadFunc(LPVOID data)
{
    ProgressDlg* pw = static_cast<ProgressDlg*>(data);
    return (DWORD)pw->thread();
}


BOOL ProgressDlg::thread()
{
    BOOL r = createProgressWindow();
    ::SetEvent(_hActiveState);
    if (!r)
        return r;

    // Window message loop
    MSG msg;
    while ((r = ::GetMessage(&msg, NULL, 0, 0)) != 0 && r != -1)
        ::DispatchMessage(&msg);

	return r;
}


BOOL ProgressDlg::createProgressWindow()
{
	_hwnd = ::CreateWindowEx(
		WS_EX_APPWINDOW | WS_EX_TOOLWINDOW | WS_EX_OVERLAPPEDWINDOW,
            cClassName, TEXT("Compare Plugin"), WS_POPUP | WS_CAPTION,
            CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
            NULL, NULL, _hInst, (LPVOID)this);
    if (!_hwnd)
        return FALSE;

    int width = cPBwidth + 10;
    int height = cPBheight + cBTNheight + 35;
    RECT win = adjustSizeAndPos(width, height);
    ::MoveWindow(_hwnd, win.left, win.top, win.right - win.left, win.bottom - win.top, TRUE);

    ::GetClientRect(_hwnd, &win);
    width = win.right - win.left;
    height = win.bottom - win.top;

	_hPText = ::CreateWindowEx(0, TEXT("STATIC"), TEXT(""),
			WS_CHILD | WS_VISIBLE | BS_TEXT | SS_PATHELLIPSIS,
			5, 5, width - 10, 20, _hwnd, NULL, _hInst, NULL);

    _hPBar = ::CreateWindowEx(0, PROGRESS_CLASS, TEXT("Progress Bar"),
            WS_CHILD | WS_VISIBLE | PBS_SMOOTH,
            5, 25, width - 10, cPBheight,
            _hwnd, NULL, _hInst, NULL);
    ::SendMessage(_hPBar, PBM_SETRANGE, 0, MAKELPARAM(0, 100));

    _hBtn = ::CreateWindowEx(0, TEXT("BUTTON"), TEXT("Cancel"),
            WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | BS_TEXT,
            (width - cBTNwidth) / 2, height - cBTNheight - 5,
            cBTNwidth, cBTNheight, _hwnd, NULL, _hInst, NULL);

	HFONT hf = (HFONT)::GetStockObject(DEFAULT_GUI_FONT);
	if (hf)
	{
		::SendMessage(_hPText, WM_SETFONT, (WPARAM)hf, MAKELPARAM(TRUE, 0));
		::SendMessage(_hBtn, WM_SETFONT, (WPARAM)hf, MAKELPARAM(TRUE, 0));
	}

    _hKeyHook = ::SetWindowsHookEx(WH_KEYBOARD, keyHookProc, NULL, GetCurrentThreadId());

    ::ShowWindow(_hwnd, SW_SHOWNORMAL);
    ::UpdateWindow(_hwnd);

    return TRUE;
}


RECT ProgressDlg::adjustSizeAndPos(int width, int height)
{
	RECT maxWin;
	maxWin.left		= ::GetSystemMetrics(SM_XVIRTUALSCREEN);
	maxWin.top		= ::GetSystemMetrics(SM_YVIRTUALSCREEN);
	maxWin.right	= ::GetSystemMetrics(SM_CXVIRTUALSCREEN) + maxWin.left;
	maxWin.bottom	= ::GetSystemMetrics(SM_CYVIRTUALSCREEN) + maxWin.top;

	POINT center;

	{
		RECT biasWin;
		::GetWindowRect(nppData._nppHandle, &biasWin);
		center.x = (biasWin.left + biasWin.right) / 2;
		center.y = (biasWin.top + biasWin.bottom) / 2;
	}

	RECT win = maxWin;
	win.right = win.left + width;
	win.bottom = win.top + height;

	::AdjustWindowRectEx(&win, ::GetWindowLongPtr(_hwnd, GWL_STYLE), FALSE, ::GetWindowLongPtr(_hwnd, GWL_EXSTYLE));

	width = win.right - win.left;
	height = win.bottom - win.top;

	if (width < maxWin.right - maxWin.left)
	{
		win.left = center.x - width / 2;
		if (win.left < maxWin.left)
			win.left = maxWin.left;
		win.right = win.left + width;
		if (win.right > maxWin.right)
		{
			win.right = maxWin.right;
			win.left = win.right - width;
		}
	}
	else
	{
		win.left = maxWin.left;
		win.right = maxWin.right;
	}

	if (height < maxWin.bottom - maxWin.top)
	{
		win.top = center.y - height / 2;
		if (win.top < maxWin.top)
			win.top = maxWin.top;
		win.bottom = win.top + height;
		if (win.bottom > maxWin.bottom)
		{
			win.bottom = maxWin.bottom;
			win.top = win.bottom - height;
		}
	}
	else
	{
		win.top = maxWin.top;
		win.bottom = maxWin.bottom;
	}

	return win;
}


LRESULT CALLBACK ProgressDlg::keyHookProc(int code, WPARAM wParam, LPARAM lParam)
{
    if (code >= 0 && (bool)Inst)
    {
        if (Inst->_hBtn == ::GetFocus())
        {
            // Key is pressed
            if (!(lParam & (1 << 31)))
            {
                if (wParam == VK_RETURN || wParam == VK_ESCAPE)
                {
                    Inst->cancel();
                    return 1;
                }
            }
        }
    }

    return ::CallNextHookEx(NULL, code, wParam, lParam);
}


LRESULT APIENTRY ProgressDlg::wndProc(HWND hwnd, UINT umsg, WPARAM wparam, LPARAM lparam)
{
    switch (umsg)
    {
        case WM_CREATE:
            return 0;

        case WM_SETFOCUS:
            ::SetFocus(Inst->_hBtn);
            return 0;

        case WM_COMMAND:
            if (HIWORD(wparam) == BN_CLICKED)
            {
				Inst->cancel();
                return 0;
            }
            break;

        case WM_DESTROY:
            ::PostQuitMessage(0);
            return 0;
    }

    return ::DefWindowProc(hwnd, umsg, wparam, lparam);
}
