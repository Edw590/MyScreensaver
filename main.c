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

// Original code obtained from: https://www.wischik.com/scr/howtoscr.html.
// Modified by me, Edw590 in 2024.
//
// Note: this is now a Visual Studio 2005 project.

#include <stdio.h>
#include <windows.h>
#include "GeneralUtils.h"
#include "unzip.h"

#define MAX_MONITORS_EDW590 100

enum TScrMode {
	MODE_NONE,
	MODE_PASSWD,
	MODE_PREVIEW,
	MODE_SAVER
};
enum TScrMode scr_mode_GL = MODE_NONE;
HINSTANCE hInstance_GL = NULL;

int image_num_GL = 0;

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

struct MonitorInfo {
	LONG x;
	LONG y;
	LONG width;
	LONG height;
};

int num_monitors_GL = 0;
struct MonitorInfo monitors_GL[MAX_MONITORS_EDW590] = {0};

HBITMAP images_GL[80] = {0};


// The 2 functions below were copied from https://stackoverflow.com/a/8712996/8228163.
__inline int c99_vsnprintf(char *outBuf, size_t size, const char *format, va_list ap) {
    int count = -1;

    if (size != 0) {
	    count = _vsnprintf_s(outBuf, size, _TRUNCATE, format, ap);
    }
    if (count == -1) {
	    count = _vscprintf(format, ap);
    }

    return count;
}
__inline int c99_snprintf(char *outBuf, size_t size, const char *format, ...) {
    int count;
    va_list ap;

    va_start(ap, format);
    count = c99_vsnprintf(outBuf, size, format, ap);
    va_end(ap);

    return count;
}


BOOL VerifyPassword(HWND hwnd) {
	// Under NT, we return TRUE immediately. This lets the saver quit, and the system manages passwords.
	// Under '95, we call VerifyScreenSavePwd. This checks the appropriate registry key and, if necessary, pops up a verify dialog
	OSVERSIONINFO osv;
	osv.dwOSVersionInfoSize = sizeof(osv);
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




/*void writeToFile(char *str) {
	HANDLE hFile = CreateFile("C:\\teste.txt",                // name of the write
	                          FILE_APPEND_DATA,          // open for writing
			                  0,                      // do not share
			                  NULL,                   // default security
			                  OPEN_ALWAYS,             // create new file only
			                  FILE_ATTRIBUTE_NORMAL,  // normal file
			                  NULL);                  // no attr. template

	DWORD dwBytesWritten = 0;
	WriteFile(
			hFile,           // open file handle
			str,      // start of data to write
			strlen(str),  // number of bytes to write
			&dwBytesWritten, // number of bytes that were written
			NULL);            // no overlapped structure

	CloseHandle(hFile);
}*/

HBITMAP getImage(int img_num, HDC hdc) {
	HRSRC hrsrc = FindResource(hInstance_GL, TEXT("ZIPFILE"), RT_RCDATA);
	if (hrsrc == NULL) {
		return NULL;
	}
	DWORD size = SizeofResource(hInstance_GL, hrsrc);
	if (size == 0) {
		return NULL;
	}
	HGLOBAL hglob = LoadResource(hInstance_GL, hrsrc);
	if (hglob == NULL) {
		return NULL;
	}
	void *buf = LockResource(hglob);
	if (buf == NULL) {
		return NULL;
	}
	HZIP hzip = OpenZip(buf, size, ZIP_MEMORY);
	if (hzip == NULL) {
		return NULL;
	}

	char image_name[100] = {0};
	c99_snprintf(image_name, sizeof(image_name), "%d.bmp", img_num);

	ZIPENTRY zip_entry = {0};
	int index = 0;
	FindZipItem(hzip, image_name, TRUE, &index, &zip_entry);
	if (index == -1) {
		return NULL;
	}

	char *image_buf = (char *) malloc(zip_entry.unc_size);
	if (image_buf == NULL) {
		return NULL;
	}

	long unc_size = zip_entry.unc_size;
	DWORD zip_result = UnzipItem(hzip, index, image_buf, zip_entry.unc_size, ZIP_MEMORY);
	while (zip_result == ZR_MORE) {
		unc_size++;
		zip_result = UnzipItem(hzip, index, image_buf, unc_size, ZIP_MEMORY);
	}

	if (zip_result != ZR_OK) {
		char str[100] = {0};
		c99_snprintf(str, sizeof(str), "ZIP error: %lu; index: %d; unc_size: %lu\n", zip_result, index, zip_entry.unc_size);

		return NULL;
	}

	CloseZip(hzip);



	if (unc_size < sizeof(BITMAPFILEHEADER)) {
		return NULL;
	}

	// Parse BMP headers
	const BITMAPFILEHEADER *fileHeader = (BITMAPFILEHEADER *) image_buf;
	if (fileHeader->bfType != 0x4D42) { // Check if 'BM'
		return NULL;
	}

	const BITMAPINFOHEADER *infoHeader = (BITMAPINFOHEADER *)((BYTE *) image_buf + sizeof(BITMAPFILEHEADER));
	const void *pixelData = (BYTE *) image_buf + fileHeader->bfOffBits;

	// Create the DIB Bitmap
	HBITMAP hbitmap = CreateDIBitmap(
			hdc,
			infoHeader,                // Bitmap information
			CBM_INIT,                  // Initialize bitmap with data
			pixelData,                 // Pointer to the actual pixel data
			(BITMAPINFO *)infoHeader,  // Pointer to bitmap information
			DIB_RGB_COLORS             // RGB color format
	);

	free(image_buf);

	return hbitmap;
}

LRESULT CALLBACK SaverWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	static HBITMAP hBitmap = {0};
	HDC hdc = NULL;
	PAINTSTRUCT ps = {0};
	BITMAP bitmap = {0};
	HDC hdc_mem = NULL;
	HGDIOBJ oldBitmap = NULL;

	switch (msg) {
		case WM_CREATE: {
			ss.hwnd = hwnd;
			GetCursorPos(&ss.InitCursorPos);
			ss.InitTime = GetTickCount();

			ss.idTimer = SetTimer(hwnd, 0, 33, NULL); // 1 s / 30 FPS = 33ms

			return 0;
		}
		case WM_TIMER: {
			image_num_GL++;
			if (image_num_GL > 79) {
				image_num_GL = 0;
			}

			InvalidateRect(hwnd, NULL, FALSE);

			return 0;
		}
		case WM_PAINT: {
			hdc = BeginPaint(hwnd, &ps);
			if (hdc == NULL) {
				return 0;
			}

			hdc_mem = CreateCompatibleDC(hdc);
			if (hdc_mem == NULL) {
				return 0;
			}

			hBitmap = images_GL[image_num_GL];
			if (hBitmap == NULL) {
				hBitmap = getImage(image_num_GL, hdc);
				images_GL[image_num_GL] = hBitmap;
			}

			if (hBitmap == NULL) {
				return 0;
			}

			oldBitmap = SelectObject(hdc_mem, hBitmap);
			if (oldBitmap == NULL) {
				return 0;
			}

			if (GetObject(hBitmap, sizeof(bitmap), &bitmap) == 0) {
				return 0;
			}

			RECT rect;
			if (!GetWindowRect(hwnd, &rect)) {
				return 0;
			}
			int window_width = rect.right - rect.left;
			int window_height = rect.bottom - rect.top;

			double aspect_ratio = (double) bitmap.bmWidth / bitmap.bmHeight;
			int image_width = (int) (window_height * aspect_ratio);
			int x = window_width / 2 - image_width / 2;
			if (!StretchBlt(hdc, x, 0, image_width, window_height, hdc_mem, 0, 0, bitmap.bmWidth, bitmap.bmHeight, SRCCOPY)) {
				return 0;
			}

			if (SelectObject(hdc_mem, oldBitmap) == NULL) {
				return 0;
			}
			if (!DeleteDC(hdc_mem)) {
				return 0;
			}

			if (!EndPaint(hwnd, &ps)) {
				return 0;
			}

			return 0;
		}
		/*case WM_ACTIVATE:
		case WM_ACTIVATEAPP:
		case WM_NCACTIVATE: {
			if (scr_mode_GL == MODE_SAVER && !ss.IsDialogActive && LOWORD(wParam) == WA_INACTIVE) {
				CloseSaverWindow();
			}

			return 0;
		}*/
		case WM_SETCURSOR: {
			if (scr_mode_GL == MODE_SAVER && !ss.IsDialogActive) {
				SetCursor(NULL);
			} else {
				SetCursor(LoadCursor(NULL, IDC_ARROW));
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
				BOOL CanClose = TRUE;
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

BOOL CALLBACK MonitorEnumProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData) {
	MONITORINFO info;
	info.cbSize = sizeof(MONITORINFO);
	GetMonitorInfo(hMonitor, &info);

	struct MonitorInfo *monitor_info = &monitors_GL[num_monitors_GL];
	monitor_info->x = info.rcMonitor.left;
	monitor_info->y = info.rcMonitor.top;
	monitor_info->width = info.rcMonitor.right - info.rcMonitor.left;
	monitor_info->height = info.rcMonitor.bottom - info.rcMonitor.top;

	num_monitors_GL++;

	if (num_monitors_GL >= MAX_MONITORS_EDW590) {
		return FALSE;
	}

	return TRUE;
}

void DoSaver(HWND hparwnd) {
	WNDCLASS wnd_class = {0};
	wnd_class.style = CS_HREDRAW | CS_VREDRAW;
	wnd_class.lpfnWndProc = SaverWindowProc;
	wnd_class.cbClsExtra = 0;
	wnd_class.cbWndExtra = 0;
	wnd_class.hInstance = hInstance_GL;
	wnd_class.hIcon = NULL;
	wnd_class.hCursor = NULL;
	wnd_class.hbrBackground = (HBRUSH) GetStockObject(BLACK_BRUSH);
	wnd_class.lpszMenuName = NULL;
	wnd_class.lpszClassName = "ScrClass";

	if (RegisterClass(&wnd_class) == 0) {
		return;
	}

	HWND hScrWindow = NULL;
	if (scr_mode_GL == MODE_PREVIEW) {
		RECT rc;
		GetWindowRect(hparwnd, &rc);
		int cx = rc.right - rc.left;
		int cy = rc.bottom - rc.top;
		hScrWindow = CreateWindowExA(0, "ScrClass", "Edw590", WS_CHILD | WS_VISIBLE, 0, 0, cx, cy, hparwnd, NULL,
		                             hInstance_GL, NULL);
	} else {
		EnumDisplayMonitors(NULL, NULL, MonitorEnumProc, 0);
		for (int i = 0; i < num_monitors_GL; i++) {
			struct MonitorInfo *monitor_info = &monitors_GL[i];
			hScrWindow = CreateWindowExA(WS_EX_TOPMOST,
			                             "ScrClass",
			                             "Edw590",
										 WS_POPUP | WS_VISIBLE,
										 monitor_info->x,
										 monitor_info->y,
										 monitor_info->width,
										 monitor_info->height,
										 NULL,
										 NULL,
										 hInstance_GL,
										 NULL);
		}
	}

	if (hScrWindow == NULL) {
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

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd) {
	ss.MouseThreshold = 4;

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
	} else {
		if (*c == '-' || *c == '/') {
			c++;
		}
		if (*c == 'p' || *c == 'P' || *c == 'l' || *c == 'L') {
			c++;
			while (*c == ' ' || *c == ':') {
				c++;
			}
			hwnd = (HWND) atoi(c);
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

			MessageBoxPrintf("Edw590 Screensaver", "This screensaver does not support configuration.");
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
