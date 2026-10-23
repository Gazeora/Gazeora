#include "platform/usb_driver.h"

// clang-format off
#include <wincrypt.h>
#include <mscat.h>
#include <newdev.h>
#include <setupapi.h>
#include <cfgmgr32.h>
#include <winioctl.h>
#include <usbioctl.h>
#include <shellapi.h>
// clang-format on

#include <algorithm>
#include <cstring>
#include <cwchar>
#include <string>
#include <utility>
#include <vector>

#include "gazeora/common/text/utf8.h"

namespace gazeora::app {

namespace {

constexpr wchar_t kCertificateName[] = L"Gazeora USB accessory driver";
constexpr wchar_t kInfName[] = L"gazeora_accessory.inf";
constexpr wchar_t kCatName[] = L"gazeora_accessory.cat";
constexpr wchar_t kProvider[] = L"Microsoft Enhanced RSA and AES Cryptographic Provider";

// What marks a driver package as Gazeora's in Windows' INF folder, where Windows keeps a copy of
// each installed one (as oemNN.inf).
constexpr char kPackageMarker[] = "; Gazeora USB driver package";
constexpr char kPhoneEntry[] = " = PhoneInstall, ";

// The driver package: WinUSB, Windows' own driver, for
// - the accessory interface: on its own (18D1:2D00) or interface 0 next to adb and audio
//   (2D01, 2D04, 2D05). Its interface GUID is how the stream finds it (usb_accessory_output.cpp:
//   kAccessoryInterface); keep the two in step.
// - the phones in `phoneIds`, in their normal mode, so libusb can ask them to switch without
//   USB debugging. Their own driver (file transfer) gives way to it.
// `stamp` tells this package from earlier ones. The catalog names the INF only.
std::string BuildInf(const std::vector<std::wstring>& phoneIds, const std::string& stamp,
                     const SYSTEMTIME& now) {
    char version[64];
    snprintf(version, sizeof(version), "%02u/%02u/%04u,1.%u.%u.%u", now.wMonth, now.wDay,
             now.wYear, now.wYear % 100, now.wMonth * 100 + now.wDay,
             now.wHour * 100 + now.wMinute);
    std::string devices;
    for (const char* id : {"USB\\VID_18D1&PID_2D00", "USB\\VID_18D1&PID_2D01&MI_00",
                           "USB\\VID_18D1&PID_2D04&MI_00", "USB\\VID_18D1&PID_2D05&MI_00"}) {
        devices += std::string("%Accessory% = Install, ") + id + "\r\n";
    }
    for (const std::wstring& id : phoneIds) {
        devices += "%Phone%" + std::string(kPhoneEntry) + Utf8::FromWide(id) + "\r\n";
    }
    return std::string(kPackageMarker) + " " + stamp +
           "\r\n"
           "; WinUSB for the accessory interface of Android phones in USB accessory mode, so\r\n"
           "; Gazeora can stream to the Gazeora app over the cable; and for phones Gazeora\r\n"
           "; asks to switch to it without USB debugging.\r\n"
           "[Version]\r\n"
           "Signature = \"$Windows NT$\"\r\n"
           "Class = USBDevice\r\n"
           "ClassGUID = {88BAE032-5A81-49f0-BC3D-A4FF138216D6}\r\n"
           "Provider = %Gazeora%\r\n"
           "CatalogFile = gazeora_accessory.cat\r\n"
           "DriverVer = " + std::string(version) +
           "\r\n"
           "\r\n"
           "[Manufacturer]\r\n"
           "%Gazeora% = Devices, NTamd64, NTarm64\r\n"
           "\r\n"
           "[Devices.NTamd64]\r\n" + devices +
           "\r\n"
           "[Devices.NTarm64]\r\n" + devices +
           "\r\n"
           "[Install]\r\n"
           "Include = winusb.inf\r\n"
           "Needs = WINUSB.NT\r\n"
           "\r\n"
           "[Install.Services]\r\n"
           "Include = winusb.inf\r\n"
           "Needs = WINUSB.NT.Services\r\n"
           "\r\n"
           "[Install.HW]\r\n"
           "AddReg = Interface\r\n"
           "\r\n"
           "[Interface]\r\n"
           "HKR,,DeviceInterfaceGUIDs,0x10000,\"{6A8E2B7C-3F41-4C9B-9D0E-7B2F5A1C8E43}\"\r\n"
           "\r\n"
           "[PhoneInstall]\r\n"
           "Include = winusb.inf\r\n"
           "Needs = WINUSB.NT\r\n"
           "\r\n"
           "[PhoneInstall.Services]\r\n"
           "Include = winusb.inf\r\n"
           "Needs = WINUSB.NT.Services\r\n"
           "\r\n"
           "[PhoneInstall.HW]\r\n"
           "AddReg = PhoneInterface\r\n"
           "\r\n"
           "[PhoneInterface]\r\n"
           "HKR,,DeviceInterfaceGUIDs,0x10000,\"{3C1F7D52-9A6E-4B8D-A2C4-5E7B9D1F3A68}\"\r\n"
           "\r\n"
           "[Strings]\r\n"
           "Gazeora = \"Gazeora\"\r\n"
           "Accessory = \"Android Accessory Interface (Gazeora)\"\r\n"
           "Phone = \"Android phone, for Gazeora's USB stream (no file transfer)\"\r\n";
}

// mssign32.dll's signing API, which the SDK headers MinGW has don't declare.
struct SignerFileInfo {
    DWORD cbSize;
    LPCWSTR pwszFileName;
    HANDLE hFile;
};
struct SignerSubjectInfo {
    DWORD cbSize;
    DWORD* pdwIndex;
    DWORD dwSubjectChoice;  // 1: a file
    SignerFileInfo* pSignerFileInfo;
};
struct SignerCertStoreInfo {
    DWORD cbSize;
    PCCERT_CONTEXT pSigningCert;
    DWORD dwCertPolicy;  // 2: include the chain
    HCERTSTORE hCertStore;
};
struct SignerCert {
    DWORD cbSize;
    DWORD dwCertChoice;  // 2: a certificate in a store
    SignerCertStoreInfo* pCertStoreInfo;
    HWND hwnd;
};
struct SignerSignatureInfo {
    DWORD cbSize;
    ALG_ID algidHash;
    DWORD dwAttrChoice;  // 0: no attributes
    void* pAttrAuthcode;
    PCRYPT_ATTRIBUTES psAuthenticated;
    PCRYPT_ATTRIBUTES psUnauthenticated;
};
struct SignerContext {
    DWORD cbSize;
    DWORD cbBlob;
    BYTE* pbBlob;
};
using SignerSignExFunction = HRESULT(WINAPI*)(DWORD, SignerSubjectInfo*, SignerCert*,
                                              SignerSignatureInfo*, void*, LPCWSTR,
                                              PCRYPT_ATTRIBUTES, void*, SignerContext**);
using SignerFreeSignerContextFunction = HRESULT(WINAPI*)(SignerContext*);

// And one MinGW's import libraries lack.
using DiInstallDriverFunction = BOOL(WINAPI*)(HWND, LPCWSTR, DWORD, PBOOL);

// A function of a system DLL, which stays loaded for the rest of the process.
template <typename Function>
Function SystemFunction(const wchar_t* dll, const char* name) {
    HMODULE module = LoadLibraryExW(dll, nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    return module ? reinterpret_cast<Function>(reinterpret_cast<void*>(GetProcAddress(module, name)))
                  : nullptr;
}

std::wstring ErrorText(const wchar_t* what, DWORD code = GetLastError()) {
    wchar_t* text = nullptr;
    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                       FORMAT_MESSAGE_IGNORE_INSERTS,
                   nullptr, code, 0, reinterpret_cast<wchar_t*>(&text), 0, nullptr);
    std::wstring message = std::wstring(what) + L": ";
    if (text) {
        message += text;
        LocalFree(text);
        while (!message.empty() && (message.back() == L'\n' || message.back() == L'\r')) {
            message.pop_back();
        }
    } else {
        wchar_t hex[16];
        swprintf(hex, 16, L"0x%08lX", static_cast<unsigned long>(code));
        message += hex;
    }
    return message;
}

// MinGW's fstream doesn't take wide paths.
bool WriteBytes(const std::wstring& path, const void* data, size_t size) {
    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }
    DWORD written = 0;
    bool ok = WriteFile(file, data, DWORD(size), &written, nullptr) && written == size;
    CloseHandle(file);
    return ok;
}

std::wstring ReadText(const std::wstring& path) {
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return {};
    }
    wchar_t buffer[1024] = {};
    DWORD read = 0;
    ReadFile(file, buffer, sizeof(buffer) - sizeof(wchar_t), &read, nullptr);
    CloseHandle(file);
    return std::wstring(buffer, read / sizeof(wchar_t));
}

// A self-signed code signing certificate, its key in `container` (machine-wide).
PCCERT_CONTEXT CreateSigningCertificate(const std::wstring& container, std::wstring& error) {
    HCRYPTPROV provider = 0;
    if (!CryptAcquireContextW(&provider, container.c_str(), kProvider, PROV_RSA_AES,
                              CRYPT_NEWKEYSET | CRYPT_MACHINE_KEYSET)) {
        error = ErrorText(L"Couldn't make a signing key");
        return nullptr;
    }
    HCRYPTKEY key = 0;
    bool made = CryptGenKey(provider, AT_SIGNATURE, 2048 << 16, &key);
    DWORD code = GetLastError();
    if (made) {
        CryptDestroyKey(key);
    }
    CryptReleaseContext(provider, 0);
    if (!made) {
        error = ErrorText(L"Couldn't make a signing key", code);
        return nullptr;
    }

    std::wstring subject = std::wstring(L"CN=\"") + kCertificateName + L"\"";
    DWORD nameSize = 0;
    CertStrToNameW(X509_ASN_ENCODING, subject.c_str(), CERT_X500_NAME_STR, nullptr, nullptr,
                   &nameSize, nullptr);
    std::vector<BYTE> name(nameSize);
    CertStrToNameW(X509_ASN_ENCODING, subject.c_str(), CERT_X500_NAME_STR, nullptr, name.data(),
                   &nameSize, nullptr);
    CERT_NAME_BLOB nameBlob = {nameSize, name.data()};

    // Code signing, nothing else.
    LPSTR usages[] = {const_cast<LPSTR>(szOID_PKIX_KP_CODE_SIGNING)};
    CERT_ENHKEY_USAGE usage = {1, usages};
    DWORD usageSize = 0;
    CryptEncodeObject(X509_ASN_ENCODING, X509_ENHANCED_KEY_USAGE, &usage, nullptr, &usageSize);
    std::vector<BYTE> encodedUsage(usageSize);
    CryptEncodeObject(X509_ASN_ENCODING, X509_ENHANCED_KEY_USAGE, &usage, encodedUsage.data(),
                      &usageSize);
    CERT_EXTENSION extension = {const_cast<LPSTR>(szOID_ENHANCED_KEY_USAGE), FALSE,
                                {usageSize, encodedUsage.data()}};
    CERT_EXTENSIONS extensions = {1, &extension};

    CRYPT_KEY_PROV_INFO keyInfo = {};
    keyInfo.pwszContainerName = const_cast<LPWSTR>(container.c_str());
    keyInfo.pwszProvName = const_cast<LPWSTR>(kProvider);
    keyInfo.dwProvType = PROV_RSA_AES;
    keyInfo.dwFlags = CRYPT_MACHINE_KEYSET;
    keyInfo.dwKeySpec = AT_SIGNATURE;
    CRYPT_ALGORITHM_IDENTIFIER algorithm = {const_cast<LPSTR>(szOID_RSA_SHA256RSA), {}};
    SYSTEMTIME expiry;
    GetSystemTime(&expiry);
    expiry.wYear += 20;
    if (expiry.wDay > 28) {
        expiry.wDay = 28;  // no 29 February in the year it lands on
    }
    PCCERT_CONTEXT certificate = CertCreateSelfSignCertificate(
        0, &nameBlob, 0, &keyInfo, &algorithm, nullptr, &expiry, &extensions);
    if (!certificate) {
        error = ErrorText(L"Couldn't make a signing certificate");
    }
    return certificate;
}

void DeleteKey(const std::wstring& container) {
    HCRYPTPROV provider = 0;
    CryptAcquireContextW(&provider, container.c_str(), kProvider, PROV_RSA_AES,
                         CRYPT_DELETEKEYSET | CRYPT_MACHINE_KEYSET);
}

// The first error CryptCAT* reports while making a catalog, and the definition line it was in.
// Its error callback takes no context pointer, so this is where it can leave them.
DWORD g_catalogFailure = 0;
std::wstring g_catalogFailureLine;

// A named WINAPI function, not a lambda: on x86 Win32 calls it __stdcall, and a lambda only
// converts to a __cdecl function pointer.
void WINAPI OnCatalogError(DWORD area, DWORD code, WCHAR* line) {
    if (!g_catalogFailure) {
        g_catalogFailure = code ? code : area;
        g_catalogFailureLine = line ? line : L"";
    }
}

// The catalog of the driver package, the INF's SHA-256 hash in it, made as makecat makes it:
// from a definition, through Windows' own catalog writer. (Catalogs put together member by
// member come out in a form driver installs don't accept.)
bool CreateCatalog(const std::wstring& folder, std::wstring& error) {
    std::wstring definition = folder + L"\\gazeora_accessory.cdf";
    // Windows 10 and 11 only: for a catalog that names older versions, Windows looks files up
    // by their SHA-1 hash, which a SHA-256 catalog doesn't have.
    std::wstring text =
        L"[CatalogHeader]\r\n"
        L"Name=" + std::wstring(kCatName) + L"\r\n"
        L"ResultDir=" + folder + L"\r\n"
        L"PublicVersion=0x00000001\r\n"
        L"CatalogVersion=2\r\n"
        L"HashAlgorithms=SHA256\r\n"
        L"EncodingType=0x00010001\r\n"
        L"CATATTR1=0x10010001:OSAttr:2:10.0\r\n"
        L"[CatalogFiles]\r\n"
        L"<hash>" + kInfName + L"=" + folder + L"\\" + kInfName + L"\r\n"
        L"<hash>" + kInfName + L"ATTR1=0x10010001:OSAttr:2:10.0\r\n"
        L"<hash>" + kInfName + L"ATTR2=0x10010001:File:" + kInfName + L"\r\n";
    // In the ANSI code page: the writer doesn't read UTF-16 definitions.
    int size = WideCharToMultiByte(CP_ACP, 0, text.c_str(), int(text.size()), nullptr, 0,
                                   nullptr, nullptr);
    std::string ansi(size_t(size), '\0');
    WideCharToMultiByte(CP_ACP, 0, text.c_str(), int(text.size()), ansi.data(), size, nullptr,
                        nullptr);
    if (!WriteBytes(definition, ansi.data(), ansi.size())) {
        error = ErrorText(L"Couldn't write the driver catalog's definition");
        return false;
    }
    using EnumAttributes = CRYPTCATATTRIBUTE*(WINAPI*)(CRYPTCATCDF*, LPWSTR, CRYPTCATMEMBER*,
                                                       CRYPTCATATTRIBUTE*,
                                                       PFN_CDF_PARSE_ERROR_CALLBACK);
    auto enumAttributes = SystemFunction<EnumAttributes>(L"wintrust.dll",
                                                         "CryptCATCDFEnumAttributesWithCDFTag");
    // Not in the Windows SDK's mscat.h either, so looked up the same way.
    using EnumMembers = LPWSTR(WINAPI*)(CRYPTCATCDF*, LPWSTR, PFN_CDF_PARSE_ERROR_CALLBACK,
                                        CRYPTCATMEMBER**, BOOL, LPVOID);
    auto enumMembers =
        SystemFunction<EnumMembers>(L"wintrust.dll", "CryptCATCDFEnumMembersByCDFTagEx");

    // The first thing the definition had wrong, and where.
    DWORD& failure = g_catalogFailure;
    std::wstring& where = g_catalogFailureLine;
    failure = 0;
    where.clear();
    PFN_CDF_PARSE_ERROR_CALLBACK onError = OnCatalogError;
    CRYPTCATCDF* cdf = enumAttributes && enumMembers
                           ? CryptCATCDFOpen(const_cast<LPWSTR>(definition.c_str()), onError)
                           : nullptr;
    if (!cdf) {
        error = ErrorText(L"Couldn't make the driver catalog", failure ? failure : GetLastError());
        DeleteFileW(definition.c_str());
        return false;
    }
    // Reading each part of the definition is what puts it in the catalog.
    CRYPTCATATTRIBUTE* attribute = nullptr;
    while ((attribute = CryptCATCDFEnumCatAttributes(cdf, attribute, onError))) {
    }
    CRYPTCATMEMBER* member = nullptr;
    LPWSTR tag = nullptr;
    while ((tag = enumMembers(cdf, tag, onError, &member, TRUE, nullptr))) {
        attribute = nullptr;
        while ((attribute = enumAttributes(cdf, tag, member, attribute, onError))) {
        }
    }
    bool ok = CryptCATCDFClose(cdf) && !failure;
    if (!ok) {
        error = failure ? ErrorText((L"Couldn't make the driver catalog (" + where + L")").c_str(),
                                    failure)
                        : ErrorText(L"Couldn't make the driver catalog");
    }
    DeleteFileW(definition.c_str());
    return ok;
}

bool SignCatalog(const std::wstring& cat, PCCERT_CONTEXT certificate, std::wstring& error) {
    HMODULE mssign = LoadLibraryExW(L"mssign32.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    auto signerSignEx = mssign ? reinterpret_cast<SignerSignExFunction>(
                                     reinterpret_cast<void*>(GetProcAddress(mssign, "SignerSignEx")))
                               : nullptr;
    auto freeContext = mssign ? reinterpret_cast<SignerFreeSignerContextFunction>(
                                    reinterpret_cast<void*>(
                                        GetProcAddress(mssign, "SignerFreeSignerContext")))
                              : nullptr;
    if (!signerSignEx || !freeContext) {
        error = L"Windows' code signing (mssign32.dll) isn't available";
        if (mssign) {
            FreeLibrary(mssign);
        }
        return false;
    }
    SignerFileInfo file = {sizeof(file), cat.c_str(), nullptr};
    DWORD index = 0;
    SignerSubjectInfo subject = {sizeof(subject), &index, 1, &file};
    SignerCertStoreInfo store = {sizeof(store), certificate, 2, nullptr};
    SignerCert signer = {sizeof(signer), 2, &store, nullptr};
    SignerSignatureInfo signature = {sizeof(signature), CALG_SHA_256, 0, nullptr, nullptr, nullptr};
    SignerContext* context = nullptr;
    HRESULT result = signerSignEx(0, &subject, &signer, &signature, nullptr, nullptr, nullptr,
                                  nullptr, &context);
    if (context) {
        freeContext(context);
    }
    FreeLibrary(mssign);
    if (FAILED(result)) {
        error = ErrorText(L"Couldn't sign the driver catalog", DWORD(result));
        return false;
    }
    return true;
}

// Every certificate this has made, out of the machine's root and trusted publisher stores.
void ForgetCertificates() {
    for (const wchar_t* name : {L"Root", L"TrustedPublisher"}) {
        HCERTSTORE store = CertOpenStore(CERT_STORE_PROV_SYSTEM_W, 0, 0,
                                         CERT_SYSTEM_STORE_LOCAL_MACHINE, name);
        if (!store) {
            continue;
        }
        while (PCCERT_CONTEXT found = CertFindCertificateInStore(
                   store, X509_ASN_ENCODING, 0, CERT_FIND_SUBJECT_STR_W, kCertificateName,
                   nullptr)) {
            CertDeleteCertificateFromStore(found);  // frees it
        }
        CertCloseStore(store, 0);
    }
}

// Trusts `certificate` on this PC, as a root and as a publisher of drivers.
bool TrustCertificate(PCCERT_CONTEXT certificate, std::wstring& error) {
    ForgetCertificates();
    // A copy without the key's whereabouts.
    PCCERT_CONTEXT copy = CertCreateCertificateContext(
        X509_ASN_ENCODING, certificate->pbCertEncoded, certificate->cbCertEncoded);
    bool ok = copy != nullptr;
    for (const wchar_t* name : {L"Root", L"TrustedPublisher"}) {
        HCERTSTORE store = ok ? CertOpenStore(CERT_STORE_PROV_SYSTEM_W, 0, 0,
                                              CERT_SYSTEM_STORE_LOCAL_MACHINE, name)
                              : nullptr;
        ok = store && CertAddCertificateContextToStore(store, copy,
                                                       CERT_STORE_ADD_REPLACE_EXISTING, nullptr);
        if (!ok) {
            error = ErrorText(L"Couldn't trust the driver's certificate");
        }
        if (store) {
            CertCloseStore(store, 0);
        }
    }
    if (copy) {
        CertFreeCertificateContext(copy);
    }
    return ok;
}

std::wstring TempFolder() {
    wchar_t temp[MAX_PATH + 1];
    DWORD length = GetTempPathW(MAX_PATH + 1, temp);
    return std::wstring(temp, length);
}

// Gazeora's driver packages in Windows' INF folder (oemNN.inf), by file name, with their text.
std::vector<std::pair<std::wstring, std::string>> InstalledPackages() {
    std::vector<std::pair<std::wstring, std::string>> packages;
    wchar_t windows[MAX_PATH];
    GetWindowsDirectoryW(windows, MAX_PATH);
    std::wstring folder = std::wstring(windows) + L"\\INF\\";
    WIN32_FIND_DATAW found;
    HANDLE search = FindFirstFileW((folder + L"oem*.inf").c_str(), &found);
    if (search == INVALID_HANDLE_VALUE) {
        return packages;
    }
    do {
        HANDLE file = CreateFileW((folder + found.cFileName).c_str(), GENERIC_READ,
                                  FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
        if (file == INVALID_HANDLE_VALUE) {
            continue;
        }
        char start[sizeof(kPackageMarker)] = {};
        DWORD read = 0;
        ReadFile(file, start, sizeof(start) - 1, &read, nullptr);
        if (strcmp(start, kPackageMarker) == 0) {
            std::string text(GetFileSize(file, nullptr), '\0');
            SetFilePointer(file, 0, nullptr, FILE_BEGIN);
            ReadFile(file, text.data(), DWORD(text.size()), &read, nullptr);
            text.resize(read);
            packages.emplace_back(found.cFileName, std::move(text));
        }
        CloseHandle(file);
    } while (FindNextFileW(search, &found));
    FindClose(search);
    return packages;
}

// The phones a package puts WinUSB on.
std::vector<std::wstring> PhoneIdsIn(const std::string& inf) {
    std::vector<std::wstring> ids;
    for (size_t at = inf.find(kPhoneEntry); at != std::string::npos;
         at = inf.find(kPhoneEntry, at + 1)) {
        size_t from = at + strlen(kPhoneEntry);
        std::string id = inf.substr(from, inf.find_first_of("\r\n", from) - from);
        std::wstring wide(id.begin(), id.end());
        if (std::find(ids.begin(), ids.end(), wide) == ids.end()) {
            ids.push_back(wide);
        }
    }
    return ids;
}

// Builds, signs and installs a package for the accessory and `phoneIds`, onto the devices
// plugged in now, and removes Gazeora's earlier packages.
bool InstallPackage(const std::vector<std::wstring>& phoneIds, std::wstring& error) {
    std::wstring folder = TempFolder() + L"GazeoraUsbDriver";
    CreateDirectoryW(folder.c_str(), nullptr);
    std::wstring inf = folder + L"\\" + kInfName;
    std::wstring cat = folder + L"\\" + kCatName;
    DeleteFileW(cat.c_str());
    SYSTEMTIME now;
    GetSystemTime(&now);
    char stamp[32];
    snprintf(stamp, sizeof(stamp), "%04u%02u%02u%02u%02u%02u%03u", now.wYear, now.wMonth,
             now.wDay, now.wHour, now.wMinute, now.wSecond, now.wMilliseconds);
    std::string text = BuildInf(phoneIds, stamp, now);
    if (!WriteBytes(inf, text.data(), text.size())) {
        error = ErrorText((L"Couldn't write the driver to " + folder).c_str());
        return false;
    }
    std::vector<std::pair<std::wstring, std::string>> earlier = InstalledPackages();

    // A new key each time, deleted as soon as the catalog is signed.
    std::wstring container = L"Gazeora USB driver " + std::to_wstring(GetCurrentProcessId()) +
                             L"-" + std::to_wstring(GetTickCount64());
    PCCERT_CONTEXT certificate = CreateSigningCertificate(container, error);
    bool ok = certificate && CreateCatalog(folder, error) &&
              SignCatalog(cat, certificate, error) && TrustCertificate(certificate, error);
    if (certificate) {
        CertFreeCertificateContext(certificate);
    }
    DeleteKey(container);

    if (ok) {
        // Into the driver store, and onto the devices plugged in now, even those whose own
        // driver Windows ranks higher (a phone's file transfer driver).
        auto installDriver = SystemFunction<DiInstallDriverFunction>(L"newdev.dll",
                                                                    "DiInstallDriverW");
        BOOL reboot = FALSE;
        if (!installDriver) {
            error = L"Windows' driver install (newdev.dll) isn't available";
            ok = false;
        } else if (!installDriver(nullptr, inf.c_str(), DIIRFLAG_FORCE_INF, &reboot) &&
                   GetLastError() != ERROR_NO_SUCH_DEVINST) {
            error = ErrorText(L"Windows didn't install the driver");
            ok = false;
        }
    }
    if (ok) {
        // The new package replaces the earlier ones, whose certificates are gone anyway.
        for (const auto& package : earlier) {
            SetupUninstallOEMInfW(package.first.c_str(), SUOI_FORCEDELETE, nullptr);
        }
    } else if (earlier.empty()) {
        ForgetCertificates();
    }
    DeleteFileW(inf.c_str());
    DeleteFileW(cat.c_str());
    RemoveDirectoryW(folder.c_str());
    return ok;
}

using GetParentFunction = CONFIGRET(WINAPI*)(PDEVINST, DEVINST, ULONG);
using GetDeviceIdFunction = CONFIGRET(WINAPI*)(DEVINST, PWSTR, ULONG, ULONG);

// The device's instance ID, e.g. USB\VID_2717&PID_FF40\MRMVCMDU8HR88XNR; for one interface of
// a device, the device's.
std::wstring DeviceInstanceId(DEVINST device) {
    auto getParent = SystemFunction<GetParentFunction>(L"cfgmgr32.dll", "CM_Get_Parent");
    auto getId = SystemFunction<GetDeviceIdFunction>(L"cfgmgr32.dll", "CM_Get_Device_IDW");
    wchar_t id[MAX_DEVICE_ID_LEN] = {};
    for (int depth = 0; getParent && getId && depth < 2; ++depth) {
        if (getId(device, id, MAX_DEVICE_ID_LEN, 0) != CR_SUCCESS) {
            return {};
        }
        if (!wcsstr(id, L"&MI_") || getParent(&device, device, 0) != CR_SUCCESS) {
            break;
        }
    }
    return id;
}

// Calls `visit` for each USB device (plugged in, or only those if `present`), with its
// hardware IDs (a list of strings, each ended by a zero).
template <typename Visit>
void ForEachUsbDevice(bool present, Visit visit) {
    HDEVINFO set = SetupDiGetClassDevsW(nullptr, L"USB", nullptr,
                                        DIGCF_ALLCLASSES | (present ? DIGCF_PRESENT : 0));
    if (set == INVALID_HANDLE_VALUE) {
        return;
    }
    SP_DEVINFO_DATA device = {sizeof(device)};
    for (DWORD i = 0; SetupDiEnumDeviceInfo(set, i, &device); ++i) {
        wchar_t ids[1024] = {};
        if (SetupDiGetDeviceRegistryPropertyW(set, &device, SPDRP_HARDWAREID, nullptr,
                                              reinterpret_cast<BYTE*>(ids), sizeof(ids) - 4,
                                              nullptr)) {
            visit(set, device, ids);
        }
    }
    SetupDiDestroyDeviceInfoList(set);
}

bool HasHardwareId(const wchar_t* ids, const std::vector<std::wstring>& wanted) {
    for (const wchar_t* id = ids; *id; id += wcslen(id) + 1) {
        for (const std::wstring& one : wanted) {
            if (_wcsicmp(id, one.c_str()) == 0) {
                return true;
            }
        }
    }
    return false;
}

// The serial numbers of the devices with any of `hardwareIds`: the last part of their instance
// ID, which a phone keeps in accessory mode.
std::vector<std::wstring> SerialsOf(const std::vector<std::wstring>& hardwareIds) {
    std::vector<std::wstring> serials;
    ForEachUsbDevice(false, [&](HDEVINFO, SP_DEVINFO_DATA& device, const wchar_t* ids) {
        if (!HasHardwareId(ids, hardwareIds)) {
            return;
        }
        std::wstring id = DeviceInstanceId(device.DevInst);
        size_t slash = id.rfind(L'\\');
        // Windows makes one up, with &s in it, for a device without a serial number.
        if (slash != std::wstring::npos && id.find(L'&', slash) == std::wstring::npos) {
            serials.push_back(id.substr(slash + 1));
        }
    });
    return serials;
}

// Power-cycles the USB port of each phone in accessory mode with one of `serials`, as if it
// were unplugged and plugged in again: it goes back to its normal mode. False if one couldn't.
bool ReconnectAccessories(const std::vector<std::wstring>& serials) {
    using GetListSizeFunction = CONFIGRET(WINAPI*)(PULONG, LPGUID, DEVINSTID_W, ULONG);
    using GetListFunction = CONFIGRET(WINAPI*)(LPGUID, DEVINSTID_W, PZZWSTR, ULONG, ULONG);
    auto getParent = SystemFunction<GetParentFunction>(L"cfgmgr32.dll", "CM_Get_Parent");
    auto getId = SystemFunction<GetDeviceIdFunction>(L"cfgmgr32.dll", "CM_Get_Device_IDW");
    auto getListSize = SystemFunction<GetListSizeFunction>(
        L"cfgmgr32.dll", "CM_Get_Device_Interface_List_SizeW");
    auto getList = SystemFunction<GetListFunction>(L"cfgmgr32.dll", "CM_Get_Device_Interface_ListW");
    if (!getParent || !getId || !getListSize || !getList) {
        return serials.empty();
    }
    GUID hubInterface = {0xF18A0E88, 0xC30C, 0x11D0,
                         {0x88, 0x15, 0x00, 0xA0, 0xC9, 0x06, 0xBE, 0xD8}};
    bool ok = true;
    ForEachUsbDevice(true, [&](HDEVINFO set, SP_DEVINFO_DATA& device, const wchar_t* ids) {
        // The accessory as a whole (18D1:2D00 to 2D05), not one of its interfaces.
        if (_wcsnicmp(ids, L"USB\\VID_18D1&PID_2D0", 20) != 0 || wcsstr(ids, L"&MI_")) {
            return;
        }
        std::wstring id = DeviceInstanceId(device.DevInst);
        size_t slash = id.rfind(L'\\');
        std::wstring serial = slash == std::wstring::npos ? L"" : id.substr(slash + 1);
        if (std::find(serials.begin(), serials.end(), serial) == serials.end()) {
            return;
        }
        // Its port on its hub, and the hub.
        DWORD port = 0;
        DEVINST hub = 0;
        wchar_t hubId[MAX_DEVICE_ID_LEN] = {};
        ULONG size = 0;
        bool cycled = false;
        if (SetupDiGetDeviceRegistryPropertyW(set, &device, SPDRP_ADDRESS, nullptr,
                                              reinterpret_cast<BYTE*>(&port), sizeof(port),
                                              nullptr) &&
            getParent(&hub, device.DevInst, 0) == CR_SUCCESS &&
            getId(hub, hubId, MAX_DEVICE_ID_LEN, 0) == CR_SUCCESS &&
            getListSize(&size, &hubInterface, hubId, CM_GET_DEVICE_INTERFACE_LIST_PRESENT) ==
                CR_SUCCESS &&
            size > 1) {
            std::vector<wchar_t> paths(size);
            if (getList(&hubInterface, hubId, paths.data(), size,
                        CM_GET_DEVICE_INTERFACE_LIST_PRESENT) == CR_SUCCESS) {
                HANDLE file = CreateFileW(paths.data(), GENERIC_WRITE, FILE_SHARE_WRITE, nullptr,
                                          OPEN_EXISTING, 0, nullptr);
                if (file != INVALID_HANDLE_VALUE) {
                    USB_CYCLE_PORT_PARAMS params = {port, 0};
                    DWORD returned = 0;
                    cycled = DeviceIoControl(file, IOCTL_USB_HUB_CYCLE_PORT, &params,
                                             sizeof(params), &params, sizeof(params), &returned,
                                             nullptr);
                    CloseHandle(file);
                }
            }
        }
        ok &= cycled;
    });
    return ok;
}

// Uninstalls the devices with any of `hardwareIds` (plugged in or remembered), and has Windows
// look at what's plugged in again: they get the driver it picks itself (file transfer).
void ReinstallDevices(const std::vector<std::wstring>& hardwareIds) {
    using DiUninstallDeviceFunction = BOOL(WINAPI*)(HWND, HDEVINFO, PSP_DEVINFO_DATA, DWORD,
                                                    PBOOL);
    auto uninstallDevice = SystemFunction<DiUninstallDeviceFunction>(L"newdev.dll",
                                                                    "DiUninstallDevice");
    HDEVINFO set = SetupDiGetClassDevsW(nullptr, L"USB", nullptr, DIGCF_ALLCLASSES);
    if (set != INVALID_HANDLE_VALUE && uninstallDevice) {
        SP_DEVINFO_DATA device = {sizeof(device)};
        for (DWORD i = 0; SetupDiEnumDeviceInfo(set, i, &device); ++i) {
            wchar_t ids[1024] = {};
            if (!SetupDiGetDeviceRegistryPropertyW(set, &device, SPDRP_HARDWAREID, nullptr,
                                                   reinterpret_cast<BYTE*>(ids), sizeof(ids) - 4,
                                                   nullptr)) {
                continue;
            }
            bool match = false;
            for (const wchar_t* id = ids; *id && !match; id += wcslen(id) + 1) {
                for (const std::wstring& wanted : hardwareIds) {
                    match |= _wcsicmp(id, wanted.c_str()) == 0;
                }
            }
            if (match) {
                BOOL reboot = FALSE;
                uninstallDevice(nullptr, set, &device, 0, &reboot);
            }
        }
    }
    if (set != INVALID_HANDLE_VALUE) {
        SetupDiDestroyDeviceInfoList(set);
    }
    using LocateFunction = CONFIGRET(WINAPI*)(PDEVINST, DEVINSTID_W, ULONG);
    using ReenumerateFunction = CONFIGRET(WINAPI*)(DEVINST, ULONG);
    auto locate = SystemFunction<LocateFunction>(L"cfgmgr32.dll", "CM_Locate_DevNodeW");
    auto reenumerate = SystemFunction<ReenumerateFunction>(L"cfgmgr32.dll",
                                                          "CM_Reenumerate_DevNode");
    DEVINST root = 0;
    if (locate && reenumerate && locate(&root, nullptr, CM_LOCATE_DEVNODE_NORMAL) == CR_SUCCESS) {
        reenumerate(root, CM_REENUMERATE_SYNCHRONOUS);
    }
}

// Runs Gazeora as an administrator with `command` and `arguments` (Windows asks the user
// first), and waits. `what` names it in errors.
bool RunElevated(HWND owner, const wchar_t* command, const std::wstring& arguments,
                 const wchar_t* what, std::wstring& error) {
    wchar_t exe[MAX_PATH * 2];
    GetModuleFileNameW(nullptr, exe, ARRAYSIZE(exe));
    std::wstring result = TempFolder() + L"gazeora_usb_driver_result.txt";
    DeleteFileW(result.c_str());
    std::wstring parameters = std::wstring(command) + L" \"" + result + L"\"" + arguments;

    SHELLEXECUTEINFOW info = {sizeof(info)};
    info.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC;
    info.hwnd = owner;
    info.lpVerb = L"runas";  // as an administrator: Windows asks first
    info.lpFile = exe;
    info.lpParameters = parameters.c_str();
    info.nShow = SW_HIDE;
    if (!ShellExecuteExW(&info)) {
        error = GetLastError() == ERROR_CANCELLED
                    ? std::wstring(what) + L" didn't happen: Windows needs an administrator's yes"
                    : ErrorText(L"Couldn't start as an administrator");
        return false;
    }
    WaitForSingleObject(info.hProcess, INFINITE);
    DWORD code = 1;
    GetExitCodeProcess(info.hProcess, &code);
    CloseHandle(info.hProcess);
    if (code == 0) {
        DeleteFileW(result.c_str());
        return true;
    }
    error = ReadText(result);
    DeleteFileW(result.c_str());
    if (error.empty()) {
        error = std::wstring(what) + L" failed";
    }
    return false;
}

}  // namespace

std::vector<std::wstring> InstalledPhoneIds() {
    std::vector<std::wstring> ids;
    for (const auto& package : InstalledPackages()) {
        for (const std::wstring& id : PhoneIdsIn(package.second)) {
            if (std::find(ids.begin(), ids.end(), id) == ids.end()) {
                ids.push_back(id);
            }
        }
    }
    return ids;
}

bool InstallUsbDriver(const std::vector<std::wstring>& phoneIds, std::wstring& error) {
    // The phones added before stay in.
    std::vector<std::wstring> ids = InstalledPhoneIds();
    for (const std::wstring& id : phoneIds) {
        if (std::find(ids.begin(), ids.end(), id) == ids.end()) {
            ids.push_back(id);
        }
    }
    return InstallPackage(ids, error);
}

bool RestoreFileTransfer(std::wstring& error) {
    std::vector<std::wstring> phones = InstalledPhoneIds();
    std::vector<std::wstring> serials = SerialsOf(phones);  // before their devices go
    if (!InstallPackage({}, error)) {
        return false;
    }
    ReinstallDevices(phones);
    // A phone streaming now stays in accessory mode until it's disconnected: reconnect it.
    if (!ReconnectAccessories(serials)) {
        error = L"File transfer is restored, but Gazeora couldn't reconnect the phone that's "
                L"streaming: unplug it and plug it in again to use file transfer.";
        return false;
    }
    return true;
}

bool InstallUsbDriverElevated(HWND owner, const std::vector<std::wstring>& phoneIds,
                              std::wstring& error) {
    std::wstring arguments;
    for (const std::wstring& id : phoneIds) {
        arguments += L" \"" + id + L"\"";
    }
    return RunElevated(owner, kInstallUsbDriverArgument, arguments, L"The driver install", error);
}

bool RestoreFileTransferElevated(HWND owner, std::wstring& error) {
    return RunElevated(owner, kRestoreFileTransferArgument, L"", L"Restoring file transfer",
                       error);
}

int RunUsbDriverCommand() {
    int count = 0;
    LPWSTR* arguments = CommandLineToArgvW(GetCommandLineW(), &count);
    if (!arguments) {
        return -1;
    }
    bool install = count >= 2 && wcscmp(arguments[1], kInstallUsbDriverArgument) == 0;
    bool restore = count >= 2 && wcscmp(arguments[1], kRestoreFileTransferArgument) == 0;
    if (!install && !restore) {
        LocalFree(arguments);
        return -1;
    }
    std::wstring result = count >= 3 ? arguments[2] : L"";
    std::vector<std::wstring> phoneIds(arguments + std::min(count, 3), arguments + count);
    LocalFree(arguments);
    std::wstring error;
    bool ok = install ? InstallUsbDriver(phoneIds, error) : RestoreFileTransfer(error);
    if (!ok && !result.empty()) {
        WriteBytes(result, error.data(), error.size() * sizeof(wchar_t));
    }
    return ok ? 0 : 1;
}

}  // namespace gazeora::app
