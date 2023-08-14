// ---------------------------------------------------------------------------

#include <vcl.h>
#pragma hdrstop

#include <list>
#include <psapi.h>
#include <XMLDoc.hpp>

#include "UnitMain.h"
#include "tools.h"

// ---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma resource "*.dfm"
TFormMain *FormMain;

const String _configFile(tl_GetProgramPath() + Application->Title + L".xml");
String _winTitle(L"μTorrent");
String _winClass(L"µTorrent");
String _exeName(L"utorrent.exe"); // lower case!
int _startupDelay = 5000; // give time for utorrent to show its window
int _pollInterval = 1000;

bool _winFound = false;
bool _firstTimer = true;
HANDLE _hUtProcess = NULL;

// ---------------------------------------------------------------------------
static BOOL CALLBACK enumWindowCallback(HWND hTarget, LPARAM lparam)
{
	if (_winTitle.Length())
	{
		String wttl(GetWindowTitlePlus(hTarget));
		if (wttl.Length() && wttl.Pos(_winTitle)); //
		else
			return TRUE;
	}
	if (_winClass.Length())
	{
		String wcls(GetWindowClassPlus(hTarget));
		if (wcls.Length() && wcls.Pos(_winClass)); //
		else
			return TRUE;
	}

	_winFound = true;
	return FALSE;
}

// ---------------------------------------------------------------------------
DWORD FindExeProcess(String exe)
{
	bool exeFound = false;
	DWORD dwProcessID = 0;
	std::list<DWORD>listProcessIDs;
	const DWORD dwMaxProcessCount = 2048;
	// get prosess IDs list
	DWORD dwRet = PSAPI_EnumProcesses(listProcessIDs, dwMaxProcessCount);
	if (dwRet != NO_ERROR)
	{
		String msg;
		msg.printf(L"PSAPI_GetProcessesList failed. Error: %lx", dwRet);
		throw Exception(msg);
	}
	// iterate list to find our exe
	const std::list<DWORD>::const_iterator end = listProcessIDs.end();
	std::list<DWORD>::const_iterator iter = listProcessIDs.begin();
	for (; iter != end; ++iter)
	{
		dwProcessID = *iter;
		HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE,
			dwProcessID);
		if (hProc)
		{
			wchar_t buffer[MAX_PATH];
			if (GetModuleFileNameEx(hProc, 0, buffer, MAX_PATH))
			{
				String fexe(buffer);
				int pos = fexe.LowerCase().Pos(exe);
				if (pos && (fexe.Length() - pos + 1 == exe.Length()))
				{
					exeFound = true;
					CloseHandle(hProc);
					break;
				}
			}
			CloseHandle(hProc);
		}
	}
	if (!exeFound)
	{
		dwProcessID = 0;
	}
	return dwProcessID;
}

// ---------------------------------------------------------------------------
__fastcall TFormMain::TFormMain(TComponent* Owner) : TForm(Owner)
{
	// TrayIcon1->Visible = true;
	Load();

	// start utorrent
	String runcmd, cmdline(GetCommandLineW()), args;
	wchar_t **argv = NULL;
	int argc = 0;
	argv = CommandLineToArgvW(cmdline.w_str(), &argc);
	args = cmdline.SubString(wcslen(argv[0]) + 3, cmdline.Length());
	runcmd.printf(L"\"%s%s\"%s", tl_GetProgramPath().w_str(), _exeName.w_str(),
		args.w_str());
	LocalFree(argv);

	STARTUPINFO si;
	PROCESS_INFORMATION pi;
	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	ZeroMemory(&pi, sizeof(pi));
	if (CreateProcess(NULL, runcmd.w_str(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
	{
		CloseHandle(pi.hThread);
		// CloseHandle(pi.hProcess);
		_hUtProcess = pi.hProcess;
	}
	else
	{
		String msg;
		msg.printf(L"CreateProcess failed. Error: %lx\n%s", GetLastError(),
			runcmd.w_str());
		throw Exception(msg);
	}

	Timer1->Interval = _startupDelay; // give time for utorrent to show its window
	Timer1->Enabled = true;
}

// ---------------------------------------------------------------------------
void __fastcall TFormMain::Timer1Timer(TObject *Sender)
{
	Timer1->Enabled = false;

	// after startup delay
	if (_firstTimer)
	{
		_firstTimer = false;
		Timer1->Interval = _pollInterval;

		// track own child ut process
		// when running multiple times by opening associated files
		DWORD exitCode;
		if (!GetExitCodeProcess(_hUtProcess, &exitCode))
		{
			String msg;
			msg.printf(L"GetExitCodeProcess failed. Error: %lx", GetLastError());
			throw Exception(msg);
		}
		if (exitCode != STILL_ACTIVE)
		{
			this->Close();
			return;
		}
	}

	// track ut window
	_winFound = false;
	EnumWindows(enumWindowCallback, NULL);
	if (_winFound)
	{
		if (TrayIcon1->Visible != false)
			TrayIcon1->Visible = false;
	}
	else
	{
		// track any ut process
		DWORD dwProcessID = FindExeProcess(_exeName);
		if (dwProcessID)
		{
			if (TrayIcon1->Visible != true)
				TrayIcon1->Visible = true;
		}
		else
		{
			this->Close();
			return;
		}

	}

	Timer1->Enabled = true;
}

// ---------------------------------------------------------------------------
void __fastcall TFormMain::Exit1Click(TObject *Sender)
{
	this->Close();
}

// ---------------------------------------------------------------------------
void __fastcall TFormMain::TerminateUtorrentProcess1Click(TObject *Sender)
{
	DWORD dwProcessID = FindExeProcess(_exeName);
	if (dwProcessID)
	{
		// close own ut process handle just in case
		if (_hUtProcess)
		{
			CloseHandle(_hUtProcess);
			_hUtProcess = NULL;
		}

		DWORD dwDesiredAccess = PROCESS_TERMINATE;
		BOOL bInheritHandle = FALSE;
		HANDLE hProcess = OpenProcess(dwDesiredAccess, bInheritHandle, dwProcessID);
		if (hProcess == NULL)
		{
			String msg;
			msg.printf(L"OpenProcess failed. Error: %lx\n%lx: %s", GetLastError(),
				dwProcessID, _exeName.w_str());
			throw Exception(msg);
		}
		if (!TerminateProcess(hProcess, 0))
		{
			String msg;
			msg.printf(L"TerminateProcess failed. Error: %lx\n%lx: %s", GetLastError(),
				dwProcessID, _exeName.w_str());
			throw Exception(msg);
		}
	}
}

// ---------------------------------------------------------------------------
void TFormMain::Save()
{
	CoInitialize(0);

	_di_IXMLDocument document = interface_cast<Xmlintf::IXMLDocument>
		(new TXMLDocument(NULL));
	document->Active = true;
	document->SetEncoding(L"UTF-8");
	document->Options = document->Options << doNodeAutoIndent;
	_di_IXMLNode root = document->CreateNode(L"Config", ntElement, L"");
	document->DocumentElement = root;

	root->Attributes[L"WindowTitle"] = _winTitle;
	root->Attributes[L"WindowClass"] = _winClass;
	root->Attributes[L"ExeName"] = _exeName;
	root->Attributes[L"StartupDelayMs"] = _startupDelay;
	root->Attributes[L"PollIntervalMs"] = _pollInterval;

	document->SaveToFile(_configFile);
	document->Release();
	CoUninitialize();
}

// ---------------------------------------------------------------------------
void TFormMain::Load()
{
	CoInitialize(0);

	if (!FileExists(_configFile))
	{
		Save();
		return;
	}

	const _di_IXMLDocument document = interface_cast<Xmlintf::IXMLDocument>
		(new TXMLDocument(NULL));
	document->LoadFromFile(_configFile);

	// find root node
	const _di_IXMLNode root = document->ChildNodes->FindNode(L"Config");
	if (root != NULL)
	{
		// get attributes
		if (root->HasAttribute(L"WindowTitle"))
			_winTitle = root->Attributes[L"WindowTitle"];

		if (root->HasAttribute(L"WindowClass"))
			_winClass = root->Attributes[L"WindowClass"];

		if (root->HasAttribute(L"ExeName"))
		{
			_exeName = root->Attributes[L"ExeName"];
			_exeName = _exeName.LowerCase();
		}

		if (root->HasAttribute(L"StartupDelayMs"))
			_startupDelay = (int)root->Attributes[L"StartupDelayMs"];

		if (root->HasAttribute(L"PollIntervalMs"))
			_pollInterval = (int)root->Attributes[L"PollIntervalMs"];
	}
	document->Release();
	CoUninitialize();
}

// ---------------------------------------------------------------------------
