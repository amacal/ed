/** Editor ED - Header ST: System Tray Management **/

#ifndef __ST_H__
#define __ST_H__


BOOL st_AddIcon(HWND hWnd, UINT uID, HICON hicon, const TCHAR *lpszTip);
BOOL st_DelIcon(HWND hWnd, UINT uID);


#endif
