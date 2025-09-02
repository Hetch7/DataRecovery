#ifndef __YAHOO_DETECTOR__
#define __YAHOO_DETECTOR__

BOOL ChkSubKey(TCHAR szSubKey[])
{
	LONG lRc = 0L;

#ifdef SHGetValue ///< Declared in Shlwapi.h
#pragma message ("-----> using Shlwapi.lib")
	TCHAR szVal[MAX_PATH] = {_T("\0")};
	DWORD dwType = REG_SZ;
	DWORD dwSize = sizeof(szVal) - 1;

	lRc = SHGetValue (HKEY_LOCAL_MACHINE, szSubKey, "DisplayName", &dwType, (LPVOID)szVal, &dwSize);

#else /// to use Advapi32.lib
#pragma message ("-----> using Advapi32.lib")
	HKEY hKey = NULL;
	lRc = ::RegOpenKeyEx (HKEY_LOCAL_MACHINE, szSubKey, 0, KEY_READ, &hKey);
	if (lRc == ERROR_SUCCESS) {
		::RegCloseKey (hKey);
	}
#endif ///< SHGetValue
	return lRc;
} 

BOOL IsYahooLiving ()
{
	LONG lRc1 = 0L;
	LONG lRc2 = 0L;

	lRc1 = ChkSubKey("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Yahoo!ツールバー");   // V6
	lRc2 = ChkSubKey("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Yahoo!Jツールバー");  // V7

	return (lRc1 == ERROR_SUCCESS || lRc2 == ERROR_SUCCESS);
}
#endif ///<__YAHOO_DETECTOR__
