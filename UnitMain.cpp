// ---------------------------------------------------------------------------

#include <vcl.h>
#pragma hdrstop

#include <list>
#include <psapi.h>
#include <XMLDoc.hpp>

#include "UnitMain.h"
// ---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma resource "*.dfm"
TFormMain *FormMain;

String _winTitle(L"μTorrent");
String _winClass(L"µTorrent");
String _exeName(L"utorrent.exe"); // lower case!
bool _winFound = false;
bool _firstTimer = true;

// ---------------------------------------------------------------------------
String tl_GetModuleName()
{
	wchar_t buf[MAX_PATH];
	GetModuleFileName(NULL, buf, MAX_PATH);
	return (String)buf;
}

// ---------------------------------------------------------------------------
String tl_GetProgramPath() // path with trailing '\'
{
	static String Path = tl_GetModuleName();
	return Path.SubString(1, Path.LastDelimiter(L"\\"));
}

// ---------------------------------------------------------------------------
String GetWindowClassPlus(HWND hwnd)
{
	wchar_t *tbuf = new wchar_t[2048];
	tbuf[0] = L'\0';
	GetClassName(hwnd, tbuf, 2047);
	String ret(tbuf);
	delete[]tbuf;
	return ret;
}

// ---------------------------------------------------------------------------
String GetWindowTitlePlus(HWND hwnd)
{
	wchar_t *tbuf = new wchar_t[2048];
	tbuf[0] = L'\0';
	GetWindowText(hwnd, tbuf, 2047);
	String ret(tbuf);
	delete[]tbuf;
	return ret;
}

DWORD PSAPI_EnumProcesses(std::list<DWORD>& listProcessIDs, DWORD dwMaxProcessCount)
{
	DWORD dwRet = NO_ERROR;
	listProcessIDs.clear();
	DWORD *pProcessIds = new DWORD[dwMaxProcessCount];
	DWORD cb = dwMaxProcessCount*sizeof(DWORD);
	DWORD dwBytesReturned = 0;

	// call PSAPI EnumProcesses
	if (::EnumProcesses(pProcessIds, cb, &dwBytesReturned))
	{
		// push returned process IDs into the output list
		const int nSize = dwBytesReturned / sizeof(DWORD);
		for (int nIndex = 0; nIndex < nSize; nIndex++)
		{
			listProcessIDs.push_back(pProcessIds[nIndex]);
		}
	}
	else
	{
		dwRet = ::GetLastError();
	}
	delete[]pProcessIds;
	return dwRet;
}

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
	// Save();
	Load();

	// start utorrent
	String exe;
	STARTUPINFO si;
	PROCESS_INFORMATION pi;
	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	ZeroMemory(&pi, sizeof(pi));
	exe.printf(L"%s%s", tl_GetProgramPath().w_str(), _exeName.w_str());
	if (CreateProcess(exe.w_str(), NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
	{
		CloseHandle(pi.hThread);
		CloseHandle(pi.hProcess);
	}
	else
	{
		String msg;
		msg.printf(L"CreateProcess failed. Error: %lx\n%s", GetLastError(), exe.w_str());
		throw Exception(msg);
	}

	Timer1->Interval = 5000; // give time for utorrent to show its window
	Timer1->Enabled = true;
}

// ---------------------------------------------------------------------------
void __fastcall TFormMain::Timer1Timer(TObject *Sender)
{
	Timer1->Enabled = false;
	if (_firstTimer)
	{
		_firstTimer = false;
		Timer1->Interval = 1000;
	}

	_winFound = false;
	EnumWindows(enumWindowCallback, NULL);
	if (_winFound)
	{
		if (TrayIcon1->Visible != false)
			TrayIcon1->Visible = false;
	}
	else
	{
		DWORD dwProcessID = FindExeProcess(_exeName);
		if (dwProcessID)
		{
			if (TrayIcon1->Visible != true)
				TrayIcon1->Visible = true;
		}
		else
		{
			this->Close();
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
	String str1;
	_di_IXMLDocument document = interface_cast<Xmlintf::IXMLDocument>
		(new TXMLDocument(NULL));
	document->Active = true;
	document->SetEncoding(L"UTF-8");
	document->Options = document->Options << doNodeAutoIndent;
	_di_IXMLNode root = document->CreateNode(L"Root", ntElement, L"");
	document->DocumentElement = root;

	_di_IXMLNode node1;
	node1 = document->CreateNode(L"utorrent", ntElement, L"");
	node1->Attributes[L"WindowTitle"] = _winTitle;
	node1->Attributes[L"WindowClass"] = _winClass;
	node1->Attributes[L"ExeName"] = _exeName;
	root->ChildNodes->Add(node1);

	str1.printf(L"%sutorrent-monitor.xml", tl_GetProgramPath().w_str());
	document->SaveToFile(str1);
	document->Release();
	CoUninitialize();
}

// ---------------------------------------------------------------------------
void TFormMain::Load()
{
	CoInitialize(0);
	String str1;
	str1.printf(L"%sutorrent-monitor.xml", tl_GetProgramPath().w_str());
	if (!FileExists(str1))
		throw Exception(L"File utorrent-monitor.xml does not exist, aborting.");

	const _di_IXMLDocument document = interface_cast<Xmlintf::IXMLDocument>
		(new TXMLDocument(NULL));
	document->LoadFromFile(str1);

	// find root node
	const _di_IXMLNode root = document->ChildNodes->FindNode(L"Root");
	if (root != NULL)
	{
		// traverse child nodes
		for (int i = 0; i < root->ChildNodes->Count; i++)
		{
			const _di_IXMLNode node = root->ChildNodes->Get(i);
			// get attributes
			if (node->HasAttribute(L"WindowTitle"))
			{
				_winTitle = node->Attributes[L"WindowTitle"];
			}
			if (node->HasAttribute(L"WindowClass"))
			{
				_winClass = node->Attributes[L"WindowClass"];
			}
			if (node->HasAttribute(L"ExeName"))
			{
				_exeName = node->Attributes[L"ExeName"];
				_exeName = _exeName.LowerCase();
			}
		}
	}
	document->Release();
	CoUninitialize();
}

// ---------------------------------------------------------------------------
