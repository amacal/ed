rem Kompilierung unter BCC V5.5.1
bcc32 -Ic:\programme\ide\bc\include -c -d ed.c ut.c fr.c st.c
brcc32 -ic:\programme\ide\bc\include -foed.res ed.rc
ilink32 -Lc:\programme\ide\bc\lib;c:\programme\ide\bc\lib\psdk -c -aa -Gn c0w32+ed+ut+fr+st, ed, nul, import32.lib+cw32.lib+comctl32.lib,, ed.res
del *.obj *.res *.tds