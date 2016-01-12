/** Editor ED - Header TOP: Kompilatsteuerung **/

#ifndef __TOP_H__
#define __TOP_H__


/* Unicode oder ANSI */
#define NO_UNICODE

/* Kompilierbereich */
#define BUILD_TB // Toolbar, funktioniert nicht unter LCC V3.8
#define BUILD_UC // Benutzerdefierbare Farben
#define BUILD_AC // Shortcuts
#define BUILD_EM // Editmenue (statt nur Shortcuts und Kontextmenue)
#define BUILD_FR // Suchen/Ersetzen
#define BUILD_FR_PR // Belege Suchtext im Dialog mit evt. Textmarkierung vor
#define BUILD_ST // Verkleinerung in Systemtray
#define BUILD_RO // Graues Nurlesefenster fuer R/O-Datei
#define NO_BUILD_RE VER02 // Richedit statt MultilineEdit: VER01, VER02 oder nicht definiert

/* C-RTL Fix */
#if defined(UNICODE) // UNICODE aktiviert unter Borland Breitfassung NUR der Windows-Funktionen.
#define _UNICODE // Borland aktiviert mit _UNICODE Breitfassung der eigenen RTL-Funktionen ( _tcsrchr -> wcsrchr),
#endif // setzt zusaetzlich UNICODE und damit Breitfassung der Windows-Funktionen (lstrlen -> lstrlenW).
// Unter MSC muessen ebf. beide Schalter gesetzt werden


#endif
