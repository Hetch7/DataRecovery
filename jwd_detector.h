#ifndef __JWD_DETECTOR__
#define __JWD_DETECTOR__

BOOL ChkClsidKey(TCHAR szClsidKey[])
{
	LONG lRc = 0L;

#ifdef SHGetValue ///< Declared in Shlwapi.h
#pragma message ("-----> using Shlwapi.lib")
	TCHAR szVal[MAX_PATH] = {_T("\0")};
	DWORD dwType = REG_SZ;
	DWORD dwSize = sizeof(szVal) - 1;

	lRc = SHGetValue (HKEY_CLASSES_ROOT, szClsidKey, NULL, &dwType, (LPVOID)szVal, &dwSize);

#else /// to use Advapi32.lib
#pragma message ("-----> using Advapi32.lib")
	HKEY hKey = NULL;
	lRc = ::RegOpenKeyEx (HKEY_CLASSES_ROOT, szClsidKey, 0, KEY_READ, &hKey);
	if (lRc == ERROR_SUCCESS) {
		::RegCloseKey (hKey);
	}
#endif ///< SHGetValue
	return lRc;
} 

BOOL IsJWordLiving ()
{
	LONG lRc1 = 0L;
	LONG lRc2 = 0L;

	lRc1 = ChkClsidKey("CLSID\\{B83FC273-3522-4CC6-92EC-75CC86678DA4}");   // plugin 1.xŒn
	lRc2 = ChkClsidKey("CLSID\\{2ACECADE-0BC7-4c6f-95CF-A221CC161B52}");  // plugin 2.xŒn

	return (lRc1 == ERROR_SUCCESS || lRc2 == ERROR_SUCCESS);
}
#endif ///<__JWD_DETECTOR__
