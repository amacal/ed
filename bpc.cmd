rem Kompilierung unter Pelles C fuer Windows V. 7.00.355
cc /Ze /Gz /Os ed.c fr.c st.c ut.c ed.rc user32.lib gdi32.lib comdlg32.lib shell32.lib comctl32.lib
del *.obj