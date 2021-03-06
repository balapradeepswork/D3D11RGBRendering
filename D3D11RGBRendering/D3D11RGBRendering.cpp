// D3D11RGBRendering.cpp : Defines the entry point for the application.
//

#include "D3D11RGBRendering.h"
#include "OutputManager.h"

#define MAX_LOADSTRING 100

char buf[1024];

UINT pitch = 0, bmpHeight = 0, bmpWidth = 0;

enum RGB
{
	RGB32,
	RGB24
};

RGB rgb = RGB24; //Change it accordingly

//
// Globals
//
OUTPUTMANAGER OutMgr;


// Global Variables:
HINSTANCE hInst;                                // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name

												// Forward declarations of functions included in this code module:
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
void WriteBitmap32ToTexture(BYTE *bitmap, RECT DeskBounds);
void WriteBitmap24ToTexture(BYTE *bitmap, RECT DeskBounds);
BYTE* ReadBitmapFromFile();

//
// Program entry point
//
int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ INT nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	INT SingleOutput = -1;


	// Window
	HWND WindowHandle = nullptr;


	// Load simple cursor
	HCURSOR Cursor = nullptr;
	Cursor = LoadCursor(nullptr, IDC_ARROW);
	if (!Cursor)
	{
		ProcessFailure(nullptr, L"Cursor load failed", L"Error", E_UNEXPECTED);
		return 0;
	}

	// Register class
	WNDCLASSEXW Wc;
	Wc.cbSize = sizeof(WNDCLASSEXW);
	Wc.style = CS_HREDRAW | CS_VREDRAW;
	Wc.lpfnWndProc = WndProc;
	Wc.cbClsExtra = 0;
	Wc.cbWndExtra = 0;
	Wc.hInstance = hInstance;
	Wc.hIcon = nullptr;
	Wc.hCursor = Cursor;
	Wc.hbrBackground = nullptr;
	Wc.lpszMenuName = nullptr;
	Wc.lpszClassName = L"ddasample";
	Wc.hIconSm = nullptr;
	if (!RegisterClassExW(&Wc))
	{
		ProcessFailure(nullptr, L"Window class registration failed", L"Error", E_UNEXPECTED);
		return 0;
	}

	// Create window
	RECT WindowRect = { 0, 0, 800, 600 };
	AdjustWindowRect(&WindowRect, WS_OVERLAPPEDWINDOW, FALSE);
	WindowHandle = CreateWindowW(L"ddasample", L"DXGI desktop duplication sample",
		WS_OVERLAPPEDWINDOW,
		0, 0,
		WindowRect.right - WindowRect.left, WindowRect.bottom - WindowRect.top,
		nullptr, nullptr, hInstance, nullptr);
	if (!WindowHandle)
	{
		ProcessFailure(nullptr, L"Window creation failed", L"Error", E_FAIL);
		return 0;
	}

	DestroyCursor(Cursor);

	ShowWindow(WindowHandle, nCmdShow);
	UpdateWindow(WindowHandle);

	RECT DeskBounds;
	UINT OutputCount;

	// Message loop (attempts to update screen when no other messages to process)
	MSG msg = { 0 };
	bool FirstTime = true;
	bool Occluded = true;

	while (WM_QUIT != msg.message)
	{
		DUPL_RETURN Ret = DUPL_RETURN_SUCCESS;
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			if (msg.message == OCCLUSION_STATUS_MSG)
			{
				// Present may not be occluded now so try again
				Occluded = false;
			}
			else
			{
				// Process window messages
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
		else if (FirstTime)
		{

			// First time through the loop so nothing to clean up
			FirstTime = false;

			// Re-initialize
			Ret = OutMgr.InitOutput(WindowHandle, &DeskBounds);

			// We start off in occluded state and we should immediate get a occlusion status window message
			Occluded = true;
		}
		else
		{
			// Nothing else to do, so try to present to write out to window if not occluded
			if (!Occluded)
			{
				BYTE *bitmap = ReadBitmapFromFile();

				(rgb == RGB32) ? WriteBitmap32ToTexture(bitmap, DeskBounds) : WriteBitmap24ToTexture(bitmap, DeskBounds);
				free(bitmap);

				Ret = OutMgr.UpdateApplicationWindow(&Occluded);
			}
		}
	}

	if (msg.message == WM_QUIT)
	{
		OutMgr.CleanRefs();
		// For a WM_QUIT message we should return the wParam value
		return static_cast<INT>(msg.wParam);
	}

	return 0;
}

BYTE* ReadBitmapFromFile()
{

	FILE *file = nullptr;


	sprintf_s(buf, (rgb == RGB32) ? "input\\rgb32.bmp" : "input\\rgb24.bmp");
	fopen_s(&file, buf, "rb");

	BITMAPFILEHEADER *bITMAPFILEHEADER = (BITMAPFILEHEADER*)malloc(sizeof(BITMAPFILEHEADER));

	BITMAPINFOHEADER *bITMAPINFOHEADER = (BITMAPINFOHEADER*)malloc(sizeof(BITMAPINFOHEADER));

	int readBytes = fread(bITMAPFILEHEADER, sizeof(BITMAPFILEHEADER), 1, file);

	readBytes = fread(bITMAPINFOHEADER, sizeof(BITMAPINFOHEADER), 1, file);

	size_t bitmapSize = bITMAPFILEHEADER->bfSize - sizeof(BITMAPFILEHEADER) - sizeof(BITMAPINFOHEADER);
	BYTE *bitmap = (BYTE*)malloc(bitmapSize);

	pitch = bitmapSize / bITMAPINFOHEADER->biHeight;
	bmpHeight = bITMAPINFOHEADER->biHeight;
	bmpWidth = bITMAPINFOHEADER->biWidth;

	readBytes = fread(bitmap, bitmapSize, 1, file);
	//memset(bitmap, 128, bitmapSize);
	fclose(file);
	return bitmap;
}

void WriteBitmap32ToTexture(BYTE *bitmap, RECT DeskBounds)
{
	// Copy image into CPU access texture
	OutMgr.m_DeviceContext->CopyResource(OutMgr.m_AccessibleSurf, OutMgr.m_SharedSurf);

	// Copy from CPU access texture to bitmap buffer
	D3D11_MAPPED_SUBRESOURCE resource;
	UINT subresource = D3D11CalcSubresource(0, 0, 0);
	OutMgr.m_DeviceContext->Map(OutMgr.m_AccessibleSurf, subresource, D3D11_MAP_WRITE, 0, &resource);


	UINT dist = 0, height = DeskBounds.bottom - DeskBounds.top, sdist = 0;

	BYTE* dptr = reinterpret_cast<BYTE*>(resource.pData);
	dptr += resource.RowPitch*(height - 1);
	int minPitch = min(pitch, resource.RowPitch);
	//memcpy_s(dptr, resource.RowPitch*height, bitmap, resource.RowPitch*height);

	for (size_t h = 0; h < height; ++h)
	{
		dist = resource.RowPitch *h;
		sdist = pitch * h;
		memcpy_s(dptr - dist, resource.RowPitch, bitmap + sdist, minPitch);
	}
	OutMgr.m_DeviceContext->Unmap(OutMgr.m_AccessibleSurf, subresource);
	OutMgr.m_DeviceContext->CopyResource(OutMgr.m_SharedSurf, OutMgr.m_AccessibleSurf);
}

void WriteBitmap24ToTexture(BYTE *bitmap, RECT DeskBounds)
{
	// Copy image into CPU access texture
	OutMgr.m_DeviceContext->CopyResource(OutMgr.m_AccessibleSurf, OutMgr.m_SharedSurf);

	// Copy from CPU access texture to bitmap buffer
	D3D11_MAPPED_SUBRESOURCE resource;
	UINT subresource = D3D11CalcSubresource(0, 0, 0);
	OutMgr.m_DeviceContext->Map(OutMgr.m_AccessibleSurf, subresource, D3D11_MAP_WRITE, 0, &resource);


	int dist = 0, height = DeskBounds.bottom - DeskBounds.top, sdist = 0;

	BYTE* dptr = reinterpret_cast<BYTE*>(resource.pData);
	dptr += resource.RowPitch*(height - 1);

	for (size_t h = 0; h < bmpHeight; ++h)
	{
		dist = resource.RowPitch *h;
		sdist = pitch * h;
		for (size_t w = 0; w < bmpWidth; w++)
		{
			dptr[-dist + w * 4] = bitmap[sdist + w * 3];
			dptr[-dist + w * 4 + 1] = bitmap[sdist + w * 3 + 1];
			dptr[-dist + w * 4 + 2] = bitmap[sdist + w * 3 + 2];
		}

	}
	OutMgr.m_DeviceContext->Unmap(OutMgr.m_AccessibleSurf, subresource);
	OutMgr.m_DeviceContext->CopyResource(OutMgr.m_SharedSurf, OutMgr.m_AccessibleSurf);
}

//
// Window message processor
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_DESTROY:
	{
		PostQuitMessage(0);
		break;
	}
	case WM_SIZE:
	{
		// Tell output manager that window size has changed
		OutMgr.WindowResize();
		break;
	}
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}

	return 0;
}
