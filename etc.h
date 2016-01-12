/** Editor ED - Header ETC: COMCTL V6 fuer BC V5.5.1 **/

// Quelle: MSVC COMMCTRL.H


#define ICC_LINK_CLASS 0x00008000
#define MAX_LINKID_TEXT 48
#define L_MAX_URL_LENGTH (2048 + 32 + sizeof("://"))

typedef struct tagLITEM {
  UINT mask;
  int iLink;
  UINT state;
  UINT stateMask;
  WCHAR szID[MAX_LINKID_TEXT];
  WCHAR szUrl[L_MAX_URL_LENGTH];
  } LITEM, *PLITEM;

typedef struct tagNMLINK {
  NMHDR hdr;
  LITEM item;
  } NMLINK, *PNMLINK;
