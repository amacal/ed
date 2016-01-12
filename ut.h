/** Editor ED - Header UT: Utensilien **/

#ifndef __UT_H__
#define __UT_H__


int ut_messageBoxExt(HWND hWnd, const TCHAR *msg, const TCHAR *title, unsigned type, ...);
TCHAR *ut_getEditText(HWND hWinEdit, int *len);
TCHAR *ut_getEditSel(HWND hWinEdit, int *len);
TCHAR *ut_uLong2Str(unsigned long l, unsigned int radix);
unsigned long ut_revRGBVal(TCHAR *s);
TCHAR *ut_revRGBStr(unsigned long l);
BOOL ut_getArg(TCHAR s[], TCHAR arg[]);
BOOL ut_dirExists(const TCHAR path[]);
BOOL ut_fileExists(const TCHAR path[]);
TCHAR *ut_strstri(const TCHAR *txt, const TCHAR *pat);
TCHAR *ut_strstr(const TCHAR *txt, const TCHAR *pat, BOOL matchCase, BOOL wholeWord);


#endif
