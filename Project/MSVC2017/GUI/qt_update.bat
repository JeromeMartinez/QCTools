mkdir _Automated
C:\Qt\5.9.9\msvc2017_64\bin\qmake ..\..\QtCreator\QCTools.pro > nul
nmake /NOLOGO /f Makefile.debug compiler_uic_make_all compiler_rcc_make_all mocables

move /Y ui_*.h _Automated
move /Y debug\* _Automated
