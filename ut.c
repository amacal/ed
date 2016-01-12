/** Editor ED - Modul UT: Utensilien **/

#include "top.h"
#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#include <stdarg.h>

#define SWAPLONG(l) (((l) >> 16) | ((l) & 0x00FF00) | (((l) & 0x0000FF) << 16))

/** Function: Kopiere Markierung im EDIT-Control **/
TCHAR *ut_getEditText(HWND hWinEdit, int *len)
  {
  int bufLen;
  TCHAR *buf;
  if (len)
    *len = 0;
  if ((bufLen=GetWindowTextLength(hWinEdit)) == 0)
    return NULL;
  if ((buf=(TCHAR*)GlobalAlloc(GMEM_FIXED, (bufLen+1)*sizeof(TCHAR))) == NULL)
    return NULL; // Leider muss erst _gesamter_ Text kopiert werden, um Markierung zu erhalten
  GetWindowText(hWinEdit, buf, bufLen+1); // Selbst wenn Text aus Datei mit SetWindowTextA() auf ANSI
  if (len) // fixiert eingelesen wurde, befuellt GetWindowText()
    *len = bufLen;
  return buf; // unter UNICODE (GetWindowTextW) Puffer mit Unicode-Text - daher funktionieren Suchroutinen!
  }

/** Function: Kopiere Markierung von maximal maxLen im EDIT-Control **/
TCHAR *ut_getEditSel(HWND hWinEdit, int *len)
  {
  int sos, eos, selLen;
  TCHAR *buf, *sel;
  if (len)
    *len = 0;
  SendMessage(hWinEdit, EM_GETSEL, (WPARAM)&sos, (LPARAM)&eos);
  if ((selLen=eos-sos) == 0)
    return NULL;
  if ((buf=ut_getEditText(hWinEdit, NULL)) == NULL)
    return NULL;
  if ((sel=(TCHAR*)GlobalAlloc(GMEM_FIXED, (selLen+1)*sizeof(TCHAR))) == NULL) {
    GlobalFree(buf); return NULL; }
  lstrcpyn(sel, buf+sos, selLen+1); // Unterschied zu strncpy: lstrcpyn kopiert nur n Zeichen, wenn n+1 angegeben,
  GlobalFree(buf); // um Null mitanzuhaengen!
  if (len)
    *len = selLen;
  return sel;
  }

/** Function: Erweiterte MessageBox-Funktion **/
int ut_messageBoxExt(HWND hWnd, const TCHAR *msg, const TCHAR *title, unsigned type, ...)
  {
  va_list varg;
  TCHAR s[256];
  va_start(varg, type); // Muss auf letztes Fixarg zeigen
  _vsntprintf(s, sizeof s - 1, msg, varg); // Es gibt kein Windowspendant wvsnprintf() mit num. Begrenzung
  va_end(varg); // LCC-Warn. ignorieren
  s[sizeof s / sizeof(TCHAR) - 1] = TEXT('\0'); // vsnprintf schreibt keine 0, falls string >= nsize, daher nachgeholt
  return MessageBox(hWnd, s, title, type);
  }

/** Function: Wandele Zahl in Zeichenkette **/
TCHAR *ut_uLong2Str(unsigned long l, unsigned int radix)
  {
  static TCHAR s[13]; // Muss 13 sein, da printf Zahlen nicht trunkieren kann (Strings schon)!
  s[0] = 0;
  if (radix == 16)
    wsprintf(s, TEXT("%06lX"), l); // _ultot(l,s,16) erlaubt leider keine Formatbreite
  else if (radix == 10)
    wsprintf(s, TEXT("%ld"), l);
  return s;
  }

#ifdef BUILD_UC

/** Function: Wandele RGB-Zeichenkette in BGR-Zahl **/
unsigned long ut_revRGBVal(TCHAR *s)
  {
  unsigned long l = _tcstoul(s, NULL, 16); // Es gibt kein lstrtoul() in Windows
  return SWAPLONG(l); // RGB -> BGR
  }

/** Function: Wandele BGR-Zahl in RGB-Zeichenkette **/
TCHAR *ut_revRGBStr(unsigned long l)
  {
  return ut_uLong2Str(SWAPLONG(l),16); // BGR -> RGB
  }

#endif

/** Function: Ermittele Kommandozeilenargumente **/
BOOL ut_getArg(TCHAR s[], TCHAR arg[])
  {
  static TCHAR *psave;
  TCHAR stop, *p;

  /* Initialisiere */
  if (!arg) {
    if (!s)
      return FALSE;
    psave = s;
    return FALSE;
    }
  if (!psave) // Von nun an MUSS psave ein gueltiger Zeiger sein!
    return FALSE;
  /* Fahre an letzter Suchposition fort */
  *arg = TEXT('\0');
  p = psave;
  /* Ueberspringe fuehrende Leerzeichen */
  while (*p == TEXT(' '))
    ++p;
  if (!*p) {
    psave = NULL; return FALSE; }
  /* Ermittele Endezeichen */
  if (*p == TEXT('"')) {
    stop = TEXT('"'); ++p; }
  else
    stop = TEXT(' ');
  /* Merke Wortbeginn */
  psave = p;
  /* Laufe bis Endezeichen */
  do ++p; while (*p && *p != stop);
  /* Kopiere Zeichenkette */
  lstrcpyn(arg, psave, p-psave+1); // Im Us. zu strncpy kopiert lstrcpyn nur n Zeichen, wenn n+1 angegeben,
  if (!*p) // um 0 mitanzuhaengen!
    psave = p;
  else
    psave = p + 1;
  return TRUE;
  }

/** Function: Ergibt TRUE, wenn path als Ordner existiert **/
BOOL ut_dirExists(const TCHAR path[])
  {
  DWORD h;
  if (!path || path[0] == '\0')
    return FALSE;
  h = GetFileAttributes(path);
  if (h == 0xFFFFFFFF)
    return FALSE;
  return (BOOL) (h & FILE_ATTRIBUTE_DIRECTORY);
  }

/** Function: Ergibt TRUE, wenn path als Datei existiert **/
BOOL ut_fileExists(const TCHAR path[])
  {
  DWORD h;
  if (!path || path[0] == '\0')
    return FALSE;
  h = GetFileAttributes(path);
  if (h == 0xFFFFFFFF)
    return FALSE;
  return (BOOL) !(h & FILE_ATTRIBUTE_DIRECTORY);
  /* Oder:
  WIN32_FIND_DATA data;
  HANDLE handle = FindFirstFile(dirName,&data);
  if (handle != INVALID_HANDLE_VALUE) {
    FindClose(handle);
    return true;
    }
  return false;
  */
  }

#ifdef BUILD_FR

/** Function: Suche Zeichenkette in Zeichenkette ohne Beachtung von Gross- und Kleinschreibung **/
static TCHAR *ut_strstri(const TCHAR *txt, const TCHAR *pat)
  {
  // CharUpper() wandelt sowohl Zeichenketten als auch Zeichen um; da aber Funktion als
  // LPTSTR CharUpper(LPTSTR lpsz) deklariert ist, muessen wir Funktionsergebnis fuer Zeichen in TCHAR*
  // umwandeln, damit Kompiler Ruhe gibt. Da, wo wir nur CharUpper-Ergebnisse vergleichen,
  // kann allerdings Umwandlung des Funktionsergebnisses entfallen. Ausserdem muessen wir Suchzeichen
  // in WORD umwandeln, damit keine neg. Vorzeichenerw. passiert ('ü' -> int -4).
  TCHAR *htxt1, *hpat2, *htxt2, patchar1;
  /* Initialisiere */
  if (txt == NULL || pat == NULL || *pat == TEXT('\0'))
    return NULL;
  patchar1 = (TCHAR)CharUpper((TCHAR*)(WORD)(*pat)); // LCC  ignorieren - CharUpper ergibt 32bit-Zeichen,
  for (htxt1=(TCHAR*)txt; *htxt1; htxt1++) { // falls Parameter Zeichen ist
    /* Ermittele, ob erstes Suchmusterzeichen enthalten ist */
    while (*htxt1 && (TCHAR)CharUpper((TCHAR*)(WORD)*htxt1) != patchar1) // LCC ignorieren oder _totupper()
      htxt1++;
    if (!*htxt1)
      return NULL;
    hpat2 = (TCHAR*)pat;
    htxt2 = (TCHAR*)htxt1;
    /* Ermittele, ob auch die nachfolgenden Suchmusterzeichen enthalten sind */
    while (CharUpper((TCHAR*)(WORD)*htxt2) == CharUpper((TCHAR*)(WORD)*hpat2)) {
      htxt2++;
      hpat2++;
      /* Falls das Ende des Suchmusters erreicht wurde, ist der Fund bestaetigt */
      if (!*hpat2)
        return htxt1;
      }
    }
  return NULL;
  }

/** Function: Suche Zeichenkette in Zeichenkette **/
TCHAR *ut_strstr(const TCHAR *txt, const TCHAR *pat, BOOL matchCase, BOOL wholeWord)
  {
  TCHAR *h = (TCHAR*)txt;
  UINT patlen = lstrlen(pat);
  while ((h = matchCase ? _tcsstr(h,pat) : ut_strstri(h,pat)) != NULL) { // Es gibt kein lstrstr() in Windows
    if (!wholeWord || ((h==txt || !IsCharAlpha(*(h-1))) && !IsCharAlpha(*(h+patlen))))
      break;
    h += patlen;
    }
  return h;
  }


#endif
