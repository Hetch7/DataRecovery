# Microsoft Developer Studio Project File - Name="DataRecovery" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Application" 0x0101

CFG=DataRecovery - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "DataRecovery.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "DataRecovery.mak" CFG="DataRecovery - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "DataRecovery - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "DataRecovery - Win32 Debug" (based on "Win32 (x86) Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "DataRecovery - Win32 Release"

# PROP BASE Use_MFC 6
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir ".\Release"
# PROP BASE Intermediate_Dir ".\Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 6
# PROP Use_Debug_Libraries 0
# PROP Output_Dir ".\Release"
# PROP Intermediate_Dir ".\Release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MD /W3 /O2 /Ob1 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_AFXDLL" /D "_MBCS" /GF /Gy /Yu"stdafx.h" /Fp".\Release/DataRecovery.pch" /Fo".\Release/" /Fd".\Release/" /FR /c /GX 
# ADD CPP /nologo /MD /W3 /O2 /Ob1 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_AFXDLL" /D "_MBCS" /GF /Gy /Yu"stdafx.h" /Fp".\Release/DataRecovery.pch" /Fo".\Release/" /Fd".\Release/" /FR /c /GX 
# ADD BASE MTL /nologo /D"NDEBUG" /mktyplib203 /tlb".\Release\DataRecovery.tlb" /win32 
# ADD MTL /nologo /D"NDEBUG" /mktyplib203 /tlb".\Release\DataRecovery.tlb" /win32 
# ADD BASE RSC /l 1033 /d "NDEBUG" 
# ADD RSC /l 1033 /d "NDEBUG" 
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo 
# ADD BSC32 /nologo 
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /out:".\Release\DataRecovery.exe" /incremental:no /pdb:".\Release\DataRecovery.pdb" /pdbtype:sept /subsystem:windows /machine:ix86 
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /out:".\Release\DataRecovery.exe" /incremental:no /pdb:".\Release\DataRecovery.pdb" /pdbtype:sept /subsystem:windows /machine:ix86 

!ELSEIF  "$(CFG)" == "DataRecovery - Win32 Debug"

# PROP BASE Use_MFC 6
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir ".\Debug"
# PROP BASE Intermediate_Dir ".\Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 6
# PROP Use_Debug_Libraries 1
# PROP Output_Dir ".\Debug"
# PROP Intermediate_Dir ".\Debug"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MDd /ZI /W3 /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_AFXDLL" /D "_MBCS" /EHsc /Yu"stdafx.h" /Fp".\Debug/DataRecovery.pch" /Fo".\Debug/" /Fd".\Debug/" /FR /GZ /c /GX 
# ADD CPP /nologo /MDd /ZI /W3 /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_AFXDLL" /D "_MBCS" /EHsc /Yu"stdafx.h" /Fp".\Debug/DataRecovery.pch" /Fo".\Debug/" /Fd".\Debug/" /FR /GZ /c /GX 
# ADD BASE MTL /nologo /D"_DEBUG" /mktyplib203 /tlb".\Debug\DataRecovery.tlb" /win32 
# ADD MTL /nologo /D"_DEBUG" /mktyplib203 /tlb".\Debug\DataRecovery.tlb" /win32 
# ADD BASE RSC /l 1041 /d "_DEBUG" 
# ADD RSC /l 1041 /d "_DEBUG" 
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo 
# ADD BSC32 /nologo 
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib shlwapi.lib /nologo /out:".\Debug\DataRecovery.exe" /incremental:yes /debug /pdb:".\Debug\DataRecovery.pdb" /pdbtype:sept /subsystem:windows /machine:ix86 
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib shlwapi.lib /nologo /out:".\Debug\DataRecovery.exe" /incremental:yes /debug /pdb:".\Debug\DataRecovery.pdb" /pdbtype:sept /subsystem:windows /machine:ix86 

!ENDIF

# Begin Target

# Name "DataRecovery - Win32 Release"
# Name "DataRecovery - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=DataRecovery.cpp

!IF  "$(CFG)" == "DataRecovery - Win32 Release"

# ADD CPP /nologo /O2 /Yu"stdafx.h" /FR /GX 
!ELSEIF  "$(CFG)" == "DataRecovery - Win32 Debug"

# ADD CPP /nologo /Od /Yu"stdafx.h" /FR /GZ /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=DataRecovery.rc
# End Source File
# Begin Source File

SOURCE=.\DataRecoveryDlg.cpp
# End Source File
# Begin Source File

SOURCE=.\LinkStatic.cpp
# End Source File
# Begin Source File

SOURCE=.\LoadMFTDlg.cpp
# End Source File
# Begin Source File

SOURCE=MFTRecord.cpp

!IF  "$(CFG)" == "DataRecovery - Win32 Release"

# ADD CPP /nologo /O2 /Yu"stdafx.h" /FR /GX 
!ELSEIF  "$(CFG)" == "DataRecovery - Win32 Debug"

# ADD CPP /nologo /Od /Yu"stdafx.h" /FR /GZ /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=NTFSDrive.cpp

!IF  "$(CFG)" == "DataRecovery - Win32 Release"

# ADD CPP /nologo /O2 /Yu"stdafx.h" /FR /GX 
!ELSEIF  "$(CFG)" == "DataRecovery - Win32 Debug"

# ADD CPP /nologo /Od /Yu"stdafx.h" /FR /GZ /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=StdAfx.cpp

!IF  "$(CFG)" == "DataRecovery - Win32 Release"

# ADD CPP /nologo /O2 /Yc"stdafx.h" /FR /GX 
!ELSEIF  "$(CFG)" == "DataRecovery - Win32 Debug"

# ADD CPP /nologo /Od /Yc"stdafx.h" /FR /GZ /GX 
!ENDIF

# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=DataRecovery.h
# End Source File
# Begin Source File

SOURCE=DataRecoveryDlg.h
# End Source File
# Begin Source File

SOURCE=.\LinkStatic.h
# End Source File
# Begin Source File

SOURCE=.\LoadMFTDlg.h
# End Source File
# Begin Source File

SOURCE=MFTRecord.h
# End Source File
# Begin Source File

SOURCE=NTFSDrive.h
# End Source File
# Begin Source File

SOURCE=Resource.h
# End Source File
# Begin Source File

SOURCE=StdAfx.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=res\DataRecovery.rc2
# End Source File
# Begin Source File

SOURCE=.\res\icon1.ico
# End Source File
# End Group
# Begin Source File

SOURCE=.\res\DataRecovery.manifest
# End Source File
# Begin Source File

SOURCE=ReadMe.txt
# End Source File
# End Target
# End Project

