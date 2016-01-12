/** Editor ED - Header FR: Suchen und Ersetzen **/

#ifndef __FR_H__
#define __FR_H__


extern HWND fr_hDlg;

HWND fr_dlg(HWND hWnd, BOOL replDlg);
BOOL fr_find(HWND hWinEdit);
TCHAR *fr_getFindStr(void);
int fr_dispatch(HWND hWinEdit);
void fr_setFocusLost(void);


#endif
