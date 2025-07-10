#define NOMINMAX

#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <limits>
#include <vector>
#include <algorithm>
#include <cctype>

#include <windows.h>
#include <shlobj.h>
#include <KnownFolders.h>
#include <atlbase.h>
#include <conio.h>
#include <tlhelp32.h>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

// Metadata
const std::string APP_NAME = "Blue Archive Screen Mode Tweaker";
const std::string APP_VERSION = "v1.0.0-rc1";
const std::string APP_AUTHOR = "modular-arisu";

// Predefine registry key & values
const std::string KEY_BLUE_ARCHIVE_PATH = "Software\\Nexon Games\\Blue Archive";
const std::string VALUE_FULLSCREEN_MODE_DEFAULT = "Screenmanager Fullscreen mode Default_h401710285";
const std::string VALUE_FULLSCREEN_MODE = "Screenmanager Fullscreen mode_h3630240806";
const std::string VALUE_RESOLUTION_WIDTH = "Screenmanager Resolution Width_h182942802";
const std::string VALUE_RESOLUTION_HEIGHT= "Screenmanager Resolution Height_h2627697771";
const std::string VALUE_RESOLUTION_WINDOW_WIDTH = "Screenmanager Resolution Window Width_h2524650974";
const std::string VALUE_RESOLUTION_WINDOW_HEIGHT = "Screenmanager Resolution Window Height_h1684712807";
const std::string VALUE_WINDOW_POS_X = "Screenmanager Window Position X_h4088080503";
const std::string VALUE_WINDOW_POS_Y = "Screenmanager Window Position Y_h4088080502";

// Other constant variables
const std::string BLUE_ARCHIVE_EXE = "BlueArchive.exe";

static void PrintWelcomeMessage() {
    std::cout << "======================================================" << std::endl;
    std::cout << "           " << APP_NAME << "           " << std::endl;
    std::cout << "======================================================" << std::endl;
    std::cout << " Version: " << APP_VERSION << std::endl;
    std::cout << " Author:  " << APP_AUTHOR << std::endl;
    std::cout << std::endl;
    std::cout << " This tool helps you change Blue Archive PC Client's" << std::endl;
    std::cout << " screen mode settings. Please review the detected " << std::endl;
    std::cout << " configurations below and select new screen mode" << std::endl;
    std::cout << " to apply." << std::endl;
    std::cout << "------------------------------------------------------" << std::endl;
    std::cout << std::endl; // Add an empty line for better spacing
}

static void ExitApp(int exitCode = 0) {
    std::cout << std::endl;
    std::cout << "Exiting..." << std::endl;
    std::cout << std::endl;
    exit(exitCode);
}

// Utility function to convert std::string to std::wstring
static std::wstring s2ws(const std::string& s) {
    int len;
    int slength = (int)s.length() + 1; // +1 for null terminator
    len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), slength, 0, 0);
    wchar_t* buf = new wchar_t[len];
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), slength, buf, len);
    std::wstring r(buf);
    delete[] buf;
    return r;
}

// Function to get the path to Blue Archive's AppData/LocalLow directory
static std::string GetBlueArchiveDataPath() {
    PWSTR pszPath = NULL;
    // Get the path to AppData\LocalLow
    HRESULT hr = SHGetKnownFolderPath(FOLDERID_LocalAppDataLow, 0, NULL, &pszPath);

    std::string appDataLowPath;
    if (SUCCEEDED(hr)) {
        // Convert wide character string (PWSTR) to standard string (std::string)
        int wlen = wcslen(pszPath);
        int len = WideCharToMultiByte(CP_UTF8, 0, pszPath, wlen, NULL, 0, NULL, NULL);
        if (len > 0) {
            std::string temp(len, '\0');
            WideCharToMultiByte(CP_UTF8, 0, pszPath, wlen, &temp[0], len, NULL, NULL);
            appDataLowPath = temp;
        }
        CoTaskMemFree(pszPath); // Free the memory allocated by SHGetKnownFolderPath
    }
    else {
        std::cerr << "ERROR: Error getting AppData\\LocalLow path: " << hr << std::endl;
        return "";
    }

    // Construct the full path
    return appDataLowPath + "\\Nexon Games\\Blue Archive";
}

// Function to read values from DeviceOption JSON
static int GetBlueArchiveDeviceOptionValue(const std::string& key) {
    std::string baDirPath = GetBlueArchiveDataPath();
    if (baDirPath.empty()) {
        std::cerr << "ERROR: Blue Archive directory path not found." << std::endl;
        return -1;
    }

    std::string jsonFilePath = baDirPath + "\\DeviceOption";

    std::ifstream jsonFile(jsonFilePath);
    if (!jsonFile.is_open()) {
        // Check for specific errors
        if (errno == ENOENT) { // File not found
            std::cerr << "ERROR: " << jsonFilePath << " not found." << std::endl;
        }
        else if (errno == EACCES) { // Permission denied
            std::cerr << "ERROR: Permission denied when accessing " << jsonFilePath << "." << std::endl;
        }
        else {
            std::cerr << "ERROR: Could not open " << jsonFilePath << ". Error code: " << errno << std::endl;
        }
        return -1;
    }

    try {
        json baJson;
        jsonFile >> baJson; // Parse the JSON from the file

        if (baJson.contains(key)) {
            return baJson[key].get<int>(); // Extract integer value
        }
        else {
            std::cerr << "ERROR: '" << key << "' key not found in " << jsonFilePath << std::endl;
            return -1;
        }
    }
    catch (const json::parse_error& e) {
        std::cerr << "ERROR: Error parsing JSON from " << jsonFilePath << ": " << e.what() << std::endl;
        return -1;
    }
    catch (const std::exception& e) {
        std::cerr << "ERROR: An unexpected error occurred: " << e.what() << std::endl;
        return -1;
    }
}

// Function to update values from DeviceOption JSON
static bool UpdateBlueArchiveDeviceOptionValue(const int screenModeTypeValue, const int screenRatioTypeValue) {
    std::string baDirPath = GetBlueArchiveDataPath();
    if (baDirPath.empty()) {
        std::cerr << "ERROR: Blue Archive data path not found. Cannot update JSON." << std::endl;
        return false;
    }

    std::string jsonFilePath = baDirPath + "\\DeviceOption";

    json baJson;
    std::ifstream inputFile(jsonFilePath);

    // 1. Load JSON file
    if (inputFile.is_open()) {
        try {
            inputFile >> baJson; // Parse existing JSON
            inputFile.close(); // Close the input file stream
        }
        catch (const json::parse_error& e) {
            std::cerr << "ERROR: Failed to parse existing JSON from " << jsonFilePath << ": " << e.what() << std::endl;
            inputFile.close();
            return false;
        }
    }
    else {
        // Check for specific errors
        if (errno == ENOENT) { // File not found
            std::cerr << "ERROR: " << jsonFilePath << " not found." << std::endl;
        }
        else if (errno == EACCES) { // Permission denied
            std::cerr << "ERROR: Permission denied when accessing " << jsonFilePath << "." << std::endl;
        }
        else {
            std::cerr << "ERROR: Could not open " << jsonFilePath << ". Error code: " << errno << std::endl;
        }
        return false;
    }

    // 2. Modify values
    // Using your exact JSON keys for width and height
    baJson["ScreenModeType"] = screenModeTypeValue;
    baJson["ScreenRatioType"] = screenRatioTypeValue;

    // 3. Save JSON file
    std::ofstream outputFile(jsonFilePath);
    if (!outputFile.is_open()) {
        // Check for specific errors
        if (errno == ENOENT) { // File not found
            std::cerr << "ERROR: " << jsonFilePath << " not found." << std::endl;
        }
        else if (errno == EACCES) { // Permission denied
            std::cerr << "ERROR: Permission denied when accessing " << jsonFilePath << "." << std::endl;
        }
        else {
            std::cerr << "ERROR: Could not open " << jsonFilePath << ". Error code: " << errno << std::endl;
        }
        return false;
    }

    try {
        outputFile << baJson.dump(0);
        outputFile.close();
        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "ERROR: Failed to write JSON to " << jsonFilePath << ": " << e.what() << std::endl;
        outputFile.close();
        return false;
    }
}

// This function queries a REG_DWORD value from the registry.
// Returns the integer value on success, -1 on failure
static int GetRegistryDwordValue(HKEY hRootKey, const std::string& subKey, const std::string& valueName) {
    HKEY hKey;
    
    // Open registry key
    std::wstring wsSubKey = s2ws(subKey); // Convert to wstring
    LONG result = RegOpenKeyExW(hRootKey, wsSubKey.c_str(), 0, KEY_READ, &hKey);

    if (result != ERROR_SUCCESS) {
        std::cerr << "ERROR: Error opening registry key '" << subKey << "': " << result << std::endl;
        return -1;
    }

    DWORD dwType;
    DWORD dwData;
    DWORD dwDataSize = sizeof(dwData);

    // Query registry
    std::wstring wsValueName = s2ws(valueName); // Convert to wstring
    result = RegQueryValueExW(hKey, wsValueName.c_str(), NULL, &dwType, (LPBYTE)&dwData, &dwDataSize);

    RegCloseKey(hKey);

    if (result != ERROR_SUCCESS) {
        std::cerr << "ERROR: Error querying registry value '" << valueName << "' in key '" << subKey << "': " << result << std::endl;
        return -1;
    }

    if (dwType == REG_DWORD) {
        return static_cast<int>(dwData);
    }
    else {
        std::cerr << "ERROR: Registry value type for '" << valueName << "' is not REG_DWORD (Actual Type: " << dwType << ")." << std::endl;
        return -1;
    }
}

static bool UpdateRegistryDwordValue(HKEY hKey, const std::string& valueName, DWORD data) {
    std::wstring wsValueName = s2ws(valueName);
    LONG result = RegSetValueEx(
        hKey,
        wsValueName.c_str(),
        0,
        REG_DWORD,
        (const BYTE*)&data,
        sizeof(data)
    );

    if (result != ERROR_SUCCESS) {
        std::cerr << "ERROR: Failed to set registry value '" << valueName << "'. Error code: " << result << std::endl;
        return false;
    }
    return true;
}

static bool UpdateBlueArchiveRegistrySettings(const std::vector<std::pair<std::string, DWORD>>& valuesToUpdate) {
    HKEY hKey;
    LONG result = RegOpenKeyEx(
        HKEY_CURRENT_USER,
        s2ws(KEY_BLUE_ARCHIVE_PATH).c_str(), // Convert registry path string to wstring
        0,
        KEY_SET_VALUE,
        &hKey
    );

    if (result != ERROR_SUCCESS) {
        std::cerr << "ERROR: Could not open registry key " << KEY_BLUE_ARCHIVE_PATH << ". Error code: " << result << std::endl;
        return false;
    }

    bool allSuccessful = true;
    for (const auto& pair : valuesToUpdate) {
        if (!UpdateRegistryDwordValue(hKey, pair.first, pair.second)) {
            allSuccessful = false;
            break; // Stop on first failure
        }
    }

    RegCloseKey(hKey); // Always close the key

    if (allSuccessful) {
        return true;
    }
    else {
        return false;
    }
}

struct BlueArchiveConfig {
    int jsonScreenModeType = -1; // Default to -1 or some invalid state
    int jsonScreenRatioType = -1;
    int fullscreenDefaultMode = -1;
    int fullscreenMode = -1;
    int resolutionWidth = -1;
    int resolutionHeight = -1;
    int windowWidth = -1;
    int windowHeight = -1;
    int windowPositionX = -1;
    int windowPositionY = -1;

    // A flag to indicate if all required settings were loaded successfully
    bool isValid = false;
};

static std::optional<BlueArchiveConfig> LoadBlueArchiveConfigs() {
    BlueArchiveConfig settings;

    int errorCounter = 0;

    // Load JSON settings using std::optional
    auto json_get_and_check = [&](const std::string& key) -> std::optional<int> {
        std::optional<int> value = GetBlueArchiveDeviceOptionValue(key);
        if (!value || value == -1) {
            std::cerr << "ERROR: Failed to retrieve value '" << key << "' from DeviceOption JSON." << std::endl;
            errorCounter += 1;
            return std::nullopt;
        }
        return value;
        };

    // Load Registry settings using std::optional
    auto reg_get_and_check = [&](HKEY rootKey, const std::string& subKeyStr, const std::string& valueNameStr) -> std::optional<int> {
        std::optional<int> value = GetRegistryDwordValue(rootKey, subKeyStr, valueNameStr);
        if (!value) {
            std::cerr << "ERROR: Failed to retrieve registry value '" << valueNameStr << "' from '" << subKeyStr << "'." << std::endl;
            errorCounter += 1;
            return std::nullopt;
        }
        return value;
        };

    // Use a temporary optional variable for each to check its validity
    // Then assign its value to the struct member
    std::optional<int> optValue;

    optValue = json_get_and_check("ScreenModeType");
    if (!optValue) return std::nullopt; settings.jsonScreenModeType = *optValue;

    optValue = json_get_and_check("ScreenRatioType");
    if (!optValue) return std::nullopt; settings.jsonScreenRatioType = *optValue;

    optValue = reg_get_and_check(HKEY_CURRENT_USER, KEY_BLUE_ARCHIVE_PATH, VALUE_FULLSCREEN_MODE_DEFAULT);
    if (!optValue) return std::nullopt; settings.fullscreenDefaultMode = *optValue;

    optValue = reg_get_and_check(HKEY_CURRENT_USER, KEY_BLUE_ARCHIVE_PATH, VALUE_FULLSCREEN_MODE);
    if (!optValue) return std::nullopt; settings.fullscreenMode = *optValue;

    optValue = reg_get_and_check(HKEY_CURRENT_USER, KEY_BLUE_ARCHIVE_PATH, VALUE_RESOLUTION_WIDTH);
    if (!optValue) return std::nullopt; settings.resolutionWidth = *optValue;

    optValue = reg_get_and_check(HKEY_CURRENT_USER, KEY_BLUE_ARCHIVE_PATH, VALUE_RESOLUTION_HEIGHT);
    if (!optValue) return std::nullopt; settings.resolutionHeight = *optValue;

    optValue = reg_get_and_check(HKEY_CURRENT_USER, KEY_BLUE_ARCHIVE_PATH, VALUE_RESOLUTION_WINDOW_WIDTH);
    if (!optValue) return std::nullopt; settings.windowWidth = *optValue;

    optValue = reg_get_and_check(HKEY_CURRENT_USER, KEY_BLUE_ARCHIVE_PATH, VALUE_RESOLUTION_WINDOW_HEIGHT);
    if (!optValue) return std::nullopt; settings.windowHeight = *optValue;

    optValue = reg_get_and_check(HKEY_CURRENT_USER, KEY_BLUE_ARCHIVE_PATH, VALUE_WINDOW_POS_X);
    if (!optValue) return std::nullopt; settings.windowPositionX = *optValue;

    optValue = reg_get_and_check(HKEY_CURRENT_USER, KEY_BLUE_ARCHIVE_PATH, VALUE_WINDOW_POS_Y);
    if (!optValue) return std::nullopt; settings.windowPositionY = *optValue;

    if (errorCounter == 0) {
        settings.isValid = true; // Mark as successfully loaded
    }
    return settings; // Return the populated settings struct wrapped in an optional
}

static bool GetScreenResolution(int& screenWidth, int& screenHeight, LPCTSTR lpDeviceName = NULL) {
    DEVMODE dm;
    ZeroMemory(&dm, sizeof(dm)); // Initialize the DEVMODE structure
    dm.dmSize = sizeof(dm);

    // EnumDisplaySettings to get the current display settings
    // ENUM_CURRENT_SETTINGS gets the currently active settings
    if (EnumDisplaySettings(lpDeviceName, ENUM_CURRENT_SETTINGS, &dm)) {
        screenWidth = dm.dmPelsWidth;
        screenHeight = dm.dmPelsHeight;
        return true;
    }
    else {
        std::string deviceNameStr;
        if (lpDeviceName) {
            // Calculate required buffer size
            int bufferSize = WideCharToMultiByte(CP_UTF8, 0, lpDeviceName, -1, NULL, 0, NULL, NULL);
            if (bufferSize > 0) {
                // Allocate string with correct size and null terminator
                deviceNameStr.resize(bufferSize - 1); // -1 to exclude null terminator from resize
                WideCharToMultiByte(CP_UTF8, 0, lpDeviceName, -1, &deviceNameStr[0], bufferSize, NULL, NULL);
            }
        }
        else {
            deviceNameStr = "Primary";
        }
        std::cerr << "ERROR: Error getting screen resolution for device: " << deviceNameStr << std::endl;
        return false;
    }
}

// Callback function used by EnumDisplayMonitors
// This function is called once for each active monitor.
static BOOL CALLBACK MonitorEnumProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData) {
    // Increment the monitor count
    int* monitorCount = reinterpret_cast<int*>(dwData);
    (*monitorCount)++;

    // Continue enumeration (return TRUE to continue, FALSE to stop)
    return TRUE;
}

// Function to detect if there are multiple monitors
static bool IsMultiMonitorSetup() {
    int monitorCount = 0;
    // EnumDisplayMonitors enumerates all display monitors that intersect a region
    // NULL for the first parameter means enumerate all monitors on the desktop
    // NULL for the second parameter means include all monitors in the enumeration
    EnumDisplayMonitors(NULL, NULL, MonitorEnumProc, reinterpret_cast<LPARAM>(&monitorCount));

    return monitorCount > 1;
}

static std::pair<int, int> GetUserDesiredResolutionByHeight(const int defaultHeight = 720, const double aspectRatio = 16.0 / 9.0) {
    int desiredHeight = -1;
    int calculatedWidth = -1;

    std::cout << std::endl;
    std::cout << "Enter desired vertical resolution (height):" << std::endl;
    std::cout << "  (e.g., " << defaultHeight << " for "
        << static_cast<int>(defaultHeight * aspectRatio) << "x" << defaultHeight
        << " with " << (aspectRatio == (16.0 / 9.0) ? "16:9" : "4:3") << " ratio)" << std::endl;

    while (true) {
        std::cout << "Desired Height: ";
        if (!(std::cin >> desiredHeight)) {
            std::cerr << "Invalid input. Please enter a whole number." << std::endl;
            std::cin.clear(); // Clear error flags
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // Discard invalid input
            continue; // Ask again
        }

        if (desiredHeight < 1) {
            std::cerr << "Height must be 1 or greater. Please try again." << std::endl;
            // No need to clear/ignore here, as it was a valid integer input (just out of range)
            continue; // Ask again
        }

        // Calculate width based on the given aspect ratio
        // We cast to int for integer pixel values, which might truncate decimals.
        // For game resolutions, integer pixels are usually required.
        calculatedWidth = static_cast<int>(desiredHeight * aspectRatio);

        std::cout << std::endl;
        std::cout << "Selected resolution: " << calculatedWidth << "x" << desiredHeight << std::endl;

        // Ask user to confirm custom resolution
        bool isConfirmed;
        std::cout << "Continue with selected resolution?" << std::endl;
        std::cout << "  1. Yes" << std::endl;
        std::cout << "  2. No, try again" << std::endl;
        std::cout << "Enter your choice (1-2): ";

        int choiceRetryResolution;
        // Input validation loop
        while (!(std::cin >> choiceRetryResolution) || choiceRetryResolution < 1 || choiceRetryResolution > 2) {
            std::cerr << "Invalid input. Please enter a number between 1 and 2: ";
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        }

        switch (choiceRetryResolution) {
        case 1:
            isConfirmed = true;
            break;
        case 2:
            isConfirmed = false;
            break;
        }

        if (isConfirmed) {
            break;
        }
        else {
            continue;
        }
    }

    return std::make_pair(calculatedWidth, desiredHeight);
}

// Checks if a specific executable is currently running.
static bool IsProcessRunning(const std::string& exeName) {
    bool found = false;
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0); // Take a snapshot of all processes

    if (hSnapshot == INVALID_HANDLE_VALUE) {
        std::cerr << "ERROR: CreateToolhelp32Snapshot failed. Error code: " << GetLastError() << std::endl;
        return false;
    }

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32); // Must set the size of the structure

    // Retrieve information about the first process, and exit if unsuccessful
    if (!Process32First(hSnapshot, &pe32)) {
        std::cerr << "ERROR: Process32First failed. Error code: " << GetLastError() << std::endl;
        CloseHandle(hSnapshot); // Close the snapshot handle
        return false;
    }

    // Enumerate all processes and compare their executable names
    do {
        // Convert the process executable name (TCHAR array) to std::string
        // This part needs to handle UNICODE vs. ANSI builds carefully.
        std::string currentExeFile_narrow;

        // If UNICODE is defined, pe32.szExeFile is wchar_t[], so convert to std::string
        std::wstring wsCurrentExeFile(pe32.szExeFile);
        int size_needed = WideCharToMultiByte(CP_UTF8, 0, wsCurrentExeFile.c_str(), (int)wsCurrentExeFile.length(), NULL, 0, NULL, NULL);
        currentExeFile_narrow.resize(size_needed); // Resize string to fit converted chars
        WideCharToMultiByte(CP_UTF8, 0, wsCurrentExeFile.c_str(), (int)wsCurrentExeFile.length(), &currentExeFile_narrow[0], size_needed, NULL, NULL);

        // Perform case-sensitive comparison directly
        if (currentExeFile_narrow == exeName) {
            found = true;
            break; // Found the process, no need to continue checking
        }
    } while (Process32Next(hSnapshot, &pe32));

    CloseHandle(hSnapshot); // Always close the snapshot handle
    return found;
}

static void FinalConfirmationBeforeApplying() {
    // Present choices to the user
    std::cout << "Apply new configs?" << std::endl;
    std::cout << "  1. Yes" << std::endl;
    std::cout << "  2. No, exit the app" << std::endl;
    std::cout << "Enter your choice (1-2): ";

    int choice;
    // Input validation loop
    while (!(std::cin >> choice) || choice < 1 || choice > 2) {
        std::cerr << "Invalid input. Please enter a number between 1 and 2: ";
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }

    switch (choice) {
    case 1:
        return;
    case 2:
        ExitApp(0);
    }
}

static void UpdateConfigs(
    int jsonScreenModeTypeValue,
    int jsonScreenRatioTypeValue,
    int regFullscreenModeDefault,
    int regFullscreenMode,
    int regResolutionWidth,
    int regResolutionHeight,
    int regWindowPositionX,
    int regWindowPositionY
) {
    // Check if BlueArchive is running
    while (true) {
        if (IsProcessRunning(BLUE_ARCHIVE_EXE)) {
            std::cout << std::endl;
            std::cerr << "ERROR: Blue Archive client (\"" << BLUE_ARCHIVE_EXE << "\") is running."
                << " Close the client before continuing.";
            std::cout << std::endl;
            std::cout << "\nPress any key to retry...";
            _getch();
            std::cout << std::endl;
        }
        else {
            break;
        }
    }

    std::cout << std::endl;
    std::cout << "Applying new changes..." << std::endl;

    std::cout << " - Updating DeviceOption JSON..." << std::endl;
    const bool jsonUpdateResult = UpdateBlueArchiveDeviceOptionValue(jsonScreenModeTypeValue, jsonScreenRatioTypeValue);

    std::cout << " - Updating Registry..." << std::endl;
    std::vector<std::pair<std::string, DWORD>> regKeyValueVector = {
        {VALUE_FULLSCREEN_MODE_DEFAULT, (DWORD)regFullscreenModeDefault},
        {VALUE_FULLSCREEN_MODE, (DWORD)regFullscreenMode},
        {VALUE_RESOLUTION_WIDTH, (DWORD)regResolutionWidth},
        {VALUE_RESOLUTION_HEIGHT, (DWORD)regResolutionHeight},
        {VALUE_RESOLUTION_WINDOW_WIDTH, (DWORD)regResolutionWidth},
        {VALUE_RESOLUTION_WINDOW_HEIGHT, (DWORD)regResolutionHeight},
        {VALUE_WINDOW_POS_X, (DWORD)regWindowPositionX},
        {VALUE_WINDOW_POS_Y, (DWORD)regWindowPositionY}
    };
    const bool regUpdateResult = UpdateBlueArchiveRegistrySettings(regKeyValueVector);

    std::cout << std::endl;
    if (jsonUpdateResult && regUpdateResult) {
        std::cout << "Successfully applied new configs." << std::endl;
    }
    else {
        std::cout << "One or more tasks has failed. Check app's output for details." << std::endl;
    }
    std::cout << std::endl;
    std::cout << "Press any key to exit...";
    _getch();
    ExitApp(jsonUpdateResult && regUpdateResult ? 0 : 1);
}

static void ApplyFullscreenMode(const bool isExclusive) {
    // Get screen resolution
    int screenWidth = 0;
    int screenHeight = 0;

    GetScreenResolution(screenWidth, screenHeight);

    // Final confirmation
    std::cout << std::endl;
    std::cout << "================== Selected Configs ==================" << std::endl;
    std::cout << "Screen Mode:        " << (isExclusive ? "Exclusive Fullscreen" : "Borderless Fullscreen") << std::endl;
    std::cout << "Window Resolution:  " << screenWidth << "x" << screenHeight << " (Auto calculated)" << std::endl;
    std::cout << "Window Position:    0x0 (Auto calculated)" << std::endl;
    std::cout << "======================================================" << std::endl;
    std::cout << std::endl;

    FinalConfirmationBeforeApplying();

    // Apply new config
    const int newFullscreenModeDefault = isExclusive ? 0 : 1;
    const int newFullscreenMode = isExclusive ? 1 : 3;

    UpdateConfigs(0, 1, newFullscreenModeDefault, newFullscreenMode, screenWidth, screenHeight, 0, 0);
}

static void ApplyWindowedMode() {
    /* Screen ratio selection */
    // Present choices to the user
    std::cout << std::endl;
    std::cout << "Please select a window screen ratio to apply:" << std::endl;
    std::cout << "  1. 16:9" << std::endl;
    std::cout << "  2. 4:3" << std::endl;
    std::cout << "Enter your choice (1-2): ";

    int choiceRatio;
    // Input validation loop
    while (!(std::cin >> choiceRatio) || choiceRatio < 1 || choiceRatio > 2) {
        std::cerr << "Invalid input. Please enter a number between 1 and 2: ";
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }

    static double aspectRatio = (choiceRatio == 1 ? 16.0/9.0 : 4.0/3.0);

    /* Screen resolution selection */
    // Present choices to the user
    std::cout << std::endl;
    std::cout << "Apply default resolution?" << std::endl;
    std::cout << "  1. Yes (" << (choiceRatio == 1 ? "1280x720" : "1280x960") << ")" << std::endl;
    std::cout << "  2. No, enter custom resolution" << std::endl;
    std::cout << "Enter your choice (1-2): ";

    int choiceResolution;
    // Input validation loop
    while (!(std::cin >> choiceResolution) || choiceResolution < 1 || choiceResolution > 2) {
        std::cerr << "Invalid input. Please enter a number between 1 and 2: ";
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }

    int resolutionWidth = 1280;
    int resolutionHeight = (choiceRatio == 1 ? 720 : 960);

    if (choiceResolution == 2) {
        std::pair<int, int> selectedRes = GetUserDesiredResolutionByHeight(resolutionHeight, aspectRatio);
        resolutionWidth = selectedRes.first;
        resolutionHeight = selectedRes.second;
    }

    /* Calculate Window Position */
    // Get screen resolution
    int screenWidth = 0;
    int screenHeight = 0;

    GetScreenResolution(screenWidth, screenHeight);

    const int windowPositionX = ((screenWidth - resolutionWidth) / 2) - 1; // Subtract extra 1 to match BA Client's logic
    const int windowPositionY = ((screenHeight - resolutionHeight) / 2) - 20;

    // Final confirmation
    std::cout << std::endl;
    std::cout << "================== Selected Configs ==================" << std::endl;
    std::cout << "Screen Mode:        Windowed" << std::endl;
    std::cout << "Window Scren Ratio: " << (choiceRatio == 1 ? "16:9" : "4:3") << std::endl;
    std::cout << "Window Resolution:  " << resolutionWidth << "x" << resolutionHeight << std::endl;
    std::cout << "Window Position:    " << windowPositionX << "x" << windowPositionY << " (Auto calculated)" << std::endl;
    std::cout << "======================================================" << std::endl;
    std::cout << std::endl;

    FinalConfirmationBeforeApplying();

    // Apply new config
    const int newScreenRatioType = (choiceRatio == 1 ? 1 : 0);
    
    UpdateConfigs(1, newScreenRatioType, 1, 3, resolutionWidth, resolutionHeight, windowPositionX, windowPositionY);
}

int main() {
    // Welcome message
    PrintWelcomeMessage();

    // Attempt to load all settings
    std::optional<BlueArchiveConfig> baSettingsOpt = LoadBlueArchiveConfigs();

    // Check if loading was successful
    if (!baSettingsOpt) { // Equivalent to: if (baSettingsOpt.has_value() == false)
        std::cerr << "ERROR: Failed to load Blue Archive configs." << std::endl;
        std::cout << std::endl;
        std::cout << "Required registry and/or config files may not have been populated," << std::endl;
        std::cout << "or the access to those resources has been blocked." << std::endl;
        std::cout << std::endl;
        std::cout << "Please run and log in to the game at least once before running this app." << std::endl;
        std::cout << std::endl;
        std::cout << "Press any key to exit...";
        _getch();
        ExitApp(1);
    }

    // If successful, extract the settings struct
    BlueArchiveConfig baConfig = *baSettingsOpt; // Dereference the optional to get the struct

    // Get screen resolution
    int screenWidth = 0;
    int screenHeight = 0;

    GetScreenResolution(screenWidth, screenHeight);

    // Show information
    std::cout << "================== Detected Configs ==================" << std::endl;
    if (baConfig.jsonScreenModeType == 0 && (baConfig.fullscreenDefaultMode == 0 || 1) && baConfig.fullscreenMode == 1) {
        // Exclusive Fullscreen
        std::cout << "Current Screen Mode: Exclusive Fullscreen" << std::endl;
        std::cout << "Screen Resolution:   " << screenWidth << "x" << screenHeight << std::endl;
        std::cout << "Game Resolution:     " << baConfig.resolutionWidth << "x" << baConfig.resolutionHeight << std::endl;
    }
    else if (baConfig.jsonScreenModeType == 0 && baConfig.fullscreenDefaultMode == 1 && baConfig.fullscreenMode == 3) {
        // Borderless Fullscreen
        std::cout << "Current Screen Mode: Borderless Fullscreen" << std::endl;
        std::cout << "Screen Resolution:   " << screenWidth << "x" << screenHeight << std::endl;
        std::cout << "Game Resolution:     " << baConfig.resolutionWidth << "x" << baConfig.resolutionHeight << std::endl;
    }
    else if (baConfig.jsonScreenModeType == 1 && baConfig.fullscreenDefaultMode == 1 && baConfig.fullscreenMode == 3) {
        // Windowed
        std::cout << "Current Screen Mode: Windowed" << std::endl;
        std::cout << "Screen Resolution:   " << screenWidth << "x" << screenHeight << std::endl;
        std::cout << "Window Scren Ratio:  " << (baConfig.jsonScreenRatioType == 1 ? "16:9" : "4:3") << std::endl;
        std::cout << "Window Resolution:   " << baConfig.resolutionWidth << "x" << baConfig.resolutionHeight << std::endl;
        std::cout << "Window Position:     " << baConfig.windowPositionX << "x" << baConfig.windowPositionY << std::endl;
    }
    else {
        // Unidentifiable
        std::cout << "Current Screen Mode: Unknown" << std::endl;
        std::cout << "Screen Resolution:   " << screenWidth << "x" << screenHeight << std::endl;
        std::cout << "Game Resolution:     " << baConfig.resolutionWidth << "x" << baConfig.resolutionHeight << std::endl;
        std::cout << "Window Scren Ratio:  " << (baConfig.jsonScreenRatioType == 1 ? "16:9" : "4:3") << std::endl;
        std::cout << "Window Position:     " << baConfig.windowPositionX << "x" << baConfig.windowPositionY << std::endl;
    }

    if (IsMultiMonitorSetup()) {
        std::cout << std::endl;
        std::cout << "WARN: Multi-monitor setup detected." << std::endl;
        std::cout << "      All configurations will be applied based on" << std::endl;
        std::cout << "      the primary monitor." << std::endl;
        std::cout << std::endl;
    }
    std::cout << "======================================================" << std::endl;
    std::cout << std::endl;

    // Present choices to the user
    std::cout << "Please select a screen mode to apply:" << std::endl;
    std::cout << "  1. Exclusive Fullscreen" << std::endl;
    std::cout << "  2. Borderless Fullscreen" << std::endl;
    std::cout << "  3. Windowed" << std::endl;
    std::cout << "  4. Exit without changes" << std::endl;
    std::cout << "Enter your choice (1-4): ";

    int choice;
    // Input validation loop
    while (!(std::cin >> choice) || choice < 1 || choice > 4) {
        std::cerr << "Invalid input. Please enter a number between 1 and 4: ";
        std::cin.clear(); // Clear error flags (e.g., if non-numeric input)
        // Discard remaining invalid input from the buffer up to the newline character
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }

    std::cout << std::endl; // Add an empty line after input

    // Handle user's choice
    switch (choice) {
    case 1:
        // Exclusive Fullscreen
        ApplyFullscreenMode(true);
        break;
    case 2:
        // Borderless Fullscreen
        ApplyFullscreenMode(false);
        break;
    case 3:
        // Windowed
        ApplyWindowedMode();
        break;
    case 4:
        ExitApp(0);
        break;
    }

    return 0;
}