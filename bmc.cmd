rem Kompilierung unter Visual C 2008
rc /i"c:\programme\microsoft sdks\windows\v6.0a\include;c:\programme\vcpp\vc\include" /fo ed.res ed.rc
cl /GF /I"c:\programme\microsoft sdks\windows\v6.0a\include" /Ic:\programme\vcpp\vc\include ed.c fr.c st.c ut.c ed.res /link /LIBPATH:"c:\programme\microsoft sdks\windows\v6.0a\lib" /LIBPATH:c:\programme\vcpp\vc\lib user32.lib gdi32.lib comctl32.lib comdlg32.lib shell32.lib comctl32.lib
del *.obj