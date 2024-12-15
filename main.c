// Minimal Screensaver
// ---------------------
// (c) 1998 Lucian Wischik. You may do whatever you want with this code, without restriction.
//
// This code is a basic minimal screensaver. It does not use SCRNSAVE.LIB or any other library:
// absolutely all the code is in this source file.
// The saver works perfectly fine under '95 and NT. It respects the Plus! configuration settings
// (password delay, mouse movement threshold). It can handle passwords correctly.
// It makes all the correct calls in the correct places, including calls that are undocumented
// by Microsoft.
// This code accompanies the guide 'How To Write a 32bit Screen Saver'. All documentation for this
// code is in that guide. It can be found at www.wischik.com/scr/
//
// Notes:
// 0. All of this is a real hassle. If you used my ScrPlus library then it would all be much much
// easier. And you'd get lots of extra features, like a standard 'Plus!' style configuration dialog
// with preview in it, and proper handling of hot corners under NT as well as '95, and a high
// performance multimedia timer, and lots of examples including some that use full-screen DirectDraw.
// www.wischik.com/scr/
// If you have C++Builder then you should use my ScrPlus/C++Builder library which has an expert
// and components for easily generating screensavers.
// If you remain blind to the joys of using ScrPlus and are willing to waste time programming it
// all yourself, then read on...
// 1. Having a 'DEBUG' flag, with diagnostic output, is ABSOLUTELY ESSENTIAL. I can guarantee
// that if you develop a screensaver without diagnostic output for every single message that
// you handle, your screensaver will crash and you won't know why.
// 2. If you also wanted to write a configuration dialog that was able to set the standard Plus!
// options, you'd need to use two additional calls: SystemAgentDetect, and ScreenSaverChanged.
// They are documented in my 'how to write a 32bit screensaver' technical guide.
// 3. NT and '95 handle passwords differently. Under NT, the saver must terminate and then the
// verify-password dialog comes up. If the user fails, then the screensaver is launched again from
// scratch. Under '95, the password dialog comes up while the saver is running.
// 4. You should probably use WM_TIMER messages for your animation, rather than idle-processing.
// By using WM_TIMER messages your animation will keep going even when (under '95) the password
// dialog is up.
// 5. Changing the saver to allow interraction is easy. All you have to do is figure out which
// messages (keyboard, mouse, ...) will be used by you and stop them from closing the window.
// 6. Changing the saver to implement your own password routine is easy under '95: all you have
// to do is change the VerifyPassword routine. Under NT it's not really possible.

// Modified by me, Edw590 in 2024.

#include <windows.h>
#include "Utils/General.h"

enum TScrMode {
	MODE_NONE,
	MODE_PASSWD,
	MODE_PREVIEW,
	MODE_SAVER
};
enum TScrMode scr_mode_GL = MODE_NONE;
HINSTANCE hInstance_GL = NULL;
HWND hScrWindow_GL = NULL;

struct TSaverSettings {
	HWND hwnd;
	DWORD PasswordDelay;   // in seconds
	DWORD MouseThreshold;  // in pixels
	POINT InitCursorPos;
	DWORD InitTime;        // in ms
	UINT  idTimer;         // a timer id, because this particular saver uses a timer
	BOOL  IsDialogActive;
	BOOL  ReallyClose;     // for NT, so we know if a WM_CLOSE came from us or it.
};
struct TSaverSettings ss = {0};

BOOL VerifyPassword(HWND hwnd) {
	// Under NT, we return TRUE immediately. This lets the saver quit, and the system manages passwords.
	// Under '95, we call VerifyScreenSavePwd. This checks the appropriate registry key and, if necessary, pops up a verify dialog
	OSVERSIONINFO osv;
	osv.dwOSVersionInfoSize=sizeof(osv);
	GetVersionEx(&osv);
	if (osv.dwPlatformId == VER_PLATFORM_WIN32_NT) {
		return TRUE;
	}

	HINSTANCE hpwdcpl = LoadLibrary("PASSWORD.CPL");
	if (hpwdcpl == NULL) {
		return TRUE;
	}

	typedef BOOL (WINAPI *VERIFYSCREENSAVEPWD) (HWND hwnd);
	VERIFYSCREENSAVEPWD VerifyScreenSavePwd;
	VerifyScreenSavePwd = (VERIFYSCREENSAVEPWD) GetProcAddress(hpwdcpl, "VerifyScreenSavePwd");
	if (VerifyScreenSavePwd == NULL) {
		FreeLibrary(hpwdcpl);

		return TRUE;
	}

	BOOL bres=VerifyScreenSavePwd(hwnd);
	FreeLibrary(hpwdcpl);

	return bres;
}

void ChangePassword(HWND hwnd) {
	// This only ever gets called under '95, when started with the /a option.
	HINSTANCE hmpr = LoadLibrary("MPR.DLL");
	if (hmpr == NULL) {
		return;
	}

	typedef VOID (WINAPI *PWDCHANGEPASSWORD) (LPCSTR lpcRegkeyname, HWND hwnd, UINT uiReserved1, UINT uiReserved2);
	PWDCHANGEPASSWORD PwdChangePassword = (PWDCHANGEPASSWORD) GetProcAddress(hmpr, "PwdChangePasswordA");
	if (PwdChangePassword == NULL) {
		FreeLibrary(hmpr);

		return;
	}

	PwdChangePassword("SCRSAVE", hwnd, 0, 0);
	FreeLibrary(hmpr);
}

void CloseSaverWindow() {
	ss.ReallyClose = TRUE;
	PostMessage(ss.hwnd, WM_CLOSE, 0, 0);
}
void StartDialog() {
	ss.IsDialogActive = TRUE;
	SendMessage(ss.hwnd, WM_SETCURSOR, 0, 0);
}
void EndDialog2() {
	ss.IsDialogActive = FALSE;
	SendMessage(ss.hwnd, WM_SETCURSOR, 0, 0);
	GetCursorPos(&ss.InitCursorPos);
}

LRESULT CALLBACK SaverWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	static HBITMAP hBitmap;
	HDC hdc;
	PAINTSTRUCT ps;
	BITMAP bitmap;
	HDC hdcMem;
	HGDIOBJ oldBitmap;

	switch (msg) {
		case WM_CREATE: {
			ss.hwnd = hwnd;

			hBitmap = (HBITMAP) LoadImage(NULL, TEXT("C:\\Users\\Edw590\\CLionProjects\\MyScreensaver\\teste.bmp"),
			                              IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);

			if (hBitmap == NULL) {
				MessageBoxPrintf("Error", "Failed to load image");
			}

			return 0;
		}
		case WM_PAINT: {
			hdc = BeginPaint(hwnd, &ps);

			hdcMem = CreateCompatibleDC(hdc);
			oldBitmap = SelectObject(hdcMem, hBitmap);

			GetObject(hBitmap, sizeof(bitmap), &bitmap);

			double aspect_ratio = (double) bitmap.bmWidth / bitmap.bmHeight;
			StretchBlt(hdc, 0, 0, (int) (1080 * aspect_ratio), 1080, hdcMem, 0, 0, bitmap.bmWidth, bitmap.bmHeight, SRCCOPY);

			SelectObject(hdcMem, oldBitmap);
			DeleteDC(hdcMem);

			EndPaint(hwnd, &ps);

			return 0;
		}
		case WM_ACTIVATE:
		case WM_ACTIVATEAPP:
		case WM_NCACTIVATE: {
			if (scr_mode_GL == MODE_SAVER && !ss.IsDialogActive && LOWORD(wParam) == WA_INACTIVE) {
				CloseSaverWindow();
			}

			return 0;
		}
		case WM_SETCURSOR: {
			if (scr_mode_GL == MODE_SAVER && !ss.IsDialogActive) {
				SetCursor(NULL);
			} else {
				SetCursor(LoadCursor(NULL,IDC_ARROW));
			}

			return FALSE;
		}
		case WM_LBUTTONDOWN:
		case WM_MBUTTONDOWN:
		case WM_RBUTTONDOWN:
		case WM_KEYDOWN: {
			if (scr_mode_GL == MODE_SAVER && !ss.IsDialogActive) {
				CloseSaverWindow();
			}

			return 0;
		}
		case WM_MOUSEMOVE: {
			if (scr_mode_GL == MODE_SAVER && !ss.IsDialogActive) {
				POINT pt;
				GetCursorPos(&pt);
				int dx = pt.x - ss.InitCursorPos.x;
				if (dx < 0) {
					dx = -dx;
					}
				int dy = pt.y - ss.InitCursorPos.y;
				if (dy<0) {
					dy = -dy;
				}
				if ((dx > (int) ss.MouseThreshold) || (dy > (int) ss.MouseThreshold)) {
					CloseSaverWindow();
				}
			}

			return 0;
		}
		case WM_SYSCOMMAND: {
			if (scr_mode_GL == MODE_SAVER) {
				if (wParam == SC_SCREENSAVE || wParam == SC_CLOSE) {
					return 0;
				}
			}

			break;
		}
		case WM_CLOSE: {
			if (scr_mode_GL == MODE_SAVER && ss.ReallyClose && !ss.IsDialogActive) {
				BOOL CanClose=TRUE;
				if (GetTickCount() - ss.InitTime > 1000 * ss.PasswordDelay) {
					StartDialog();
					CanClose=VerifyPassword(hwnd);
					EndDialog2();
				}

				if (CanClose) {
					DestroyWindow(hwnd);
				}
			}

			if (scr_mode_GL == MODE_SAVER) {
				return 0; // so that DefWindowProc doesn't get called, because it would just DestroyWindow
			}

			break;
		}
		case WM_DESTROY: {
			if (ss.idTimer != 0) {
				KillTimer(hwnd, ss.idTimer);
			}
			ss.idTimer = 0;
			DeleteObject(hBitmap);
			PostQuitMessage(0);

			return 0;
		}
		default:
			break;
	}

	return DefWindowProc(hwnd, msg, wParam, lParam);
}

void DoSaver(HWND hparwnd) {
	WNDCLASS wnd_class;
	wnd_class.style = CS_HREDRAW | CS_VREDRAW;
	wnd_class.lpfnWndProc = SaverWindowProc;
	wnd_class.cbClsExtra = 0;
	wnd_class.cbWndExtra = 0;
	wnd_class.hInstance = hInstance_GL;
	wnd_class.hIcon = NULL;
	wnd_class.hCursor = NULL;
	wnd_class.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	wnd_class.lpszMenuName = NULL;
	wnd_class.lpszClassName = "ScrClass";

	if (RegisterClass(&wnd_class) == 0) {
		return;
	}

	if (scr_mode_GL == MODE_PREVIEW) {
		RECT rc;
		GetWindowRect(hparwnd,&rc);
		int cx = rc.right - rc.left;
		int cy = rc.bottom - rc.top;
		hScrWindow_GL = CreateWindowEx(0, "ScrClass", "Edw590", WS_CHILD | WS_VISIBLE, 0, 0, cx, cy, hparwnd, NULL,
									   hInstance_GL, NULL);
	} else {
		int cx = GetSystemMetrics(SM_CXSCREEN);
		cx = 1000;
		int cy = GetSystemMetrics(SM_CYSCREEN);
		hScrWindow_GL = CreateWindowEx(WS_EX_TOPMOST, "ScrClass", "Edw590", WS_POPUP | WS_VISIBLE, 0, 0, cx, cy, NULL,
									   NULL, hInstance_GL, NULL);
	}

	if (hScrWindow_GL == NULL) {
		return;
	}

	UINT dummy;
	if (scr_mode_GL == MODE_SAVER) {
		SystemParametersInfo(SPI_SCREENSAVERRUNNING, 1, &dummy, 0);
	}

	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	if (scr_mode_GL == MODE_SAVER) {
		SystemParametersInfo(SPI_SCREENSAVERRUNNING, 0, &dummy, 0);
	}
}

// This routine is for using ScrPrev. It's so that you can start the saver
// with the command line /p scrprev and it runs itself in a preview window.
// You must first copy ScrPrev somewhere in your search path
HWND CheckForScrprev() {
	HWND hwnd = FindWindow("Scrprev", NULL); // looks for the Scrprev class
	if (hwnd == NULL) {
		// try to load it
		STARTUPINFO si;
		PROCESS_INFORMATION pi;
		ZeroMemory(&si, sizeof(si));
		ZeroMemory(&pi, sizeof(pi));

		si.cb = sizeof(si);
		si.lpReserved = NULL;
		si.lpTitle = NULL;
		si.dwFlags = 0;
		si.cbReserved2 = 0;
		si.lpReserved2 = 0;
		si.lpDesktop = 0;
		BOOL cres = CreateProcess(NULL, "Scrprev", 0, 0, FALSE, CREATE_NEW_PROCESS_GROUP | CREATE_DEFAULT_ERROR_MODE,
								  0, 0, &si, &pi);
		if (!cres) {
			return NULL;
		}

		DWORD wres = WaitForInputIdle(pi.hProcess, 2000);
		if (wres == WAIT_TIMEOUT) {
			return NULL;
		}
		if (wres == 0xFFFFFFFF) {
			return NULL;
		}
		hwnd = FindWindow("Scrprev", NULL);
	}
	if (hwnd==NULL) {
		return NULL;
	}
	SetForegroundWindow(hwnd);
	hwnd = GetWindow(hwnd, GW_CHILD);
	if (hwnd == NULL) {
		return NULL;
	}

	return hwnd;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd) {
	hInstance_GL = hInstance;
	char *c = GetCommandLine();
	if (*c == '\"') {
		c++;
		while (*c != 0 && *c != '\"') {
			c++;
		}
	} else {
		while (*c != 0 && *c != ' ') {
			c++;
		}
	}
	if (*c != 0) {
		c++;
	}
	while (*c == ' ') {
		c++;
	}

	HWND hwnd = NULL;
	if (*c == '\0') {
		scr_mode_GL = MODE_SAVER;
		hwnd = NULL;
	} else {
		if (*c == '-' || *c == '/') {
			c++;
		}
		if (*c == 'p' || *c == 'P' || *c == 'l' || *c == 'L') {
			c++;
			while (*c == ' ' || *c == ':') {
				c++;
			}
			if ((strcmp(c, "scrprev") == 0) || (strcmp(c, "ScrPrev") == 0) || (strcmp(c, "SCRPREV") == 0)) {
				hwnd = CheckForScrprev();
			} else {
				hwnd = (HWND) atoi(c);
			}

			scr_mode_GL = MODE_PREVIEW;
		} else if (*c=='s' || *c=='S') {
			scr_mode_GL = MODE_SAVER;
		} else if (*c=='c' || *c=='C') {
			c++;
			while (*c==' ' || *c==':') {
				c++;
			}
			if (*c == '\0') {
				hwnd=GetForegroundWindow();
			} else {
				hwnd=(HWND) atoi(c);
			}

			MessageBoxPrintf("Edw590", "This screensaver does not support configuration.");
		} else if (*c=='a' || *c=='A') {
			c++;
			while (*c==' ' || *c==':') {
				c++;
			}
			hwnd = (HWND) atoi(c);
			scr_mode_GL = MODE_PASSWD;
		}
	}

	if (scr_mode_GL == MODE_PASSWD) {
		ChangePassword(hwnd);
	}
	if (scr_mode_GL == MODE_SAVER || scr_mode_GL == MODE_PREVIEW) {
		DoSaver(hwnd);
	}

	return 0;
}
