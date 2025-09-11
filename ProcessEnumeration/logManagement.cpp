#include <windows.h>
#include <iostream>

using std::string_view;
using std::wstring_view;
using std::string;


string g_Name{ "PersistentService " };
string g_Logs{ R"(C:\Temp\service.log)" };

static std::string Utf8FromWide(std::wstring_view w)
{
	if (w.empty()) return {};

	int size = WideCharToMultiByte(CP_UTF8, 0,
		w.data(), static_cast<int>(w.size()),
		nullptr, 0, nullptr, nullptr);
	if (size <= 0) return {};

	std::string utf8(size, '\0');
	WideCharToMultiByte(CP_UTF8, 0,
		w.data(), static_cast<int>(w.size()),
		utf8.data(), size, nullptr, nullptr);
	return utf8;
}

BOOL WriteLog(string_view Message)
{
	errno_t err{};
	FILE* log{};

	if (0 != (err = fopen_s(&log, g_Logs.c_str(), "a+")))
	{
		return false;
	}

	fprintf(log, ">> %s \n", Message.data());
	fclose(log);
	return true;

}

BOOL WriteLog(std::wstring_view Message)
{
	errno_t err{};
	FILE* log{};

	if (0 != (err = fopen_s(&log, g_Logs.c_str(), "a+")))
		return FALSE;

	std::string utf8 = Utf8FromWide(Message);
	fprintf(log, ">> %s \n", utf8.c_str());

	fclose(log);
	return TRUE;
}