/** Editor ED - Modul ST: System Tray Management **/

#include "top.h"

#ifdef BUILD_ST

#include <windows.h>
#ifdef __LCC__
#include "lcc.h"
#endif
#include "res.h"

/** Function: Fuege Anwendungssymbol zum Taskleistenstatusfenster hinzu **/
BOOL st_AddIcon(HWND hWnd, UINT uID, HICON hicon, const TCHAR *lpszTip)
  {
  BOOL ret;
  NOTIFYICONDATA tnid;

  tnid.cbSize = sizeof(NOTIFYICONDATA);
  tnid.hWnd = hWnd; // Handle des Fensters, welches Callback-Nachrichten erhalten soll
  tnid.uID = uID; // Symbol-Bezeichner
  tnid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
  tnid.uCallbackMessage = IDM_UNHIDE;
  tnid.hIcon = hicon; // Symbol-Handle
  if (lpszTip) // Tooltip-Text
    lstrcpyn(tnid.szTip, lpszTip, sizeof tnid.szTip); // lpszTip ist auf 64b beschraenkt
  else
    tnid.szTip[0] = TEXT('\0');
  ret = Shell_NotifyIcon(NIM_ADD, &tnid);
  if (hicon)
    DestroyIcon(hicon);
  return ret; // TRUE, falls erfolgreich
  }

/** Function: Entferne Anwendungssymbol von Taskleistenstatusfenster **/
BOOL st_DelIcon(HWND hWnd, UINT uID)
  {
  BOOL ret;
  NOTIFYICONDATA tnid;

  tnid.cbSize = sizeof(NOTIFYICONDATA);
  tnid.hWnd = hWnd; // Handle des Fensters, welches Symbol hinzufuegte
  tnid.uID = uID; // Symbol-Bezeichner
  ret = Shell_NotifyIcon(NIM_DELETE, &tnid);
  return ret; // TRUE, falls erfolgreich
  }

#endif
