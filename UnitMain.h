// ---------------------------------------------------------------------------

#ifndef UnitMainH
#define UnitMainH
// ---------------------------------------------------------------------------
#include <System.Classes.hpp>
#include <Vcl.Controls.hpp>
#include <Vcl.StdCtrls.hpp>
#include <Vcl.Forms.hpp>
#include <Vcl.ExtCtrls.hpp>
#include <Vcl.Menus.hpp>

// ---------------------------------------------------------------------------
class TFormMain : public TForm
{
__published: // IDE-managed Components
	TTrayIcon *TrayIcon1;
	TTimer *Timer1;
	TPopupMenu *PopupMenu1;
	TMenuItem *Exit1;
	TMenuItem *utorrentmonitor1;
	TMenuItem *N1;
	TMenuItem *Closemenu1;
	TMenuItem *TerminateUtorrentProcess1;

	void __fastcall Timer1Timer(TObject *Sender);
	void __fastcall Exit1Click(TObject *Sender);
	void __fastcall TerminateUtorrentProcess1Click(TObject *Sender);

private: // User declarations
	void Save();
	void Load();

public: // User declarations
	__fastcall TFormMain(TComponent* Owner);
};

// ---------------------------------------------------------------------------
extern PACKAGE TFormMain *FormMain;
// ---------------------------------------------------------------------------
#endif
