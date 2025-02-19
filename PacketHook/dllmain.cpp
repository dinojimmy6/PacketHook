#include "PacketHook.h"
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#define IDM_OPEN_FILTER 44
HWND hWndLog, hMaple;
HWND hStartButton, hStopButton, hClearButton, hApplyFilterButton;
HWND hWndEditIn, hWndEditOut, hWndPacketExpansion;
BOOL scrolling;
int wWidth = 1600;
int wHeight = 600;
int vSpace = 25;
int page = wHeight / vSpace;
int last_packets_in_size = 0;
list<vector<char>*>* selected;

typedef HWND(WINAPI* GetHMaple)();
int OpenFilterWindow(HWND);
int OpenPacketWindow(HWND);
void OutputFilter(HWND, HWND);
void SaveFilter(HWND, HWND);
void InitFilters();

void WINAPI GetWndClass(WNDCLASSEX& wcex, LPCWSTR name, HMODULE hModule, WNDPROC proc) {
	wcex.cbClsExtra = 0;
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.cbWndExtra = 0;
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
	wcex.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wcex.hIconSm = LoadIcon(NULL, IDI_APPLICATION);
	wcex.hInstance = hModule;
	wcex.lpfnWndProc = proc;
	wcex.lpszClassName = name;
	wcex.lpszMenuName = NULL;
	wcex.style = CS_HREDRAW | CS_VREDRAW;
}

void AddMenus(HWND hwnd) {

	HMENU hMenubar;
	HMENU hMenu;

	hMenubar = CreateMenu();
	hMenu = CreateMenu();

	AppendMenuW(hMenu, MF_STRING, IDM_OPEN_FILTER, L"&Filter");
	AppendMenuW(hMenubar, MF_POPUP, (UINT_PTR)hMenu, L"&Edit");
	SetMenu(hwnd, hMenubar);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_CREATE:
		AddMenus(hWnd);
		break;
	case WM_DESTROY:
		PostQuitMessage(EXIT_SUCCESS);
		break;
	case WM_SIZE:
		InvalidateRect(hWnd, NULL, TRUE);
		UpdateWindow(hWnd);
		break;
	case WM_COMMAND:
		switch (wParam) {
		case BN_CLICKED: {
			if ((HWND) lParam == hStartButton) {
				logging = TRUE;
			}
			else if ((HWND)lParam == hStopButton) {
				logging = FALSE;
			}
			else if ((HWND)lParam == hClearButton) {
				ClearPackets();
				InvalidateRect(hWnd, NULL, TRUE);
				UpdateWindow(hWnd);
			}
			break;
		}
		case IDM_OPEN_FILTER: {
			CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)OpenFilterWindow, (LPVOID)hWnd, 0, NULL);
			break;
		}
		
		default:
			return DefWindowProc(hWnd, msg, wParam, lParam);
		}
	default:
		return DefWindowProc(hWnd, msg, wParam, lParam);
	}
	return FALSE;
}

LRESULT CALLBACK ScrollableLog(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam, int page, int vSpace)
{
	SCROLLINFO si;
	static int yPos;

	switch (msg)
	{
	case WM_SIZE:
		InvalidateRect(hWnd, NULL, TRUE);
		UpdateWindow(hWnd);
		return 0;

	case WM_VSCROLL:
		// Get all the vertial scroll bar information.
		si.cbSize = sizeof(si);
		si.fMask = SIF_ALL;
		GetScrollInfo(hWnd, SB_VERT, &si);

		// Save the position for comparison later on.
		yPos = si.nPos;
		switch (LOWORD(wParam))
		{

			// User clicked the HOME keyboard key.
		case SB_TOP:
			si.nPos = si.nMin;
			break;

			// User clicked the END keyboard key.
		case SB_BOTTOM:
			si.nPos = si.nMax;
			break;

			// User clicked the top arrow.
		case SB_LINEUP:
			si.nPos -= 1;
			break;

			// User clicked the bottom arrow.
		case SB_LINEDOWN:
			si.nPos += 1;
			break;

			// User clicked the scroll bar shaft above the scroll box.
		case SB_PAGEUP:
			si.nPos -= si.nPage;
			break;

			// User clicked the scroll bar shaft below the scroll box.
		case SB_PAGEDOWN:
			si.nPos += si.nPage;
			break;

			// User dragged the scroll box.
		case SB_THUMBTRACK:

			si.nPos = si.nTrackPos;
			break;

		case SB_ENDSCROLL:
			scrolling = FALSE;
			break;

		default:
			break;
		}

		// Set the position and then retrieve it.  Due to adjustments
		// by Windows it may not be the same as the value set.
		si.fMask = SIF_POS;
		SetScrollInfo(hWnd, SB_VERT, &si, TRUE);
		GetScrollInfo(hWnd, SB_VERT, &si);

		// If the position has changed, scroll window and update it.
		if (si.nPos != yPos)
		{
			ScrollWindow(hWnd, 0, vSpace * (yPos - si.nPos), NULL, NULL);
			UpdateWindow(hWnd);
		}
		return 0;
		return FALSE;
	}
}

LRESULT CALLBACK TextWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	HDC hdc;
	PAINTSTRUCT ps;
	SCROLLINFO si;
	static int yPos;
	static int xPos;
	static int firstLine;

	switch (msg)
	{
	case WM_LBUTTONDBLCLK: {
		lock_guard<mutex> lock(packets_mutex);
		int yClickPos = HIWORD(lParam);
		selected = *(packets.begin() + firstLine + yClickPos / vSpace);
		CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)OpenPacketWindow, (LPVOID)hWnd, 0, NULL);
		return 0;
	}
	case WM_SIZE:
	case WM_VSCROLL:
		return ScrollableLog(hWnd, msg, wParam, lParam, page, vSpace);
	case WM_PAINT: {
		hdc = BeginPaint(hWnd, &ps);
		si.cbSize = sizeof(si);
		si.fMask = SIF_ALL;
		GetScrollInfo(hWnd, SB_VERT, &si);
		firstLine = max(0, si.nPos);
		int lastLine = min(si.nMax, si.nPos + ps.rcPaint.bottom / vSpace);
		lastLine = lastLine > packets.size() ? packets.size() : lastLine;
		lock_guard<mutex> lock(packets_mutex);
		if (packets.size() == 0) {
			EndPaint(hWnd, &ps);
			return -1;
		}
		vector<list<vector<char>*>*>::iterator packets_iter = packets.begin() + firstLine;
		SetTextAlign(hdc, TA_TOP | TA_LEFT);
		int x = 0;
		int y = 0;
		SIZE size;
		wchar_t buff[20000];
		for (; packets_iter < packets.end() && packets_iter <= packets.begin() + lastLine; ++packets_iter) {
			list<vector<char>*>* packet = *packets_iter;
			if (packet->size() > 0) {
				SetTextColor(hdc, 0);
				if (*((*(packet->begin()))->begin())) {
					TextOut(hdc, x, y, L"Out: ", 5);
				}
				else {
					TextOut(hdc, x, y, L"In: ", 4);
				}
				GetTextExtentPoint32(hdc, L"Out: ", 5, &size);
				x += size.cx;
				list<vector<char>*>::iterator it = ++packet->begin();
				for (; it != packet->end(); ++it) {
					wchar_t *buffptr = buff;
					vector<char>::iterator it2;
					SetTextColor(hdc, 0);
					TextOut(hdc, x, y, L"[ ", 2);
					GetTextExtentPoint32(hdc, L"[ ", 2, &size);
					x += size.cx;
					int mode = *((*it)->begin());
					for (it2 = (*it)->begin() + 1; it2 != (*it)->end(); ++it2) {
						int forward = swprintf_s(buffptr, 100, L"%x ", (*it2) & 0xFF);
						buffptr += forward;
					}
					switch (mode) {
					case CODE_INT:
						if ((*it)->size() == 5) {
							SetTextColor(hdc, 0xFF0000);
						}
						else if ((*it)->size() == 3) {
							SetTextColor(hdc, 0xFF00);
						}
						else if ((*it)->size() == 2) {
							SetTextColor(hdc, 0xFF);
						}
						else if ((*it)->size() == 9) {
							SetTextColor(hdc, 0xFF00FF);
						}
						break;
					case CODE_STR:
						SetTextColor(hdc, 0xFFFF00);
						break;
					case CODE_BUFFER:
						SetTextColor(hdc, 0x00FFFF);
						break;
					}
					TextOut(hdc, x, y, buff, buffptr - buff);
					GetTextExtentPoint32(hdc, buff, buffptr - buff, &size);
					x += size.cx;
					SetTextColor(hdc, 0);
					TextOut(hdc, x, y, L"] ", 2);
					GetTextExtentPoint32(hdc, L"] ", 2, &size);
					x += size.cx;
				}
				y += vSpace;
				x = 0;
			}
		}
		EndPaint(hWnd, &ps);
		break;
	}
	default:
		return DefWindowProc(hWnd, msg, wParam, lParam);
	}
	return FALSE;
}

LRESULT CALLBACK PacketWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	HDC hdc;
	PAINTSTRUCT ps;
	SCROLLINFO si;
	static int yPos;

	switch (msg)
	{
	case WM_CREATE:
		SCROLLINFO si;
		si.cbSize = sizeof(si);
		si.fMask = SIF_RANGE | SIF_PAGE;
		si.nMin = 0;
		si.nMax = selected->size() <= page ? page - 1 : selected->size() - 1;
		si.nPage = page;
		SetScrollInfo(hWnd, SB_VERT, &si, TRUE);
		return 0;
	case WM_SIZE:
	case WM_VSCROLL:
		return ScrollableLog(hWnd, msg, wParam, lParam, page, vSpace);
	case WM_PAINT: {
		hdc = BeginPaint(hWnd, &ps);
		si.cbSize = sizeof(si);
		si.fMask = SIF_ALL;
		GetScrollInfo(hWnd, SB_VERT, &si);
		int firstLine = max(0, si.nPos);
		int lastLine = min(si.nMax, si.nPos + ps.rcPaint.bottom / vSpace);
		lastLine = lastLine > packets.size() ? packets.size() : lastLine;
		lock_guard<mutex> lock(packets_mutex);
		vector<vector<char>*> packet{ std::begin(*selected), std::end(*selected) };
		SetTextAlign(hdc, TA_TOP | TA_LEFT);
		int x = 0;
		int y = 0;
		SIZE size;
		wchar_t buff[20000];
		if (packet.size() > 0) {
			vector<vector<char>*>::iterator it = packet.begin() + firstLine + 1;
			for (; it != packet.end(); ++it) {
				wchar_t *buffptr = buff;
				vector<char>::iterator it2;
				SetTextColor(hdc, 0);
				TextOut(hdc, x, y, L"[ ", 2);
				GetTextExtentPoint32(hdc, L"[ ", 2, &size);
				x += size.cx;
				int mode = *((*it)->begin());
				for (it2 = (*it)->begin() + 1; it2 != (*it)->end(); ++it2) {
					int forward = swprintf_s(buffptr, 100, L"%x ", (*it2) & 0xFF);
					buffptr += forward;
				}
				switch (mode) {
				case CODE_INT:
					if ((*it)->size() == 5) {
						SetTextColor(hdc, 0xFF0000);
					}
					else if ((*it)->size() == 3) {
						SetTextColor(hdc, 0xFF00);
					}
					else if ((*it)->size() == 2) {
						SetTextColor(hdc, 0xFF);
					}
					else if ((*it)->size() == 9) {
						SetTextColor(hdc, 0xFF00FF);
					}
					break;
				case CODE_STR:
					SetTextColor(hdc, 0xFFFF00);
					break;
				case CODE_BUFFER:
					SetTextColor(hdc, 0x00FFFF);
					break;
				}
				TextOut(hdc, x, y, buff, buffptr - buff);
				GetTextExtentPoint32(hdc, buff, buffptr - buff, &size);
				x += size.cx;
				SetTextColor(hdc, 0);
				TextOut(hdc, x, y, L"]", 1);
				GetTextExtentPoint32(hdc, L"]", 1, &size);
				y += vSpace;
				x = 0;
			}
		}
		EndPaint(hWnd, &ps);
		break;
	}
	default:
		return DefWindowProc(hWnd, msg, wParam, lParam);
	}
	return FALSE;
}

void WINAPI UpdateLog(HWND hwnd) {
	while (1) {
		int newSize = packets.size();
		if (last_packets_in_size != newSize) {
			SCROLLINFO si;
			si.cbSize = sizeof(si);
			si.fMask = SIF_RANGE | SIF_PAGE;
			si.nMin = 0;
			si.nMax = packets.size() <= page ? page - 1 : packets.size() - 1;
			si.nPage = page;
			SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
			InvalidateRect(hwnd, NULL, TRUE);
			UpdateWindow(hwnd);
		}
		last_packets_in_size = newSize;
		Sleep(500);
	}
}

void UpdateWindowPos(HWND hWnd) {
	while (1) {
		RECT rect;
		GetWindowRect(hMaple, &rect);
		MoveWindow(hWnd, rect.left, rect.bottom, 1700, 900, FALSE);
		Sleep(250);
	}
}

LRESULT CALLBACK FilterWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_CREATE: {
		hWndEditIn = CreateWindowEx(WS_EX_CLIENTEDGE, L"Edit", L"FilterIn",
			ES_LEFT | ES_MULTILINE | WS_CHILD | WS_VISIBLE | WS_VSCROLL, 10, 10, 180,
			200, hWnd, NULL, NULL, NULL);
		hWndEditOut = CreateWindowEx(WS_EX_CLIENTEDGE, L"Edit", L"FilterOut",
			ES_LEFT | ES_MULTILINE | WS_CHILD | WS_VISIBLE | WS_VSCROLL, 10, 220, 180,
			200, hWnd, NULL, NULL, NULL);

		hApplyFilterButton = CreateWindowEx(WS_EX_CLIENTEDGE, L"BUTTON", L"Apply", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
			10, 425, 60, 30, hWnd, NULL, (HINSTANCE)GetWindowLong(hWnd, GWL_HINSTANCE), NULL);
		OutputFilter(hWndEditIn, hWndEditOut);
		break;
	}
	case WM_DESTROY:
		PostQuitMessage(EXIT_SUCCESS);
		break;
	case WM_COMMAND:
		switch (wParam) {
		case BN_CLICKED: {
			if ((HWND)lParam == hApplyFilterButton) {
				SaveFilter(hWndEditIn, hWndEditOut);
			}
			break;
		}
		default:
			return DefWindowProc(hWnd, msg, wParam, lParam);
		}
	default:
		return DefWindowProc(hWnd, msg, wParam, lParam);
	}
	return FALSE;
}

int OpenAdjacentWindow(HWND hParent, LPCWSTR wcex, LPCWSTR name, int width, int height) {
	RECT rect;
	GetWindowRect(hParent, &rect);
	HWND hWnd = CreateWindow(wcex, name, WS_OVERLAPPEDWINDOW | WS_VSCROLL,
		rect.right, rect.top, width, height, NULL, NULL, NULL, NULL);
	ShowWindow(hWnd, 1);
	UpdateWindow(hWnd);

	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return EXIT_SUCCESS;
}

int OpenFilterWindow(HWND hParent) {
	return OpenAdjacentWindow(hParent, L"FiltersClass", L"Filters", 200, 600);
}

int OpenPacketWindow(HWND hParent) {
	return OpenAdjacentWindow(hParent, L"PacketClass", L"Packet", 600, 600);
}

void OutputFilter(HWND hWndEditIn, HWND hWndEditOut) {
	map<int, BOOL>::iterator it = blacklist_in.begin();
	char buff[10000];
	char* buffptr = buff;
	for (; it != blacklist_in.end(); ++it) {
		int forward = sprintf_s(buffptr, 100, "0x%x\r\n", (*it).first);
		buffptr += forward;
	}
	SetWindowTextA(hWndEditIn, buff);
	char buff1[10000];
	buffptr = buff1;
	it = blacklist_out.begin();
	for (; it != blacklist_out.end(); ++it) {
		int forward = sprintf_s(buffptr, 100, "0x%x\r\n", (*it).first);
		buffptr += forward;
	}
	SetWindowTextA(hWndEditOut, buff1);
}

void SaveFilter(HWND hWndEditIn, HWND hWndEditOut) {
	char buff[10000];
	char to_file_in[10000];
	char* outptr = to_file_in;
	GetWindowTextA(hWndEditIn, buff, 10000);
	stringstream stream_in((string(buff)));
	char line[50];
	while (stream_in.getline(line, 50)) {
		try {
			int forward = sprintf_s(outptr, 100, "0x%x\n", stoi(string(line), NULL, 16));
			outptr += forward;
		}
		catch (invalid_argument) {
			wchar_t buff[100];
			swprintf_s(buff, L"INVALID FILTER TEXT");
			MessageBox(NULL, buff, buff, MB_ICONERROR);
			return;
		}
	}
	char to_file_out[10000];
	outptr = to_file_out;
	GetWindowTextA(hWndEditOut, buff, 10000);
	stringstream stream_out((string(buff)));
	while (stream_out.getline(line, 50)) {
		try {
			int forward = sprintf_s(outptr, 100, "0x%x\n", stoi(string(line), NULL, 16));
			outptr += forward;
		}
		catch (invalid_argument) {
			wchar_t buff[100];
			swprintf_s(buff, L"INVALID FILTER TEXT");
			MessageBox(NULL, buff, buff, MB_ICONERROR);
			return;
		}
	}
	ofstream outfile;
	outfile.open("loader\\PacketHook.config", ofstream::out | ofstream::trunc);
	outfile << "IN\n";
	outfile << to_file_in;
	outfile << "OUT\n";
	outfile << to_file_out;
	outfile.close();
	InitFilters();
}

void InitFilters() {
	blacklist_in.clear();
	blacklist_out.clear();
	ifstream in_file;
	in_file.open("loader\\PacketHook.config", std::ifstream::in);
	int n = 0;
	string line;
	getline(in_file, line);
	if (line.compare("IN")) {
		wchar_t buff[100];
		swprintf_s(buff, L"BAD CONFIG FILE %s", line);
		MessageBox(NULL, buff, buff, MB_ICONERROR);
	}
	while (getline(in_file, line)) {
		istringstream iss(line);
		if (!line.compare("OUT")) {
			break;
		}
		iss >> hex >> n;
		blacklist_in.insert({ n, 1 });
		wchar_t buff[100];
		swprintf_s(buff, L"read in %d", n);
		//MessageBox(NULL, buff, buff, MB_ICONERROR);
	}
	while (in_file >> hex >> n) {
		blacklist_out.insert({ n, 1 });
		wchar_t buff[100];
		swprintf_s(buff, L"read out %d", n);
		//MessageBox(NULL, buff, buff, MB_ICONERROR);
	}
}

BOOL WINAPI OpenWindow(HMODULE hModule) {
	InitFilters();
	LPCTSTR windowClass = TEXT("WinApp");
	LPCTSTR windowTitle = TEXT("Windows Application");
	WNDCLASSEX wcex;
	GetWndClass(wcex, L"WinApp", hModule, WndProc);

	WNDCLASSEX wcex1;
	GetWndClass(wcex1, L"TextArea", hModule, TextWndProc);
	wcex1.style |= CS_DBLCLKS;

	WNDCLASSEX wcex2;
	GetWndClass(wcex2, L"FiltersClass", hModule, FilterWndProc);

	WNDCLASSEX packet_class;
	GetWndClass(packet_class, L"PacketClass", hModule, PacketWndProc);

	if (!RegisterClassEx(&wcex))
	{
		MessageBox(NULL, TEXT("RegisterClassEx Failed!"), TEXT("Error"),
			MB_ICONERROR);
		return EXIT_FAILURE;
	}
	if (!RegisterClassEx(&wcex1))
	{
		MessageBox(NULL, TEXT("RegisterClassEx Failed!"), TEXT("Error"),
			MB_ICONERROR);
		return EXIT_FAILURE;
	}
	if (!RegisterClassEx(&wcex2))
	{
		MessageBox(NULL, TEXT("RegisterClassEx Failed!"), TEXT("Error"),
			MB_ICONERROR);
		return EXIT_FAILURE;
	}
	if (!RegisterClassEx(&packet_class))
	{
		MessageBox(NULL, TEXT("RegisterClassEx Failed!"), TEXT("Error"),
			MB_ICONERROR);
		return EXIT_FAILURE;
	}

	LPVOID lpGetHMaple = (LPVOID)GetProcAddress(GetModuleHandle(L"dinput8.dll"), "GetHMaple");
	
	hMaple = ((GetHMaple)lpGetHMaple)();
	while (1) {
		Sleep(500);
		hMaple = ((GetHMaple)lpGetHMaple)();
		wchar_t buff[100];
		GetClassName(hMaple, buff, 100);
		if (!wcscmp(buff, L"MapleStoryClass")) {
			break;
		}
	}
	RECT rect;
	GetWindowRect(hMaple, &rect);
	HWND hWnd;

	if (!(hWnd = CreateWindow(windowClass, windowTitle, WS_OVERLAPPEDWINDOW,
		rect.left, rect.bottom, 1700,
		900, NULL, NULL, hModule, NULL)))
	{
		MessageBox(NULL, TEXT("CreateWindow Failed!"), TEXT("Error"), MB_ICONERROR);
		return EXIT_FAILURE;
	}
	
	hWndLog = CreateWindowEx(WS_EX_CLIENTEDGE, TEXT("TextArea"), NULL,
		ES_LEFT | ES_MULTILINE | ES_READONLY | WS_CHILD | WS_VISIBLE | WS_VSCROLL, 10, 40, wWidth,
		wHeight, hWnd, NULL, NULL, NULL);

	CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)UpdateLog, (LPVOID)hWndLog, 0, NULL);
	CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)UpdateWindowPos, (LPVOID)hWnd, 0, NULL);

	hStartButton = CreateWindowEx(WS_EX_CLIENTEDGE, L"BUTTON", L"Start", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
								  10, 50 + wHeight, 60, 30, hWnd, NULL, (HINSTANCE)GetWindowLong(hWnd, GWL_HINSTANCE), NULL);

	hStopButton = CreateWindowEx(WS_EX_CLIENTEDGE, L"BUTTON", L"Stop", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
								 80, 50 + wHeight, 60, 30, hWnd, NULL, (HINSTANCE)GetWindowLong(hWnd, GWL_HINSTANCE), NULL);

	hClearButton = CreateWindowEx(WS_EX_CLIENTEDGE, L"BUTTON", L"Clear", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
								  150, 50 + wHeight, 60, 30, hWnd, NULL, (HINSTANCE)GetWindowLong(hWnd, GWL_HINSTANCE), NULL);

	ShowWindow(hWnd, 1);
	UpdateWindow(hWnd);

	MSG msg;

	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return EXIT_SUCCESS;
}

DWORD WINAPI Hook(HWND handle) {
	Sleep(5000);
	if (!DetourFunction(TRUE, (PVOID*)&_Decode1, Decode1_Detour)) {
		MessageBox(NULL, L"DECODE1 DETOUR FAILED", L"DECODE1 DETOUR FAILED", MB_OK | MB_ICONINFORMATION);
	}
	if (!DetourFunction(TRUE, (PVOID*)&_Decode2, Decode2_Detour)) {
		MessageBox(NULL, L"DECODE2 DETOUR FAILED", L"DECODE2 DETOUR FAILED", MB_OK | MB_ICONINFORMATION);
	}
	if (!DetourFunction(TRUE, (PVOID*)&_Decode4, Decode4_Detour)) {
		MessageBox(NULL, L"DECODE4 DETOUR FAILED", L"DECODE4 DETOUR FAILED", MB_OK | MB_ICONINFORMATION);
	}
	if (!DetourFunction(TRUE, (PVOID*)&_Decode8, Decode8_Detour)) {
		MessageBox(NULL, L"DECODE8 DETOUR FAILED", L"DECODE8 DETOUR FAILED", MB_OK | MB_ICONINFORMATION);
	}
	if (!DetourFunction(TRUE, (PVOID*)&_DecodeStr, DecodeStr_Detour)) {
		MessageBox(NULL, L"DECODESTR DETOUR FAILED", L"DECODESTR DETOUR FAILED", MB_OK | MB_ICONINFORMATION);
	}
	if (!DetourFunction(TRUE, (PVOID*)&_DecodeBuffer, DecodeBuffer_Detour)) {
		MessageBox(NULL, L"DECODEBUFFER DETOUR FAILED", L"DECODEBUFFER DETOUR FAILED", MB_OK | MB_ICONINFORMATION);
	}
	if (!DetourFunction(TRUE, (PVOID*)&_CInPacket, CInPacket_Detour)) {
		MessageBox(NULL, L"CINPACKET DETOUR FAILED", L"CINPACKET DETOUR FAILED", MB_OK | MB_ICONINFORMATION);
	}
	if (!DetourFunction(TRUE, (PVOID*)&_Encode1, Encode1_Detour)) {
		MessageBox(NULL, L"ENCODE1 DETOUR FAILED", L"ENCODE1 DETOUR FAILED", MB_OK | MB_ICONINFORMATION);
	}
	if (!DetourFunction(TRUE, (PVOID*)&_Encode2, Encode2_Detour)) {
		MessageBox(NULL, L"ENCODE2 DETOUR FAILED", L"ENCODE2 DETOUR FAILED", MB_OK | MB_ICONINFORMATION);
	}
	if (!DetourFunction(TRUE, (PVOID*)&_Encode4, Encode4_Detour)) {
		MessageBox(NULL, L"ENCODE4 DETOUR FAILED", L"ENCODE4 DETOUR FAILED", MB_OK | MB_ICONINFORMATION);
	}
	if (!DetourFunction(TRUE, (PVOID*)&_Encode8, Encode8_Detour)) {
		MessageBox(NULL, L"ENCODE8 DETOUR FAILED", L"ENCODE8 DETOUR FAILED", MB_OK | MB_ICONINFORMATION);
	}
	if (!DetourFunction(TRUE, (PVOID*)&_EncodeStr, EncodeStr_Detour)) {
		MessageBox(NULL, L"ENCODESTR DETOUR FAILED", L"ENCODESTR DETOUR FAILED", MB_OK | MB_ICONINFORMATION);
	}
	if (!DetourFunction(TRUE, (PVOID*)&_EncodeBuffer, EncodeBuffer_Detour)) {
		MessageBox(NULL, L"ENCODEBUFFER DETOUR FAILED", L"ENCODEBUFFER DETOUR FAILED", MB_OK | MB_ICONINFORMATION);
	}
	if (!DetourFunction(TRUE, (PVOID*)&_COutPacket, COutPacket_Detour)) {
		MessageBox(NULL, L"COUTPACKET DETOUR FAILED", L"COUTPACKET DETOUR FAILED", MB_OK | MB_ICONINFORMATION);
	}
	if (!DetourFunction(TRUE, (PVOID*)&_MemoryFree, MemoryFree_Detour)) {
		MessageBox(NULL, L"MEMORYFREE DETOUR FAILED", L"MEMORYFREE DETOUR FAILED", MB_OK | MB_ICONINFORMATION);
	}
	return 1;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved) {
	DisableThreadLibraryCalls(hModule);
	if (ul_reason_for_call == DLL_PROCESS_ATTACH)
	{
		DWORD windowId;
		CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)OpenWindow, (LPVOID)hModule, 0, &windowId);
		CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)Hook, (LPVOID)windowId, 0, NULL);
	}
	return TRUE;
}