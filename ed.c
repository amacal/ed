/** Editor ED - Modul ED: Editor in C und Win32-API **/

// Alle Rechte vorbehalten (c) 2009-2015, Asdala.

/** Include-Dateien **/
#include "top.h" // Compile-Scope, muss zuerst kommen
#include <windows.h>
#include <shlobj.h> // SHGetFolderPath etc. V1.1 New
#include <commctrl.h> // SB_SETPARTS etc.
#ifdef BUILD_RE
#include <richedit.h>
#endif
#ifdef __LCC__
#include "lcc.h" // Windows-Konstanten, die LLC nicht kennt
#endif
#ifdef __BORLANDC__
#include "etc.h" // Compiler-spez. Ergaenzungen
#endif
#include "res.h" // Ressourcen-Konstanten
#include "ut.h" // Utility-Funktionen
#ifdef BUILD_FR
#include "fr.h" // Suchen/Ersetzen-Funktionen
#endif
#ifdef BUILD_ST
#include "st.h" // Systemtray-Funktionen
#endif

/** Globale Variablen **/
static const TCHAR appTitle[] = TEXT("ED");
static TCHAR filePath[MAX_PATH]; // Von diversen Funktionen via ofn gesetzt
static const TCHAR fileDefExt[] = TEXT("txt"); // Standarddateierweiterung
static const TCHAR fileFilter[] = TEXT("Texte (*.txt, *.log)\0*.txt;*.log\0" "Alle\0*.*\0"); // Dateifilter
static int dirty, readOnly, printPageNo=1; // Datei veraendert, Datei schreibgeschuetzt, Drucke Seitenzahlen
#ifdef BUILD_RE
static HINSTANCE richEditLib; // RichEditbibliothek
#endif

/** Praeprozessordirektiven **/
#define MAXTEXT 200000 // Max. Textgroesse von EDIT/RICHTEXT-Control
#define MESTAT(val) ((val) ? MF_ENABLED : MF_GRAYED) // Schalte Menueeintraege ein oder aus
enum { STSTAT, STINDI, STFILE }; // Namen der Statusleistenteile statt 0, 1, 2

/** Function: Ermittele HDC des Standarddruckers **/
static HDC getDefaultPrinterDC(void)
  {
  PRINTDLG pd;
  ZeroMemory(&pd, sizeof(PRINTDLG));
  pd.lStructSize = sizeof(PRINTDLG);
  pd.Flags = PD_RETURNDEFAULT | PD_RETURNDC;
  PrintDlg(&pd);
  if (pd.hDevMode)
    GlobalFree(pd.hDevMode); // Falls beim Aufruf von PrintDlg hDevMode und hDevNames NULL sind, alloziert OS
  if (pd.hDevNames) // Speicher und fuellt Strukturen mit Druckerwerten; daher sollten wir Speicher wieder freigeben.
    GlobalFree(pd.hDevNames);
  return pd.hDC;
  }

/** Function: Menue Waehle Schrift aus **/
static BOOL menuOptionsFont(HWND hWnd, LOGFONT *lf, DWORD cfFlags)
  {
  CHOOSEFONT cf;
  BOOL result;
  ZeroMemory(&cf, sizeof(CHOOSEFONT));
  cf.lStructSize = sizeof(CHOOSEFONT);
  cf.hwndOwner = hWnd;
  if (cfFlags & CF_PRINTERFONTS) // menuOptionsFont() fuer Druckschrift aufgerufen
    if ((cf.hDC=getDefaultPrinterDC()) == NULL) // Somit nur Schriften des Standarddruckers verfuegbar
      return FALSE;
  cf.lpLogFont = lf; // Druck- oder Screen-Logfont
  cf.Flags = CF_INITTOLOGFONTSTRUCT | cfFlags; // Kein CF_EFFECTS, da Underlined etc. unnoetig
  result = ChooseFont(&cf); // Mit CF_INITTOLOGFONTSTRUCT befuellt ChooseFont indirekt auch logFont
  if (cf.hDC)
    DeleteDC(cf.hDC);
  return result;
  }

/** Function: Lade ANSI-Datei ins EDIT-Fenster **/
static BOOL fileLoad(HWND hWnd, TCHAR fileName[])
  {
  HANDLE hFile;
  BOOL ok = FALSE;
  DWORD fileSize, read;
  char *buf;

  hFile = CreateFile(fileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
  if (hFile != INVALID_HANDLE_VALUE) {
    fileSize = GetFileSize(hFile, NULL);
    if (fileSize != 0xFFFFFFFF) {
      if ((buf = (char*)GlobalAlloc(GPTR, fileSize + 1)) != NULL) {
        if (ReadFile(hFile, buf, fileSize, &read, NULL)) { // Es gibt keine *A- bzw. *W-Version
          buf[fileSize] = '\0'; // [ANSIONLY]; Nullen im Text interpretiert EDIT-Control als Textende
          ok = SetWindowTextA(hWnd, buf); // [ANSIONLY]
          }
        GlobalFree(buf);
        }
      }
    CloseHandle(hFile);
    }
  return ok;
  }

/** Function: Schreibe EDIT-Fenster in ANSI-Datei **/
static BOOL fileSave(HWND hWnd, TCHAR fileName[])
  {
  HANDLE hFile;
  BOOL ok = FALSE;
  DWORD textLen, bufSize, written;
  char *buf;

  hFile = CreateFile(fileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  if (hFile != INVALID_HANDLE_VALUE) {
    if ((textLen=GetWindowTextLength(hWnd)) > 0) {
      bufSize = textLen + 1;
      if ((buf =(char*)GlobalAlloc(GPTR, bufSize)) != NULL) {
        if (GetWindowTextA(hWnd, buf, bufSize)) // [ANSIONLY]
          ok = WriteFile(hFile, buf, textLen, &written, NULL);
        GlobalFree(buf);
        }
      }
    CloseHandle(hFile);
    }
  return ok;
  }

/** Function: Menue Datei Neu **/
static void menuFileNew(HWND hWnd)
  {
  HWND hWinEdit = GetDlgItem(hWnd, IDW_WIN_EDIT);
  SetDlgItemText(hWnd, IDW_WIN_EDIT, TEXT("")); // Loesche EDIT-Text
  SendDlgItemMessage(hWnd, IDW_WIN_STAT, SB_SETTEXT, STINDI, (LPARAM)TEXT("")); // Oder "Neu"
  SendDlgItemMessage(hWnd, IDW_WIN_STAT, SB_SETTEXT, STFILE, (LPARAM)TEXT("")); // Oder "Unbenannt"
  dirty = 0;
  *filePath = 0;
  SendMessage(hWinEdit, EM_SETREADONLY, readOnly=0, 0);
  }

/** Function: Lade Datei **/
static BOOL fileOpen(HWND hWnd, TCHAR newFilePath[], int userReadOnly)
  {
  HWND hWinEdit = GetDlgItem(hWnd, IDW_WIN_EDIT);
  TCHAR s[6] = TEXT("\4");
  int i;
  readOnly = userReadOnly; // V1.1 Fix
  #ifdef BUILD_RO
  if (!readOnly)
    readOnly = (GetFileAttributes(newFilePath) & FILE_ATTRIBUTE_READONLY) ? 1 : 0;
  #endif
  if (!fileLoad(hWinEdit, newFilePath)) {
    ut_messageBoxExt(hWnd, TEXT("Kann Datei '%s' nicht laden!"), appTitle, MB_ICONWARNING, newFilePath);
    return FALSE;
    }
  lstrcpyn(filePath, newFilePath, MAX_PATH);
  SendMessage(hWinEdit, EM_SETREADONLY, readOnly, 0);
  SendDlgItemMessage(hWnd, IDW_WIN_STAT, SB_SETTEXT, STINDI, (LPARAM)TEXT("")); // Oder "Geoeffnet"
  SendDlgItemMessage(hWnd, IDW_WIN_STAT, SB_SETTEXT, STFILE, (LPARAM)filePath);
  dirty = 0;
  /* V1.1: Steht ein .LOG zu Beginn der Datei, fuege Datum am Ende ein */
  if (!readOnly)
    if (SendMessage(hWinEdit, EM_GETLINE, (WPARAM)0, (LPARAM)s) > 0)
      if (lstrcmp(s,TEXT(".LOG")) == 0) {
        i = GetWindowTextLength(hWinEdit);
        SendMessage(hWinEdit, EM_SETSEL, (WPARAM)i, (LPARAM)i);
        SendMessage(hWinEdit, EM_REPLACESEL, (WPARAM)FALSE, (LPARAM)TEXT("\r\n--- "));
        SendMessage(hWnd, WM_COMMAND, (WPARAM)IDM_EDIT_INSDATE, (LPARAM)0);
        SendMessage(hWinEdit, EM_REPLACESEL, (WPARAM)FALSE, (LPARAM)TEXT("\r\n"));
        }
  return TRUE;
  }

/** Function: Menue Datei oeffnen **/
static void menuFileOpen(HWND hWnd)
  {
  OPENFILENAME ofn;
  ZeroMemory(&ofn, sizeof ofn);
  ofn.lStructSize = sizeof ofn;
  ofn.hwndOwner = hWnd;
  ofn.lpstrFilter = fileFilter;
  ofn.lpstrFile = filePath;
  ofn.nMaxFile = MAX_PATH;
  ofn.lpstrDefExt = fileDefExt;
  ofn.Flags = OFN_FILEMUSTEXIST;
  if (!GetOpenFileName(&ofn))
    return;
  fileOpen(hWnd, filePath, ofn.Flags & OFN_READONLY); // Merke Benutzerentscheidung RO oder RW aus Dialog
  }

/** Function: Menue Datei speichern unter **/
static void menuFileSaveAs(HWND hWnd)
  {
  HWND hWinEdit;
  OPENFILENAME ofn;
  ZeroMemory(&ofn, sizeof ofn);
  ofn.lStructSize = sizeof ofn;
  ofn.hwndOwner = hWnd;
  ofn.lpstrFilter = fileFilter;
  ofn.lpstrFile = filePath;
  ofn.nMaxFile = MAX_PATH;
  ofn.lpstrDefExt = fileDefExt;
  ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;
  if (!GetSaveFileName(&ofn))
    return;
  hWinEdit = GetDlgItem(hWnd, IDW_WIN_EDIT);
  if (!fileSave(hWinEdit, filePath)) {
    ut_messageBoxExt(hWnd, TEXT("Kann Datei '%s' nicht speichern!"), appTitle, MB_ICONWARNING, filePath);
    return;
    }
  SendMessage(hWinEdit, EM_SETREADONLY, readOnly=0, 0); // V1.1 Fix
  SendDlgItemMessage(hWnd, IDW_WIN_STAT, SB_SETTEXT, STINDI, (LPARAM)TEXT("")); // Oder "Gespeichert"
  SendDlgItemMessage(hWnd, IDW_WIN_STAT, SB_SETTEXT, STFILE, (LPARAM)filePath);
  dirty = 0;
  }

/** Function: Menue Datei speichern **/
static void menuFileSave(HWND hWnd)
  {
  HWND hWinEdit = GetDlgItem(hWnd, IDW_WIN_EDIT);
  if (*filePath == 0) {
    menuFileSaveAs(hWnd);
    return;
    }
  if (!fileSave(hWinEdit, filePath)) {
    ut_messageBoxExt(hWnd, TEXT("Kann Datei '%s' nicht speichern!"), appTitle, MB_ICONWARNING, filePath);
    return;
    }
  SendDlgItemMessage(hWnd, IDW_WIN_STAT, SB_SETTEXT, STINDI, (LPARAM)TEXT("")); // Statt "Gespeichert"
  dirty = 0;
  }

/** Function: Menue Seite einrichten **/
static void menuFilePageSetup(PAGESETUPDLG *psd)
  {
  if (PageSetupDlg(psd)) { // Druckseiteninitialisierung (Seitenraender etc.) schon in WM_CREATE erfolgt
    if (psd->hDevMode) { // Falls beim Aufruf von PageSetupDlg hDevMode und hDevNames NULL sind, alloziert
      GlobalFree(psd->hDevMode); psd->hDevMode = NULL; } // OS Speicher und fuellt die Strukturen mit
    if (psd->hDevNames) { // Druckerwerten; daher sollten wir Speicher wieder freigeben.
      GlobalFree(psd->hDevNames); psd->hDevNames = NULL; }
    }
  }

/** Function: Drucke Datei **/
static BOOL filePrint(HWND hWnd, PAGESETUPDLG *psd, LOGFONT *lfPrt, PRINTDLG *pd)
  {
  BOOL ok = TRUE;
  DOCINFO di;
  HFONT hSavedPrintFont, hPrintFont;
  HDC hDCScr;
  TEXTMETRIC tm;
  TCHAR *buf;
  SIZE lnSize;
  HWND hWinEdit = GetDlgItem(hWnd, IDW_WIN_EDIT);
  int yChar, linesPerPage, line, page, dpiXPrt, dpiYPrt, dpiYScr, bufLen, i, iSOL, iEOL;
  LONG marginLeft, marginRight, marginTop, marginBottom, hSavedlfHeight, xPage, yPage, xPrint, yPrint;

  /* Ermittele DPI von Drucker und Bildschirm */
  hDCScr = GetWindowDC(hWnd);
  dpiYScr = GetDeviceCaps(hDCScr, LOGPIXELSY);
  ReleaseDC(hWnd, hDCScr);
  dpiXPrt = GetDeviceCaps(pd->hDC, LOGPIXELSX);
  dpiYPrt = GetDeviceCaps(pd->hDC, LOGPIXELSY);

  /* Rechne Druckraender von Hundertstel mm in Pixel um */
  marginLeft = psd->rtMargin.left * dpiXPrt / 2540; // 2500 / 100 * 600 dpi / 25,4 mmpi = 2500 * 600 / 2540 = 590 px
  marginTop = psd->rtMargin.top *  dpiYPrt / 2540;
  marginRight = psd->rtMargin.right * dpiXPrt / 2540;
  marginBottom = psd->rtMargin.bottom * dpiYPrt / 2540;

  /* Waehle Druckschrift ueber (passager korrigierten) LOGFONT aus */
  // Da LOGFONT auf (veralteter) Bildschirmetrik von 72 dpi fusst, ist Druckschriftgroesse (300 dpi etc.) zu korrigieren,
  // sonst Ausdruck zu klein. Zudem muss alte LONGFONT-Groesse des Druckfonts gemerkt werden, da sonst beim
  // naechsten Aufruf von menuOptionsFont() der Dialog die korrigierte (z.B. vierfache) Groesse als Startwert
  // darstellen und bei jedem weiteren Aufruf entsprechend weiter vergroessern wuerde.
  // In den meisten Dokus wird der korrigierte Wert ueber CHOOSEFONT.iPointSize ermittelt:
  // Annahme: Benutzergewaehlte Punktgroesse von 10 PT (1 Pt = 1/72 Inch). (somit iPointSize = 10 x PtSize = 100).
  // Dann gilt: lfHeight = - Punktgroesse * Druck-DPI / Urbildschirm-DPI
  // Berechnung lfHeight vom Font 10 Pt fuer Bildschirm-Uraufloesung (72 dpi, MAC):
  // lfHeight = - 10
  // Berechnung von lfHeight vom Font 10 Pt fuer heutige Bildschirm-Standardaufloesung (96 dpi):
  // lfHeight = - 10 * 96 / 72 = -13,333
  // Berechnung lfHeight vom Font 10 Pt fuer Bildschirm-Hochaufloesung (120 dpi):
  // lfHeight = - 10 * 120 / 72 = -16,666
  // Berechnung lfHeight vom Font 10 Pt fuer Druckeraufloesung 600 dpi:
  // lfHeight = - 10 * 600 / 72 = -83,333
  // Da CHOOSEFONT.iPointSize nur in menuOptionsFont() z.V. steht und Programmende nicht ueberdauert, berechnen wir
  // korrigiertes lfPrt.lfHeight direkt aus unkorrigiertem mit Faktor DruckDPI (z.B. 600 dpi) / BildschirmDPI (meist 96 dpi):
  // lfPrt.lfHeight = lfPrt.lfHeight * dpiYPrt / dpiYScr
  hSavedlfHeight = lfPrt->lfHeight; // Merke alte LOGFONT-Groesse
  lfPrt->lfHeight = MulDiv(lfPrt->lfHeight, dpiYPrt, dpiYScr);  // Passagere LOGFONT-Korrektur
  hPrintFont = CreateFontIndirect(lfPrt); // Erstelle Druckfont
  lfPrt->lfHeight = hSavedlfHeight; // Restauriere alten LOGFONT
  hSavedPrintFont = SelectObject(pd->hDC, (HGDIOBJ)hPrintFont);
  if (hSavedPrintFont == NULL || hSavedPrintFont == (HGDIOBJ)GDI_ERROR)
    MessageBox(hWnd, TEXT("Kann Druckfont nicht laden!"), appTitle, MB_ICONWARNING);

  /* Ermittele Pixel-Metrik der Druckschrift und -seite */
  GetTextMetrics(pd->hDC, &tm); // Ermittle Metrik des aktuell ausgew. phys. Fonts des ausgew. Druckers hDC
  yChar = tm.tmHeight + tm.tmExternalLeading; // Druckzeilenhoehe = Druckzeichenhoehe + Durchschuss
  xPage = GetDeviceCaps(pd->hDC, HORZRES);
  yPage = GetDeviceCaps(pd->hDC, VERTRES);
  xPrint = xPage - marginLeft - marginRight; // Bedruckbare Breite [px]
  yPrint = yPage - marginTop - marginBottom; // Bedruckbare Hoehe [Rasterlinien]
  linesPerPage = yPrint / yChar;

  /* Lade EDIT-Control in Puffer */
  if ((buf = pd->Flags & PD_SELECTION ? ut_getEditSel(hWinEdit, &bufLen) : ut_getEditText(hWinEdit, &bufLen)) == NULL)
    return FALSE; // EM_GETLINE unterscheidet keine weiche und harte Umbrueche, daher ganzer Text zu kopieren

  /* Benenne Druckjob */
  ZeroMemory(&di, sizeof di);
  di.cbSize = sizeof di;
  if (pd->Flags & PD_PRINTTOFILE)
    di.lpszOutput = TEXT("FILE:");
  di.lpszDocName = *filePath ? filePath : TEXT("Unbenannt");

  /* Drucke */
  if (StartDoc(pd->hDC, &di) > 0) {

    /* Fuer jede Druckseite */
    for (page=i=iSOL=0; i<bufLen; page++) {
      if (StartPage(pd->hDC) <= 0) {
        ok = FALSE; break; }

      /* Fuer jede Druckzeile */
      for (line=0; i<bufLen && line<linesPerPage; line++) {

        /* Ermittele Druckzeilenende iEOL */
        for (lnSize.cx=iEOL=0; i<bufLen && lnSize.cx<xPrint; i++) {
          if (buf[i] == TEXT(' '))
            iEOL = i;
          else if (buf[i] == TEXT('-'))
            iEOL = i + 1;
          else if (buf[i] == TEXT('\r')) {
            iEOL = i; i += 2; break; }
          if (!GetTextExtentPoint32(pd->hDC, buf+iSOL, i+1-iSOL, &lnSize)) // Ermittele lnSize.cx bis i+1 Zeichen
            MessageBox(hWnd, TEXT("Druckmetrik nicht bestimmbar.\nPrüfen Sie den Ausdruck!"),
            appTitle, MB_ICONWARNING);
          };
        if (iEOL == 0) // Bei uebergrossen Zeichenketten wuerde kein iEOL gesetzt, daher zwangsweise hier
          iEOL = i - 1;
        if (i >= bufLen && buf[iEOL] != TEXT('\r')) // Falls letzte Zeile ohne CRLF, wuerde sonst nur bis
          iEOL = bufLen; // vorletztes Zeichen (nur 1 Wort auf Zeile) oder bis vor letztes Blank gedruckt

        /* Drucke Druckzeile */
        TextOut(pd->hDC, marginLeft, marginTop+line*yChar, buf+iSOL, iEOL-iSOL); // Drucke Druckzeile
        switch (buf[iEOL]) { // Ermittele naechsten Druckzeilenanfang
          case TEXT(' '): iSOL = iEOL + 1; break;
          case TEXT('\r'): iSOL = iEOL + 2; break;
          default: iSOL = iEOL; // Lass iSOL nur Trennzeichen ueberspringen, aber nicht '-' etc.
          }
        } // Fuer jede Druckzeile

      /* Drucke Seitennummer */
      if (printPageNo) {
        TCHAR num[6];
        int numWidth = wsprintf(num, TEXT("%d"), page+1);
        TextOut(pd->hDC, xPage/2, yPage-marginBottom+100, num, numWidth); // Vereinfachte Positionsberechnung
        }
      if (EndPage(pd->hDC) <= 0) {
        ok = FALSE; break; }
      } // Fuer jede Druckseite
    if (ok)
      EndDoc(pd->hDC);
    }

  /* Raeume auf */
  GlobalFree(buf);
  SelectObject(pd->hDC, (HGDIOBJ)hSavedPrintFont);
  DeleteObject(hPrintFont);
  return ok;
  }

/** Function: Menu Datei drucken **/
static void menuFilePrint(HWND hWnd, PAGESETUPDLG *psd, LOGFONT *lf)
  {
  PRINTDLG pd;
  TCHAR saveStatText[200];
  int sos, eos;
  HWND hWinEdit = GetDlgItem(hWnd, IDW_WIN_EDIT);

  /* Praepariere Druckdialog */
  ZeroMemory(&pd, sizeof pd);
  pd.lStructSize = sizeof pd;
  pd.hwndOwner = hWnd;  // Veraltetes PD_PRINTSETUP durch PageSetup ersetzt
  pd.Flags = PD_USEDEVMODECOPIESANDCOLLATE | PD_NOPAGENUMS | PD_RETURNDC;
  pd.nCopies = 1;
  SendMessage(hWinEdit, EM_GETSEL, (WPARAM)&sos, (LPARAM)&eos);
  if (eos > sos) // Falls Text markiert ist,
    pd.Flags |= PD_SELECTION; // schlage "Markierung" als Druckbereich im Druckdialog vor

  /* Rufe Druckdialog auf */
  if (!PrintDlg(&pd))
    return;

  /* Drucke */
  SendDlgItemMessage(hWnd, IDW_WIN_STAT, SB_GETTEXT, STINDI, (LPARAM)saveStatText); //  Speichere alt
  SendDlgItemMessage(hWnd, IDW_WIN_STAT, SB_SETTEXT, STINDI, (LPARAM)TEXT("Drucke...")); // Setze neu
  SetBkMode(pd.hDC, TRANSPARENT);
  if (!filePrint(hWnd, psd, lf, &pd))
    MessageBox(hWnd, TEXT("Kann Datei nicht drucken!"), appTitle, MB_ICONWARNING);

  /* Raeume auf */
  SendDlgItemMessage(hWnd, IDW_WIN_STAT, SB_SETTEXT, STINDI, (LPARAM)saveStatText);
  if (pd.hDevMode)
    GlobalFree(pd.hDevMode); // Falls beim Aufruf von PrintDlg hDevMode und hDevNames NULL sind, alloziert OS
  if (pd.hDevNames) // Speicher und fuellt Strukturen mit Druckerwerten; daher sollten wir Speicher wieder freigeben.
    GlobalFree(pd.hDevNames);
  if (pd.hDC)
    DeleteDC(pd.hDC);
  }

/** Function: Rueckfrage zur Dateispeicherung vor Programmende **/
static int fileSaveBeforeExit(HWND hWnd)
  {
  int i = ut_messageBoxExt(hWnd,
    TEXT("Der Text in der Datei '%s' wurde geändert.\nSollen die Änderungen gespeichert werden?"),
    appTitle, MB_ICONQUESTION | MB_YESNOCANCEL, filePath);
  if (i == IDYES)
    menuFileSave(hWnd);
  return i;
  }

#ifdef BUILD_UC

/** Function: Initialisiere EDIT-Farben **/
static void initColors(CHOOSECOLOR *cc, COLORREF *customclrs, HWND hWnd, COLORREF init)
  {
  cc->lStructSize = sizeof(CHOOSECOLOR);
  cc->hwndOwner = hWnd;
  cc->rgbResult = init;
  cc->lpCustColors = customclrs; // Lass Windows die Benutzerfarben merken zwischen Coloraufrufen
  cc->Flags = CC_ANYCOLOR | CC_RGBINIT;
  }

#endif

/** Function: Callback-Funktion des Versionsdialogs **/
static BOOL CALLBACK verDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
  {
  INITCOMMONCONTROLSEX iccx;
  LITEM item;

  switch (msg) {
  case WM_INITDIALOG:
    iccx.dwSize = sizeof(INITCOMMONCONTROLSEX);
    iccx.dwICC = ICC_LINK_CLASS;
    if (!InitCommonControlsEx(&iccx)) {
      MessageBox(hDlg, TEXT("InitCommonControlsEx() gescheitert! Fehlt COMCTL32.DLL V6?"), appTitle, MB_ICONINFORMATION);
      return FALSE;
      }
    return TRUE;
  case WM_COMMAND:
    switch (wParam) {
    case IDOK:
      EndDialog(hDlg, TRUE);
      return TRUE;
    case IDCANCEL:
      EndDialog(hDlg, FALSE);
      return TRUE;
    }
  case WM_NOTIFY:
    if (wParam == IDC_VERLINK)
      switch (((LPNMHDR)lParam)->code) {
      // WM_NOTIFY-Meldungen des SysLink-Elementes: wParam = IDC_SYSLINK und lParam = NM_CLICK oder NM_RETURN
      case NM_CLICK:
      case NM_RETURN:
        item = ((PNMLINK)lParam)->item;
        // ShellExecute zu Link fkt. nur mit UNICODE gesetzt oder explizit ShellExecuteW().
        // "open" muß ebf. Unicode sein (L""), Resource-Eintrag ist immer in UNICODE
        ShellExecuteW(NULL, L"open", item.szUrl, NULL, NULL, SW_SHOW);
        return TRUE;
      }
  } // switch(msg)
  return FALSE;
  }

/** Function: Callbackfunktion des Hauptfensters **/
static LRESULT CALLBACK winProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
  {
  static HWND hWinEdit, hWinStat;
  static TCHAR iniPath[MAX_PATH];
  static HFONT hFont;
  static PAGESETUPDLG psd;
  static LOGFONT lfScr, lfPrt;
  #ifdef BUILD_TB
  static HWND hWinTool;
  #endif
  #ifdef BUILD_UC
  static COLORREF bgClr, bgClr1, bgClr2, fgClr, fgClr1, fgClr2;
  static CHOOSECOLOR choosecolor;
  static LOGBRUSH logbrush;
  #ifdef BUILD_RE
  static CHARFORMAT cr;
  #endif
  #endif
  static BOOL showWinTool, showWinStat, saveOptions, nightMode, makeIniDir;
  static int padding;
  #ifdef BUILD_FR
  static UINT fr_msg;
  #endif
  static TCHAR s[MAX_PATH]; // Generisch fuer mehrere Codeabschnitte in winProc verwendet

  switch (msg) {

  case WM_CREATE:
    {
    #ifdef BUILD_TB
    const TBBUTTON tbb[] = {
      // Bei deklarativer Initialisierung wird undokumentiertes, in commctrl.h definiertes, zusaetzliches Paddingfeld
      // benoetigt, da es ein Array ist; bei expl. Initialisierung der einzelnen Felder zur Laufzeit tritt Problem nicht auf.
      // V1.1 Funktioniert nicht mehr mit LCC V3.8 Build 2011
      // StdBitmapID  CommandID       State            Style           Pad  dwData Beschriftung
      { STD_FILENEW,  IDM_FILE_NEW,   TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0L, 0 },
      { STD_FILEOPEN, IDM_FILE_OPEN,  TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0L, 0 },
      { STD_FILESAVE, IDM_FILE_SAVE,  TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0L, 0 },
      { STD_PRINT,    IDM_FILE_PRINT, TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0L, 0 },
      // { STD_PRINTPRE, IDM_FILE_PRINT, TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0L, 0 },
      { 0,            0,              TBSTATE_ENABLED, TBSTYLE_SEP,    {0}, 0L, 0 },
      { STD_UNDO,     IDM_EDIT_UNDO,  TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0L, 0 },
      { STD_CUT,      IDM_EDIT_CUT,   TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0L, 0 },
      { STD_COPY,     IDM_EDIT_COPY,  TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0L, 0 },
      { STD_PASTE,    IDM_EDIT_PASTE, TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0L, 0 },
      { STD_DELETE,   IDM_EDIT_CLEAR, TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0L, 0 },
      { STD_FIND,     IDM_EDIT_FIND,  TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0L, 0 },
      { STD_REPLACE,  IDM_EDIT_REPL,  TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0L, 0 },
      // { 0,            0,              TBSTATE_ENABLED, TBSTYLE_SEP,    {0}, 0L, 0 },
      // { STD_HELP,     IDM_HELP_VER,   TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0L, 0 },
      };
    TBADDBITMAP tbab;
    #endif
    #ifdef BUILD_UC
    static COLORREF customcolors[16]; // Muss static sein, merkt sich benutzerdef. Farben
    #endif
    const int statwidths[] = {15, 140, -1}; // Li. Statusfeld endet bei 15, mittl. bei 140, re. am Fensterende

    /* Lese Ini-Datei (appDataPath | programPath) "\ed.ini" */
    lstrcpy(iniPath+GetModuleFileName(NULL,iniPath,MAX_PATH)-3,TEXT("ini"));
    if (!ut_fileExists(iniPath)) { // V1.1 New
      if (SHGetFolderPath(0, CSIDL_LOCAL_APPDATA, 0, SHGFP_TYPE_CURRENT, iniPath) == S_OK) {
        lstrcat(iniPath, TEXT("\\ed"));
        if (!ut_dirExists(iniPath))
          makeIniDir = TRUE;
        lstrcat(iniPath, TEXT("\\ed.ini"));
        }
      }
    GetPrivateProfileString(TEXT("Options"), TEXT("ScrFontName"), TEXT("Times New Roman"), lfScr.lfFaceName, 33, iniPath);
    lfScr.lfHeight = GetPrivateProfileInt(TEXT("Options"), TEXT("ScrFontSize"), -16, iniPath);
    lfScr.lfWeight = GetPrivateProfileInt(TEXT("Options"), TEXT("ScrFontWeight"), 400, iniPath);
    GetPrivateProfileString(TEXT("Options"), TEXT("PrtFontName"), TEXT("Times New Roman"), lfPrt.lfFaceName, 33, iniPath);
    lfPrt.lfHeight = GetPrivateProfileInt(TEXT("Options"), TEXT("PrtFontSize"), -16, iniPath);
    lfPrt.lfWeight = GetPrivateProfileInt(TEXT("Options"), TEXT("PrtFontWeight"), 400, iniPath);
    #ifdef BUILD_UC
    GetPrivateProfileString(TEXT("Options"), TEXT("ScrFontColor1"), TEXT(""), s, 7, iniPath);
    fgClr = fgClr1 = s[0] ? ut_revRGBVal(s) : GetSysColor(COLOR_WINDOWTEXT);
    GetPrivateProfileString(TEXT("Options"), TEXT("ScrBgColor1"), TEXT(""), s, 7, iniPath);
    bgClr = bgClr1 = s[0] ? ut_revRGBVal(s) : GetSysColor(COLOR_WINDOW);
    GetPrivateProfileString(TEXT("Options"), TEXT("ScrFontColor2"), TEXT("FFFF80"), s, 7, iniPath);
    fgClr2 = ut_revRGBVal(s);
    GetPrivateProfileString(TEXT("Options"), TEXT("ScrBgColor2"), TEXT("000000"), s, 7, iniPath);
    bgClr2 = ut_revRGBVal(s);
    #endif
    #ifdef BUILD_TB
    showWinTool = GetPrivateProfileInt(TEXT("Options"), TEXT("ToolBar"), 1, iniPath);
    #endif
    showWinStat = GetPrivateProfileInt(TEXT("Options"), TEXT("StatusBar"), 1, iniPath);
    padding = GetPrivateProfileInt(TEXT("Options"), TEXT("Padding"), 10, iniPath);

    /* Baue Child-Fenster EDIT */
    #ifdef BUILD_RE
    #if BUILD_RE == VER01 // RichEdit 1.0
    if ((richEditLib=LoadLibrary(TEXT("riched32.dll"))) == NULL)
      MessageBox(hWnd, TEXT("Kann riched32.dll nicht laden!"), appTitle, MB_ICONWARNING);
    hWinEdit = CreateWindowEx(WS_EX_CLIENTEDGE, TEXT("RICHEDIT"), NULL,
      WS_CHILD | WS_VISIBLE | ES_MULTILINE | WS_VSCROLL | ES_AUTOVSCROLL | ES_NOHIDESEL,
      0, 0, 0, 0, hWnd, (HMENU)IDW_WIN_EDIT, GetModuleHandle(NULL), NULL);
    #else // RichEdit 2.0 und 3.0
    if ((richEditLib=LoadLibrary(TEXT("riched20.dll"))) == NULL)
      MessageBox(hWnd, TEXT("Kann riched20.dll nicht laden!"), appTitle, MB_ICONWARNING);
    hWinEdit = CreateWindowEx(WS_EX_CLIENTEDGE, RICHEDIT_CLASS, NULL,
      WS_CHILD | WS_VISIBLE | ES_MULTILINE | WS_VSCROLL | ES_AUTOVSCROLL | ES_NOHIDESEL,
      0, 0, 0, 0, hWnd, (HMENU)IDW_WIN_EDIT, GetModuleHandle(NULL), NULL);
    #endif
    SendMessage(hWinEdit, EM_EXLIMITTEXT, (WPARAM)0, (LPARAM)(DWORD)MAXTEXT); // Erhoehe 32K Std fuer RE
    #else
    hWinEdit = CreateWindowEx(WS_EX_CLIENTEDGE, TEXT("EDIT"), NULL, // Initialer Text _innerhalb_ EDIT
      WS_CHILD | WS_VISIBLE | ES_MULTILINE | WS_VSCROLL | ES_AUTOVSCROLL | ES_NOHIDESEL,
      0, 0, 0, 0, hWnd, (HMENU)IDW_WIN_EDIT, GetModuleHandle(NULL), NULL);
    SendMessage(hWinEdit, EM_SETLIMITTEXT, (WPARAM)MAXTEXT, (LPARAM)0); // Erhoehe 64K Std fuer EDIT
    #endif
    if (!hWinEdit)
      MessageBox(hWnd, TEXT("Kann Editorfenster nicht laden!"), appTitle, MB_ICONWARNING);

    /* Initialisiere Font */
    if (lfScr.lfFaceName[0] == 0)
      GetObject(GetStockObject(SYSTEM_FONT), sizeof lfScr, (PTSTR)&lfScr); // Falls INI leer, benutze Systemfont
    hFont = CreateFontIndirect(&lfScr);
    SendMessage(hWinEdit, WM_SETFONT, (WPARAM)hFont, (LPARAM)TRUE);

    /* Initialisiere Farben */
    #ifdef BUILD_UC
    #ifdef BUILD_RE
    ZeroMemory(&cr, sizeof cr);
    cr.cbSize = sizeof cr;
    cr.dwMask = CFM_COLOR;
    cr.crTextColor = fgClr;
    SendMessage(hWinEdit, EM_SETBKGNDCOLOR, (WPARAM)0, (LPARAM)bgClr);
    // Microsoft: SCF_ALL-Nachricht soll nach WM_SETFONT erfolgen:
    SendMessage(hWinEdit, EM_SETCHARFORMAT, (WPARAM)(UINT)SCF_ALL, (LPARAM)(CHARFORMAT*)&cr);
    #endif
    logbrush.lbHatch = 0;
    logbrush.lbStyle = BS_SOLID;
    logbrush.lbColor = RGB(0,0,0);
    initColors(&choosecolor, customcolors, hWinEdit, fgClr); // Befuellt choosecolor und customcolors
    #endif

    /* Initialisiere teilweise Druckseite */
    // Ein Teil der Druckseiteninitialisierung wird bereits hier statt erst in menuePageSetup() vorgenommen,
    // damit auch ohne vorherigen Aufruf von menuePageSetup() andere Funktionen wie z.B. menuPrint()
    // die Randbreite kennen bzw. eine solche ueberhaupt erst eingestellt ist.
    // Die Variante, PageSetupDlg ohne Dialogbox als Initialisierung von DEVMODE / DEVNAMES aufzurufen
    // wird hier nicht verwendet, um keine evt. Wartezeit auf nicht vorhandene Drucker zu erzeugen.
    ZeroMemory(&psd, sizeof(psd));
    psd.lStructSize = sizeof(psd);
    psd.hwndOwner = hWnd;
    psd.Flags = PSD_INHUNDREDTHSOFMILLIMETERS | PSD_MARGINS;
    psd.rtMargin.top = psd.rtMargin.left = psd.rtMargin.right = psd.rtMargin.bottom = 2500; // 25 mm

    /* Baue Child-Fenster Toolbar */
    #ifdef BUILD_TB
    hWinTool = CreateWindowEx(0, TOOLBARCLASSNAME, NULL, WS_CHILD,
      // Kein WS_VISIBLE, da per INI Toolbar beim Start unsichtbar sein kann; Schaltung muss daher extra erfolgen
      0, 0, 0, 0, hWnd,(HMENU)IDW_WIN_TOOL, GetModuleHandle(NULL), NULL);
    if (!hWinTool)
      MessageBox(hWnd, TEXT("Kann Werkzeugleiste nicht laden!"), appTitle, MB_ICONWARNING);
    SendMessage(hWinTool, TB_BUTTONSTRUCTSIZE, (WPARAM)sizeof(TBBUTTON), 0); // MS Kompatibilitaet
    tbab.hInst = HINST_COMMCTRL;
    tbab.nID = IDB_STD_SMALL_COLOR;  // Alternativ: IDB_STD_LARGE_COLOR
    SendMessage(hWinTool, TB_ADDBITMAP, 0, (LPARAM)&tbab); // Fuege Buttons hinzu
    SendMessage(hWinTool, TB_ADDBUTTONS, sizeof tbb / sizeof tbb[0],(LPARAM)&tbb);
    if (showWinTool)
       ShowWindow(hWinTool, SW_SHOW);
    #endif

    /* Baue Child-Fenster Statusleiste */
    hWinStat = CreateWindowEx(0, STATUSCLASSNAME, NULL, WS_CHILD | SBARS_SIZEGRIP,
      // Kein WS_VISIBLE, da per INI Statusleiste beim Start unsichtbar sein kann; Schaltung muss daher extra erfolgen
      0, 0, 0, 0, hWnd, (HMENU)IDW_WIN_STAT, GetModuleHandle(NULL), NULL);
    if (!hWinStat)
      MessageBox(hWnd, TEXT("Kann Statusleiste nicht laden!"), appTitle, MB_ICONWARNING);
    SendMessage(hWinStat, SB_SETPARTS, sizeof statwidths / sizeof statwidths[0], (LPARAM)statwidths);
    // SendMessage(hWinStat, SB_SETTEXT, 0, (LPARAM)TEXT("Bereit"));
    if (showWinStat)
       ShowWindow(hWinStat, SW_SHOW);

    /* Melde Nachrichten fuer Suchen/Ersetzen-Dialog beim System an */
    #ifdef BUILD_FR
    fr_msg = RegisterWindowMessage(FINDMSGSTRING);
    #endif

    /* Lese Namen zu ladender Texte von Kommandozeile oder von Shell-Drop auf ed.exe ein */
    ut_getArg(GetCommandLine(), NULL); // Achtung: ut_getArg() muss erst mit (.., NULL) initialisiert werden!
    ut_getArg(NULL, s); // Ueberspringe "ed.exe"
    ut_getArg(NULL, s); // Lese Dateinamen
    if (*s)
      fileOpen(hWnd, s, 0);

    return 0;
    } // WM_CREATE

  case WM_SIZE:
    {
    RECT rcEdit, rcStat;
    #ifdef BUILD_TB
    RECT rcTool;
    #endif
    int iEditHeight, iStatHeight = 0, iToolHeight = 0;
    #ifdef BUILD_TB
    if (showWinTool) {
      SendMessage(hWinTool, TB_AUTOSIZE, 0, 0); GetWindowRect(hWinTool, &rcTool);
      iToolHeight = rcTool.bottom - rcTool.top;
      }
      #endif
    if (showWinStat) {
      SendMessage(hWinStat, WM_SIZE, 0, 0); GetWindowRect(hWinStat, &rcStat); iStatHeight = rcStat.bottom - rcStat.top; }
    GetClientRect(hWnd, &rcEdit); iEditHeight = rcEdit.bottom - iToolHeight - iStatHeight;
    SetWindowPos(hWinEdit, NULL, 0, iToolHeight, rcEdit.right, iEditHeight, SWP_NOZORDER);
    // Wie stellt man das Padding des EDIT-Control ein? An sich klingt SendMessage(hWinEdit, EM_SETMARGINS... gut:
    // die Einstellung wirkt permanent und muss daher nicht in WM_SIZE untergebracht werden; geaendert wird
    // aber nur der li. und re. Rand, nicht der ob. und unt.
    // Setzt man stattdessen die Option EC_USEFONTINFO, werden Raender sogar verkleinert statt vergroessert!
    // Relativer Rand berechnet aus FontInfo:
    // SendMessage(hWinEdit, EM_SETMARGINS, (WPARAM)(EC_USEFONTINFO), 0);
    // Absoluter Rand: fkt. zwar, aber geht nicht auf variable Schriftgroesse ein:
    // SendMessage(hWinEdit, EM_SETMARGINS, (WPARAM)(EC_LEFTMARGIN | EC_RIGHTMARGIN), MAKELPARAM(30,30));
    // Daher Weg ueber EM_SETRECT gegangen, dessen Einstellung aber immer nur bis zum naechsten WM_SIZE haelt;
    // daher muss dieser Ansatz in WM_SIZE selbst untergebracht werden.
    #ifndef BUILD_RE
    // Unter RICHEDIT verkleinert sich Fensterinhalt bei jedem Sizing immer mehr, daher hier deaktiviert
    if (padding > 0) {
      SendMessage(hWinEdit, EM_GETRECT, 0, (LPARAM)(LPRECT)&rcEdit);
      rcEdit.left += padding; rcEdit.top += padding; rcEdit.right -= padding; rcEdit.bottom -= padding;
      SendMessage(hWinEdit, EM_SETRECT, 0, (LPARAM)(LPRECT)&rcEdit);
      }
    #endif
    return 0;
    }

  #ifdef BUILD_EM
  case WM_INITMENUPOPUP:
    switch (lParam) {
    case 0: /* Menue Datei */
      {
      /* Aktiviere Menueeintraege Neu, Speichern, Drucken, falls moeglich */
      // Restriktiver als die meisten Editoren:
      // - Speichern/Drucken nur bei nichtleeren Dateien gestattet
      // - Speichern nur bei readwrite gestattet
      // - Neue Datei nur moeglich, falls nichtleer oder (ggf. leere) benannte Datei aktuell geladen
      int textLen = GetWindowTextLength(hWinEdit);
      EnableMenuItem((HMENU)wParam, IDM_FILE_NEW, MESTAT(textLen || *filePath));
      EnableMenuItem((HMENU)wParam, IDM_FILE_SAVE, MESTAT(dirty && textLen));
      EnableMenuItem((HMENU)wParam, IDM_FILE_SAVEAS, MESTAT(textLen));
      EnableMenuItem((HMENU)wParam, IDM_FILE_PRINT, MESTAT(textLen));
      return 0;
      }
    case 1: /* Menue Bearbeiten */
      {
      /* Aktiviere Menueeintraege Rueckgaengig und Einfuegen, falls moeglich */
      int sel0, selz, enable; // READONLY verweigert Tippen/Einfuegen, nicht aber REPLACESEL, INSDATE, daher hier
      EnableMenuItem((HMENU)wParam, IDM_EDIT_UNDO,
        MESTAT(!readOnly && SendMessage(hWinEdit,EM_CANUNDO,0,0L)));
      EnableMenuItem((HMENU)wParam, IDM_EDIT_PASTE,
        MESTAT(!readOnly && IsClipboardFormatAvailable(CF_TEXT)));
      SendMessage(hWinEdit, EM_GETSEL, (WPARAM)&sel0, (LPARAM)&selz);
      /* Aktiviere Menueeintraege Kopieren, Ausschneiden und Loeschen, falls Text ausgewaehlt */
      enable = sel0 != selz;
      EnableMenuItem((HMENU)wParam, IDM_EDIT_CUT, MESTAT(enable && !readOnly));
      EnableMenuItem((HMENU)wParam, IDM_EDIT_COPY, MESTAT(enable));
      EnableMenuItem((HMENU)wParam, IDM_EDIT_CLEAR, MESTAT(enable && !readOnly));
      /* Aktiviere Menueeintraege Suchen, Ersetzen und Weitersuchen, falls Dialoge nicht schon aktiv */
      EnableMenuItem((HMENU)wParam, IDM_EDIT_FIND, MESTAT(fr_hDlg==NULL));
      EnableMenuItem((HMENU)wParam, IDM_EDIT_NEXT, MESTAT(fr_hDlg==NULL));
      EnableMenuItem((HMENU)wParam, IDM_EDIT_REPL, MESTAT(fr_hDlg==NULL && !readOnly));
      EnableMenuItem((HMENU)wParam, IDM_EDIT_INSDATE, MESTAT(!readOnly));
      return 0;
      }
    case 2: /* Menue Optionen */
      /* Setze Status bestimmter Menueeintraege */
      EnableMenuItem((HMENU)wParam, IDM_OPTION_FGCOL, MESTAT(!readOnly)); // V1.1 Fix
      EnableMenuItem((HMENU)wParam, IDM_OPTION_BGCOL, MESTAT(!readOnly)); // V1.1 Fix
      CheckMenuItem((HMENU)wParam, IDM_OPTION_PRINTPAGENO, printPageNo?MF_CHECKED:MF_UNCHECKED);
      #ifdef BUILD_TB
      CheckMenuItem((HMENU)wParam, IDM_OPTION_SHOWTOOL, showWinTool?MF_CHECKED:MF_UNCHECKED);
      #endif
      CheckMenuItem((HMENU)wParam, IDM_OPTION_SHOWSTAT, showWinStat?MF_CHECKED:MF_UNCHECKED);
      CheckMenuItem((HMENU)wParam, IDM_OPTION_SAVE, saveOptions?MF_CHECKED:MF_UNCHECKED);
      return 0;
    }
  #endif

  #ifdef BUILD_UC
  #ifndef BUILD_RE
  case WM_CTLCOLOREDIT:
    {
    static HBRUSH hBrush;
    SetBkColor((HDC)wParam, bgClr);
    SetTextColor((HDC)wParam, fgClr);
    logbrush.lbColor = bgClr;
    DeleteObject(hBrush);
    hBrush = CreateBrushIndirect(&logbrush);
    return (LRESULT)hBrush;
    }
  #endif
  #endif

  case WM_SETFOCUS:
    SetFocus(hWinEdit);
    #ifdef BUILD_FR
    if (fr_hDlg) // Benutzer von Suchdialog in WIN_EDIT gewechselt; informiere FindRoutinen,
      fr_setFocusLost(); // dass urspr. Selektion invalidisiert wurde
    #endif
    return 0;

  case WM_DROPFILES:
    {
    HDROP hDrop = (HDROP)wParam;
    if (DragQueryFile(hDrop, 0, s, MAX_PATH) > 0)
      if (!dirty || fileSaveBeforeExit(hWnd) != IDCANCEL)
        fileOpen(hWnd, s, 0);
    DragFinish(hDrop);
    return 0;
    }

  case WM_CLOSE:
    if (!dirty || fileSaveBeforeExit(hWnd) != IDCANCEL)
      DestroyWindow(hWnd);
    return 0;

  case WM_QUERYENDSESSION:
    return !dirty || fileSaveBeforeExit(hWnd) != IDCANCEL;

  case WM_DESTROY:
    /* Schreibe Ini-Datei */
    if (saveOptions) {
      if (makeIniDir) { // V1.1 New
        lstrcpyn(s,iniPath,lstrlen(iniPath)-sizeof TEXT("ed.ini")+1); // s := iniPath ohne Dateinamen
        if (!CreateDirectory(s,NULL))
          ut_messageBoxExt(hWnd, TEXT("Kann Verzeichnis '%s' nicht anlegen!"), appTitle, MB_ICONWARNING, s);
        }
      if (WritePrivateProfileString(TEXT("Ini"), TEXT("Version"), TEXT("1.0"), iniPath) == FALSE)
        ut_messageBoxExt(hWnd, TEXT("Kann Optionen nicht in '%s' schreiben!"), appTitle, MB_ICONWARNING, iniPath);
      else {
        WritePrivateProfileString(TEXT("Options"), TEXT("ScrFontName"), lfScr.lfFaceName, iniPath);
        WritePrivateProfileString(TEXT("Options"), TEXT("ScrFontSize"), ut_uLong2Str(lfScr.lfHeight,10), iniPath);
        WritePrivateProfileString(TEXT("Options"), TEXT("ScrFontWeight"), ut_uLong2Str(lfScr.lfWeight,10), iniPath);
        WritePrivateProfileString(TEXT("Options"), TEXT("PrtFontName"), lfPrt.lfFaceName, iniPath);
        WritePrivateProfileString(TEXT("Options"), TEXT("PrtFontSize"), ut_uLong2Str(lfPrt.lfHeight,10), iniPath);
        WritePrivateProfileString(TEXT("Options"), TEXT("PrtFontWeight"), ut_uLong2Str(lfPrt.lfWeight,10), iniPath);
        #ifdef BUILD_UC
        WritePrivateProfileString(TEXT("Options"), TEXT("ScrFontColor1"), ut_revRGBStr(fgClr1), iniPath);
        WritePrivateProfileString(TEXT("Options"), TEXT("ScrBgColor1"), ut_revRGBStr(bgClr1), iniPath);
        WritePrivateProfileString(TEXT("Options"), TEXT("ScrFontColor2"), ut_revRGBStr(fgClr2), iniPath);
        WritePrivateProfileString(TEXT("Options"), TEXT("ScrBgColor2"), ut_revRGBStr(bgClr2), iniPath);
        #endif
        WritePrivateProfileString(TEXT("Options"), TEXT("StatusBar"), showWinStat ? TEXT("1") : TEXT("0"), iniPath);
        #ifdef BUILD_TB
        WritePrivateProfileString(TEXT("Options"), TEXT("ToolBar"), showWinTool ? TEXT("1") : TEXT("0"), iniPath);
        #endif
        }
      }
    DeleteObject(hFont);
    PostQuitMessage(0);
    return 0;

  #ifdef BUILD_ST
  case IDM_UNHIDE:
    if ((UINT)wParam == IDI_ICON && (UINT)lParam == WM_LBUTTONDBLCLK) {
      st_DelIcon(hWnd, IDI_ICON);
      ShowWindow(hWnd, SW_RESTORE); // Jetzt wieder sichtbar, aber noch nicht aktiv
      SetForegroundWindow(hWnd); // Jetzt aktiv
      }
    return 0;
  #endif

  case WM_COMMAND:
    switch (LOWORD(wParam)) { // EDIT-Control sendet Nachrichten an das Elternfenster via WM_COMMAND

    case IDW_WIN_EDIT:
      switch(HIWORD(wParam)) {

      case EN_UPDATE:
        // Bei RichEdit erst ab V.2 automatisch an Parent gesendet; fuer V1 muesste (wie fuer EN_CHANGE) es erst via
        // EM_SETEVENTMASK angemeldet werden. Leider wird schon das initiale Textladen als Aenderung interpretiert.
        if (!dirty) {
          dirty = 1;
          SendDlgItemMessage(hWnd, IDW_WIN_STAT, SB_SETTEXT, STINDI, (LPARAM)TEXT("*"));
          return 0;
          }
        break;

      case EN_ERRSPACE: case EN_MAXTEXT:
        MessageBox(hWnd, TEXT("Maximale Editiergröße überschritten!"), appTitle, MB_ICONWARNING);
        return 0;
      }
      break; // IDW_WIN_EDIT

    case IDM_FILE_NEW:
      if (!dirty || fileSaveBeforeExit(hWnd) != IDCANCEL)
        menuFileNew(hWnd);
      return 0;

    case IDM_FILE_OPEN:
      if (!dirty || fileSaveBeforeExit(hWnd) != IDCANCEL)
        menuFileOpen(hWnd);
      return 0;

    case IDM_FILE_SAVE:
      menuFileSave(hWnd);
      return 0;

    case IDM_FILE_SAVEAS:
      menuFileSaveAs(hWnd);
      return 0;

    case IDM_FILE_PAGESETUP:
      menuFilePageSetup(&psd);
      return 0;

    case IDM_FILE_PRINT:
      menuFilePrint(hWnd, &psd, &lfPrt);
      return 0;

    case IDM_FILE_EXIT: // Wird nur bei Datei|Beenden angesprungen
      PostMessage(hWnd, WM_CLOSE, 0, 0);
      return 0;

    #ifdef BUILD_EM

    case IDM_EDIT_UNDO:
      SendMessage(hWinEdit, WM_UNDO, 0, 0);
      return 0;

    case IDM_EDIT_CUT:
      SendMessage(hWinEdit, WM_CUT, 0, 0);
      return 0;

    case IDM_EDIT_COPY:
      SendMessage(hWinEdit, WM_COPY, 0, 0);
      return 0;

    case IDM_EDIT_PASTE:
      SendMessage(hWinEdit, WM_PASTE, 0, 0);
      return 0;

    case IDM_EDIT_CLEAR:
      SendMessage(hWinEdit, WM_CLEAR, 0, 0);
      return 0;

    #endif

    case IDM_EDIT_SELALL:
      SendMessage(hWinEdit, EM_SETSEL, 0, -1);
      return 0;

    case IDM_EDIT_INSDATE:
      if (GetDateFormat(LOCALE_USER_DEFAULT, DATE_LONGDATE, NULL, NULL, s, sizeof s/sizeof s[0]))
        SendMessage(hWinEdit, EM_REPLACESEL, TRUE, (LPARAM)s);
      return 0;

    #ifdef BUILD_FR
    case IDM_EDIT_FIND: case IDM_EDIT_REPL:
      fr_hDlg = fr_dlg(hWnd, LOWORD(wParam)==IDM_EDIT_REPL);
      return 0;

    case IDM_EDIT_NEXT: // F3
      if (fr_getFindStr()[0] == TEXT('\0'))
        fr_hDlg = fr_dlg(hWnd, 0);
      else
        if(!fr_find(hWinEdit))
          ut_messageBoxExt(hWnd, TEXT("Kein weiterer Suchtext '%s' gefunden."), appTitle, MB_OK, fr_getFindStr());
      return 0;
    #endif

    case IDM_OPTION_FONTPRT:
      if (!menuOptionsFont(hWinEdit, &lfPrt, CF_PRINTERFONTS))
        break;
      return 0;

    case IDM_OPTION_FONTSCR:
      if (!menuOptionsFont(hWinEdit, &lfScr, CF_SCREENFONTS))
      // Befuellt ueber ChooseFont() cf und logFont. Ueber cf.rgbColors koennte aus FontDialog benutzergewaehlte
      // Textfarbe aktiviert werden; der Dialog bietet aber nur 16 Grundfarben, daher nicht genutzt.
        break; // Achtung! Falls i.O., muss Code hier durchlaufen!

    case IDM_OPTION_FONTDEC: case IDM_OPTION_FONTINC:
      {
      HFONT hFontNew;
      if (LOWORD(wParam) == IDM_OPTION_FONTDEC)
        lfScr.lfHeight = (8 * lfScr.lfHeight) / 10;
      else if (LOWORD(wParam) == IDM_OPTION_FONTINC)
        lfScr.lfHeight = (10 * lfScr.lfHeight) / 8;
      hFontNew = CreateFontIndirect(&lfScr);
      SendMessage(hWinEdit, WM_SETFONT, (WPARAM)hFontNew, (LPARAM)TRUE);
      DeleteObject(hFont);
      hFont = hFontNew;
      if (padding)
        SendMessage(hWnd, WM_SIZE, 0, 0);
      return 0;
      }

    case IDM_OPTION_PRINTPAGENO:
      printPageNo = !printPageNo;
      return 0;

    #ifdef BUILD_UC

    case IMD_OPTION_COLORMODE:
      nightMode = !nightMode;
      if (nightMode) {
        bgClr = bgClr2; fgClr = fgClr2; }
      else {
        bgClr = bgClr1; fgClr = fgClr1; }
      #ifdef BUILD_RE
      cr.crTextColor = fgClr;
      SendMessage(hWinEdit, EM_SETCHARFORMAT, (WPARAM)(UINT)SCF_ALL, (LPARAM)(CHARFORMAT*)&cr);
      SendMessage(hWinEdit, EM_SETBKGNDCOLOR, (WPARAM)0, (LPARAM)bgClr);
      #else
      InvalidateRect(hWinEdit, 0, TRUE);
      #endif
      return 0;

    case IDM_OPTION_FGCOL: // Funktioniert nicht ohne Behandlung von WM_CTLCOLOREDIT
      if (ChooseColor(&choosecolor)) {
        fgClr = choosecolor.rgbResult;
        if (nightMode)
          fgClr2 = fgClr;
        else
          fgClr1 = fgClr;
        #ifdef BUILD_RE
        cr.crTextColor = fgClr;
        SendMessage(hWinEdit, EM_SETCHARFORMAT, (WPARAM)(UINT)SCF_ALL, (LPARAM)(CHARFORMAT*)&cr);
        #else
        InvalidateRect(hWinEdit, 0, FALSE);
        #endif
        }
      return 0;

    case IDM_OPTION_BGCOL: // Funktioniert nicht ohne Behandlung von WM_CTLCOLOREDIT
      if (ChooseColor(&choosecolor)) {
        bgClr = choosecolor.rgbResult;
        if (nightMode)
          bgClr2 = bgClr;
        else
          bgClr1 = bgClr;
        #ifdef BUILD_RE
        SendMessage(hWinEdit, EM_SETBKGNDCOLOR, (WPARAM)0, (LPARAM)bgClr);
        #else
        InvalidateRect(hWinEdit, 0, TRUE);
        #endif
        }
      return 0;

    #endif

    #ifdef BUILD_TB
    case IDM_OPTION_SHOWTOOL:
      showWinTool = !showWinTool;
      ShowWindow(hWinTool, showWinTool ? SW_SHOW : SW_HIDE);
      SendMessage(hWnd, WM_SIZE, 0, 0); // Ruft WM_SIZE auf
      return 0;
    #endif

    case IDM_OPTION_SHOWSTAT:
      showWinStat = !showWinStat;
      ShowWindow(hWinStat, showWinStat ? SW_SHOW : SW_HIDE);
      SendMessage(hWnd, WM_SIZE, 0, 0); // Ruft WM_SIZE auf
      return 0;

    case IDM_OPTION_SAVE:
      saveOptions =! saveOptions;
      return 0;

    case IDM_OPTION_SHOWFULL:
      {
      static BOOL showFull, saveShowWinTool, saveShowWinStat;
      static HMENU hMenu;
      LONG winStyle;
      showFull = !showFull;
      // Sowohl SetMenu() als auch ShowWindow(hWnd) rufen WM_SIZE auf (vorausgesetzt, die Aufrufe erfolgen
      // verzoegert, sonst reduziert Windows die 2 WM_SIZE-Nachrichten auf 1), so dass ein
      // SendMessage(hWnd, WM_SIZE, 0, 0) unnoetig ist
      if (showFull) {
        saveShowWinTool = showWinTool; saveShowWinStat = showWinStat;  // Sichere alten Status von
        showWinTool = showWinStat = FALSE; //  Statusbar und Toolbar und setze neuen (benoetigt von WM_SIZE)
        #ifdef BUILD_TB
        ShowWindow(hWinTool, SW_HIDE);
        #endif
        ShowWindow(hWinStat, SW_HIDE);
        ShowScrollBar(hWinEdit, SB_VERT, FALSE);
        hMenu = GetMenu(hWnd);
        SetMenu(hWnd, NULL);
        ShowWindow(hWnd, SW_SHOWMAXIMIZED);
        winStyle = GetWindowLong(hWnd, GWL_STYLE) & ~WS_CAPTION;
        SetWindowLong(hWnd, GWL_STYLE, winStyle);
        SetWindowPos(hWnd, 0, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE);
        }
      else {
        showWinTool = saveShowWinTool; showWinStat = saveShowWinStat;
        #ifdef BUILD_TB
        ShowWindow(hWinTool, showWinTool ? SW_SHOW : SW_HIDE);
        #endif
        ShowWindow(hWinStat, showWinStat ? SW_SHOW : SW_HIDE);
        ShowScrollBar(hWinEdit, SB_VERT, TRUE);
        SetMenu(hWnd, hMenu);
        ShowWindow(hWnd, SW_RESTORE);
        winStyle = GetWindowLong(hWnd, GWL_STYLE) | WS_CAPTION;
        SetWindowLong(hWnd, GWL_STYLE, winStyle);
        SetWindowPos(hWnd, 0, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE);
        }
      return 0;
      }

    case IDM_HELP_VER:
      DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_VER), hWnd, verDlgProc);
      return 0;

    case IDM_HELP_COMMANDS:
      MessageBox(hWnd, TEXT(
      "Strg N\tNeue Datei\n"
      "Strg O\tDatei öffnen\n"
      "Strg S\tDatei schließen\n"
      "Strg P\tDatei drucken\n"
      "Alt F4\tBeenden\n"
      "\n"
      "Alt Rück\tRückgängig\n"
      "Strg Z\tRückgängig\n"
      "Strg X\tAusschneiden\n"
      "Sh Entf\tAusschneiden\n"
      "Strg C\tKopieren\n"
      "Strg Einf\tKopieren\n"
      "Strg V\tEinfügen\n"
      "Sh Einf\tEinfügen\n"
      "Entf\tLöschen\n"
      "Rück\tLöschen\n"
      "Strg F\tSuchen\n"
      "F3\tWeitersuchen\n"
      "Strg R\tErsetzen\n"
      "Strg A\tAlles markieren\n"
      "Strg D\tDatum einfügen\n"
      "\n"
      "F2\tFarbwechsel\n"
      "F11\tVollbild an/aus\n"
      "Strg +\tSchrift vergrößern\n"
      "Strg -\tSchrift verkleinern\n"
      "Esc\tFlüchten\n"
      "F1\tHilfe"
      ), TEXT("Tastenkürzel"), MB_OK);
      return 0;

    #ifdef BUILD_ST
    case IDM_HIDE:
      st_AddIcon(hWnd, IDI_ICON, LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON)), appTitle);
      ShowWindow(hWnd, SW_HIDE);
      return 0;
    #endif

    } // LOWORD(wParam)
    break; // WM_COMMAND

  #ifdef BUILD_FR
  default: // Hier treffen durch fr_dlg() erzeugte Nachrichten fuer fr_find*() ein; lParam haelt FR struct vor, hier nicht mehr benutzt
    if (msg == fr_msg) {
      int frCode = fr_dispatch(hWinEdit);
      switch(frCode) {
        case -3: fr_hDlg = NULL; break;
        case -2: MessageBox(hWnd, TEXT("Kein weiterer Suchtext gefunden."), appTitle, MB_OK); break;
        case -1: break;
        default: ut_messageBoxExt(hWnd, TEXT("%d Ersetzungen."), appTitle, MB_OK, frCode);
        }
      return 0;
      }
    break;
  #endif

  } // msg

  return DefWindowProc(hWnd, msg, wParam, lParam);
  }

/** Function: Main **/
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR lpCmdLn, int nCmdShow)
  {
  HWND hWinMain;
  WNDCLASSEX classMain;
  MSG msg;
  #ifdef BUILD_AC
  HACCEL hAccel;
  #endif

  InitCommonControls();

  /* Erstelle Hauptfensterklasse und verbinde sie mit Callback-Funktion von Hauptfenster */
  ZeroMemory(&classMain, sizeof classMain);
  classMain.cbSize = sizeof classMain;
  classMain.lpfnWndProc = winProc;
  classMain.hInstance = hInst;
  classMain.hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON));
  classMain.hCursor = LoadCursor(NULL, IDC_ARROW);
  // classMain.hbrBackground = (HBRUSH)(COLOR_WINDOW+1); hWinEdit, hWinTool und hWinStat zeichnen selbst
  classMain.lpszMenuName = MAKEINTRESOURCE(IDM_MENU);
  classMain.lpszClassName = appTitle;
  classMain.hIconSm =
    (HICON)LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON), IMAGE_ICON, 16, 16, 0);
  if (!RegisterClassEx(&classMain)) {
    #ifdef UNICODE
    MessageBox(NULL, TEXT("Programm arbeitet mit Unicode und setzt Windows NT voraus!"), appTitle, MB_ICONSTOP);
    #else
    MessageBox(NULL, TEXT("Kann Fensterklasse nicht registrieren!"), appTitle, MB_ICONSTOP);
    #endif
    return 1;
    }

  /* Erstelle Hauptfenster */
  hWinMain = CreateWindowEx(WS_EX_ACCEPTFILES, appTitle, appTitle,
    WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
    CW_USEDEFAULT, NULL, NULL, hInst, NULL);
  if (hWinMain == NULL) {
    MessageBox(NULL, TEXT("Kann Hauptfenster nicht laden!"), appTitle, MB_ICONSTOP);
    return 1;
    }
  ShowWindow(hWinMain, nCmdShow);
  UpdateWindow(hWinMain);

  #ifdef BUILD_AC
  if ((hAccel=LoadAccelerators(hInst, MAKEINTRESOURCE(IDA_ACCEL))) == NULL)
    MessageBox(NULL, TEXT("Kann Accelerators nicht laden - Resourcen gebunden?"), appTitle, MB_ICONWARNING);
  #endif

  /* Empfange Nachrichten der Dispatcher-Schleife */
  while (GetMessage(&msg, NULL, 0, 0) > 0)
    #ifdef BUILD_FR
    if (fr_hDlg == NULL || !IsDialogMessage(fr_hDlg, &msg)) // Somit Tab und ENTER durch Dialog statt durch winProc behandelt
    #endif
      #ifdef BUILD_AC
      if (!TranslateAccelerator(hWinMain, hAccel, &msg))
      #endif
        {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        }

  #ifdef BUILD_RE
  if (richEditLib)
    FreeLibrary(richEditLib); // Stuerzte innerhalb WM_DESTROY immer ab; am Main-Ende aber problemlos
  #endif

  return msg.wParam;
  }
