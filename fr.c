/** Editor ED - Modul FR: Suchen und Ersetzen **/

#include "top.h"

#ifdef BUILD_FR

#include <windows.h>
#include "ut.h"
#include "res.h"

#ifdef BUILD_RE

#error Suchfunktionalität sollte besser mit der RICHEDIT-eigenen Suchfunktion implementiert werden!

#else

#define MAX_STRING_LEN 256

HWND fr_hDlg;
static FINDREPLACE fr_dlgStruc;
static TCHAR fr_findStr[MAX_STRING_LEN], fr_replStr[MAX_STRING_LEN];
static int fr_findCt, fr_replCt, fr_focusLost;

#ifdef BUILD_FR_PR

/** Function: Besetze Suchmuster in Suchen-Ersetzen-Dialogbox mit markiertem Text vor **/
static void fr_preset(HWND hWinEdit)
  {
  int sos, eos, selLen;
  TCHAR *buf;
  SendMessage(hWinEdit, EM_GETSEL, (WPARAM)&sos, (LPARAM)&eos);
  if ((selLen=eos-sos) == 0)
    return;
  if (selLen > MAX_STRING_LEN)
    selLen = MAX_STRING_LEN;
  if ((buf=ut_getEditText(hWinEdit,NULL)) == NULL)
    return;
  lstrcpyn(fr_findStr, buf+sos, selLen+1); // Unterschied zu strncpy: lstrcpyn kopiert nur n Zeichen,
  GlobalFree(buf); // wenn n+1 angegeben, um Null mitanzuhaengen!
  }

#endif

/** Function: Rufe Suchen-Ersetzen-Dialogbox auf **/
HWND fr_dlg(HWND hWnd, BOOL replDlg)
  {
  // Der fr_dlg ist nur eine Huelse und kehrt sofort zum Aufrufer zurueck; werden die Buttons betaetigt,
  // werden fr_msg-Nachrichten erzeugt, die die die eigentlichen Arbeitsfunktionen fr_find*() ansteuern.
  // Nachfolgende Aenderungen von Suchoptionen (matchCase etc.) in fr_dlgStruc werden ueber den Dialog sofort an
  // die Variable fr_dlgStruc weitergegeben, sodass die Arbeitsfunktionen immer den aktuellen Zustand haben.
  fr_findCt = fr_replCt = 0;
  #ifdef BUILD_FR_PR
  fr_preset(GetDlgItem(hWnd, IDW_WIN_EDIT)); // fr_FindText mit evt. Markierung vorbesetzen
  #endif
  ZeroMemory(&fr_dlgStruc, sizeof(FINDREPLACE));
  fr_dlgStruc.lStructSize = sizeof(FINDREPLACE);
  fr_dlgStruc.hwndOwner = hWnd; // Bestimmt, an welches Fenster Nachrichten gesendet werden
  fr_dlgStruc.Flags = FR_DOWN | FR_NOUPDOWN; // DOWN voreinstellen und fixieren
  fr_dlgStruc.lpstrFindWhat = fr_findStr;
  fr_dlgStruc.wFindWhatLen = MAX_STRING_LEN;
  if (replDlg) {
    fr_dlgStruc.lpstrReplaceWith = fr_replStr; fr_dlgStruc.wReplaceWithLen  = MAX_STRING_LEN;
    return ReplaceText(&fr_dlgStruc);
    }
  else
    return FindText(&fr_dlgStruc); // Hier wird Dialog aufgerufen; Find- und ReplaceText warten Benutzereingaben
    // in Suchfelder NICHT ab, sondern kehren sofort unter Rueckgabe von hFindDlg zurueck (moduslos).
  }

/** Function: Suche Suchmuster **/
BOOL fr_find(HWND hWinEdit)
  {
  int cursorPos;
  TCHAR *buf, *bufFoundPos;
  // Dokument in Puffer kopieren und dort suchen, da es leider kein direktes Find fuer EDIT-Controls gibt, wohl
  // aber fuer RICHEDIT! Puffer muss auch jedesmal neu angelegt werden, da bei einem moduslosem Dialog
  // der Benutzer Text zwischenzeitlich aendern koennte.
  fr_focusLost = 0;
  if ((buf=ut_getEditText(hWinEdit,NULL)) == NULL)
    return FALSE;
  /* Suche Text im Dokument */
  SendMessage(hWinEdit, EM_GETSEL, 0, (LPARAM)&cursorPos); // CaretPos speichern, von Find*() als Startpos. genutzt
  bufFoundPos = ut_strstr(buf+cursorPos, fr_dlgStruc.lpstrFindWhat, fr_dlgStruc.Flags&FR_MATCHCASE,
    fr_dlgStruc.Flags&FR_WHOLEWORD);
  GlobalFree(buf);
  if (bufFoundPos == NULL) // Abbruch mit Fehlercode, falls erfolglos
    return FALSE;
  /* Sonst Startadresse bestimmen und neu setzen fuer naechste Suche */
  cursorPos = bufFoundPos - buf + lstrlen(fr_dlgStruc.lpstrFindWhat);
  // Auswaehlen des gefundenen Texts
  SendMessage(hWinEdit, EM_SETSEL, bufFoundPos-buf, cursorPos);
  SendMessage(hWinEdit, EM_SCROLLCARET, 0, 0); // Aktualisiere Fenster, so dass Caret sichtbar wird
  ++fr_findCt;
  return TRUE;
  }

/** Function: Ersetze naechstes Suchmuster **/
static BOOL fr_repl(HWND hWinEdit)
  {
  static BOOL noMoreToFind;
  int cursorPos;

  if (noMoreToFind) { // Unterdrueckt die bei letztmoeglicher Ersetzung (unlogische) Meldung: Kein Suchtext gefunden
    noMoreToFind = FALSE; return FALSE; }
  // Falls Ersetzen-Button als 1. gedrueckt, wuerde einfach Cursorposition ersetzt, daher initial 1x fr_find() aufgerufen.
  // Auch falls Benutzer zwischenzeitlich ins WIN_EDIT-Fenster wechselte und Aenderungen vornahmt, muss erst fr_find() aufgerufen werden.
  if (fr_findCt == 0 || fr_focusLost) {
    // Falls Benutzer waehrend aktiven Ersetzen-Fensters ins EDIT-Control wechselt (fr_focusLost), stimmt ggf.
    // die zu ersetzende Markierung nicht mehr und es muss neu gesucht werden. Falls aber nur gewechselt und rueckgewechselt
    // wird, OHNE die Markierung/Cursorpos. zu aendern, wuerde aktuelle Markierung uebersprungen, daher Cursor um findLen rueckgesetzt.
    SendMessage(hWinEdit, EM_GETSEL, 0, (LPARAM)&cursorPos);
    cursorPos -= lstrlen(fr_dlgStruc.lpstrFindWhat);
    if (cursorPos < 0) cursorPos = 0;
    SendMessage(hWinEdit, EM_SETSEL, cursorPos, cursorPos);
    return fr_find(hWinEdit);
    }
  // Markierten Text ersetzen
  SendMessage(hWinEdit, EM_REPLACESEL, TRUE, (LPARAM)fr_dlgStruc.lpstrReplaceWith);
  ++fr_replCt;
  // Naechste Fundstelle lokalisieren und markieren
  noMoreToFind = !fr_find(hWinEdit); // noMoreToFind-Rahmen
  return TRUE;
  }

/** Function: Ersetze alle Suchmuster **/
static UINT fr_replAll(HWND hWinEdit)
  {
  fr_findCt = fr_replCt = 0;
  SendMessage(hWinEdit, EM_SETSEL, 0, 0);
  while (fr_repl(hWinEdit));
  return fr_replCt;
  }

/** Function: Informiere andere Module ueber Suchmuster **/
TCHAR *fr_getFindStr(void)
  {
  return fr_findStr;
  }

/** Function: Dispatcher **/
int fr_dispatch(HWND hWinEdit)
  {
  // Returns:
  // -3 FR_DIALOGTERM
  // -2 No more found
  // -1 Something found
  // 0..n Count of replacements
  if (fr_dlgStruc.Flags & FR_DIALOGTERM)
    return -3;
  else
    if (fr_dlgStruc.Flags & FR_FINDNEXT)
      return fr_find(hWinEdit) ? -1 : -2;
    else
      if (fr_dlgStruc.Flags & FR_REPLACE)
        return fr_repl(hWinEdit) ? -1 : -2;
      else
        if (fr_dlgStruc.Flags & FR_REPLACEALL)
          return fr_replAll(hWinEdit);
        else
          return -4; // Unknown
  }

/** Function: Informiere FR-Modul, dass Benutzer in das Hauptfenster gewechselt war **/
void fr_setFocusLost(void)
  {
  fr_focusLost = 1; // fr_repl() fragt spaeter fr_setFocusLost ab und fr_find setzt es zurueck
  }

#endif


#endif
