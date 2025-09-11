#pragma once

#include <windows.h>
#include <iostream>

using std::string_view;
using std::wstring_view;

BOOL
WriteLog(string_view Message);

BOOL
WriteLog(wstring_view Message);