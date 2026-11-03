/*
 * KAMI Second Life Windows login device identity compatibility module.
 *
 * Adapted from the volume serial hashing behavior in Linden Lab's
 * official viewer, indra/newview/llappviewerwin32.cpp,
 * LLAppViewerWin32::generateSerialNumber(), at:
 * https://github.com/secondlife/viewer/tree/b645b767684ccabdfd285f0c2a55855697029909
 *
 * Original viewer component: Copyright (C) Linden Research, Inc.
 * This adapted compatibility module is available under GNU LGPL v2.1
 * (or, at your option, a later version) and is deliberately built as
 * a standalone DLL to retain KAMI's separate software boundary.
 * Full terms: third_party/secondlife-lgpl/LICENSE.LGPL-2.1.txt.
 *
 * No raw volume, motherboard or OS serial is transmitted. The DLL also
 * adapts llmachineid.cpp + llhasheduniqueid.cpp's WMI-first 6-byte
 * folded machine ID hashing. These are pseudonymous identifiers, not
 * cryptographic security or password hashing.
 */
#include "kami/official_login_identity.hpp"
#include <windows.h>
#include <bcrypt.h>
#include <array>
#include <cstring>
#include <cwchar>
#include <string>
#include <string_view>
#include <wbemidl.h>
#include <wrl/client.h>
#include <oleauto.h>

extern "C" bool kami_sl_hash_volume_serial(
    std::uint32_t serial,char* output,std::size_t capacity) noexcept
{
    if(!output||capacity<33)return false;
    output[0]='\0';
    BCRYPT_ALG_HANDLE provider=nullptr;
    if(!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(
        &provider,BCRYPT_MD5_ALGORITHM,nullptr,0)))return false;

    // On Windows, DWORD is 32 bits and uses little-endian storage. This
    // matches official LLMD5::update(&serial, sizeof(DWORD)).
    std::array<unsigned char,sizeof(std::uint32_t)> volume_bytes{};
    static_assert(sizeof(DWORD)==sizeof(std::uint32_t));
    std::memcpy(volume_bytes.data(),&serial,volume_bytes.size());
    std::array<unsigned char,16> digest{};
    const NTSTATUS status=BCryptHash(
        provider,nullptr,0,volume_bytes.data(),
        static_cast<ULONG>(volume_bytes.size()),digest.data(),
        static_cast<ULONG>(digest.size()));
    BCryptCloseAlgorithmProvider(provider,0);
    SecureZeroMemory(volume_bytes.data(),volume_bytes.size());
    if(!BCRYPT_SUCCESS(status)){
        SecureZeroMemory(digest.data(),digest.size());
        return false;
    }
    constexpr char digits[]="0123456789abcdef";
    for(std::size_t i=0;i<digest.size();++i){
        output[i*2]=digits[digest[i]>>4];
        output[i*2+1]=digits[digest[i]&15U];
    }
    output[32]='\0';
    SecureZeroMemory(digest.data(),digest.size());
    return true;
}

extern "C" bool kami_sl_host_volume_id(
    char* output,std::size_t capacity) noexcept
{
    if(!output||capacity<33)return false;
    output[0]='\0';
    DWORD volume_serial=0;
    // Official SL Windows viewer hashes this 4-byte C: volume serial.
    const BOOL ok=GetVolumeInformationW(L"C:\\",nullptr,0,
        &volume_serial,nullptr,nullptr,nullptr,0);
    if(!ok)return false;
    const bool result=kami_sl_hash_volume_serial(
        static_cast<std::uint32_t>(volume_serial),output,capacity);
    SecureZeroMemory(&volume_serial,sizeof(volume_serial));
    return result;
}

namespace {
using Microsoft::WRL::ComPtr;
class BStr {
public:
    BSTR value=nullptr;
    explicit BStr(const wchar_t* text):value(SysAllocString(text)){}
    ~BStr(){if(value)SysFreeString(value);}
    BStr(const BStr&)=delete;
    BStr& operator=(const BStr&)=delete;
};
bool uuid_like(std::wstring_view value) noexcept {
    if(value.size()!=36)return false;
    bool all_zero=true,all_f=true;
    for(std::size_t i=0;i<value.size();++i){
        if(i==8||i==13||i==18||i==23){
            if(value[i]!=L'-')return false;
            continue;
        }
        const wchar_t ch=value[i];
        const bool hex=(ch>=L'0'&&ch<=L'9')||
            (ch>=L'A'&&ch<=L'F')||(ch>=L'a'&&ch<=L'f');
        if(!hex)return false;
        if(ch!=L'0')all_zero=false;
        if(ch!=L'F'&&ch!=L'f')all_f=false;
    }
    return !all_zero&&!all_f;
}
bool md5_raw_bytes(const unsigned char* bytes,ULONG byte_count,
                   char* output,std::size_t capacity) noexcept {
    if(!bytes||!output||capacity<33)return false;
    output[0]='\0';
    BCRYPT_ALG_HANDLE provider=nullptr;
    if(!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(
         &provider,BCRYPT_MD5_ALGORITHM,nullptr,0)))return false;
    std::array<unsigned char,16> digest{};
    const NTSTATUS result=BCryptHash(provider,nullptr,0,
        const_cast<PUCHAR>(bytes),byte_count,digest.data(),
        static_cast<ULONG>(digest.size()));
    BCryptCloseAlgorithmProvider(provider,0);
    if(!BCRYPT_SUCCESS(result)){
        SecureZeroMemory(digest.data(),digest.size());
        return false;
    }
    constexpr char hex[]="0123456789abcdef";
    for(std::size_t i=0;i<digest.size();++i){
        output[2*i]=hex[digest[i]>>4];
        output[2*i+1]=hex[digest[i]&15];
    }
    output[32]='\0';
    SecureZeroMemory(digest.data(),digest.size());
    return true;
}
bool fetch_wmi_serial(const wchar_t* query,const wchar_t* property,
                      std::wstring& output){
    output.clear();
    // Called from KAMI's existing COM-initialized async auth worker.
    ComPtr<IWbemLocator> locator;
    if(FAILED(CoCreateInstance(CLSID_WbemLocator,nullptr,
       CLSCTX_INPROC_SERVER,IID_PPV_ARGS(locator.GetAddressOf()))))
        return false;
    BStr root(L"ROOT\\CIMV2");
    ComPtr<IWbemServices> service;
    if(FAILED(locator->ConnectServer(root.value,nullptr,nullptr,nullptr,
                0,nullptr,nullptr,service.GetAddressOf())))
        return false;
    if(FAILED(CoSetProxyBlanket(service.Get(),RPC_C_AUTHN_WINNT,
         RPC_C_AUTHZ_NONE,nullptr,RPC_C_AUTHN_LEVEL_CALL,
         RPC_C_IMP_LEVEL_IMPERSONATE,nullptr,EOAC_NONE)))
        return false;
    BStr lang(L"WQL"),statement(query);
    ComPtr<IEnumWbemClassObject> results;
    if(FAILED(service->ExecQuery(lang.value,statement.value,
            WBEM_FLAG_FORWARD_ONLY|WBEM_FLAG_RETURN_IMMEDIATELY,
            nullptr,results.GetAddressOf()))||!results)
        return false;
    ComPtr<IWbemClassObject> entry;
    ULONG returned=0;
    // Do not hang viewer login indefinitely on a broken WMI provider.
    if(results->Next(2000,1,entry.GetAddressOf(),&returned)!=WBEM_S_NO_ERROR
       ||returned!=1||!entry)return false;
    VARIANT value;VariantInit(&value);
    const HRESULT hr=entry->Get(property,0,&value,nullptr,nullptr);
    if(SUCCEEDED(hr)&&value.vt==VT_BSTR&&value.bstrVal){
        const auto length=SysStringLen(value.bstrVal);
        if(length>0&&length<256)
            output.assign(value.bstrVal,value.bstrVal+length);
    }
    VariantClear(&value);
    return !output.empty();
}
} // namespace

extern "C" bool kami_sl_hash_machine_serial(
    const wchar_t* serial,char* output,std::size_t capacity) noexcept {
    if(!serial||!output||capacity<33)return false;
    output[0]='\0';
    const std::size_t n=wcsnlen(serial,256);
    if(n==0||n>=256)return false;
    std::array<unsigned char,6> folded{};
    // Same wraparound sum over successive groups of six UTF-16 code units
    // as official LLMachineID::getGenericSerialNumber in llmachineid.cpp.
    for(std::size_t i=0;i<n;++i)
        folded[i%folded.size()]=static_cast<unsigned char>(
            folded[i%folded.size()]+serial[i]);
    const bool result=md5_raw_bytes(folded.data(),
        static_cast<ULONG>(folded.size()),output,capacity);
    SecureZeroMemory(folded.data(),folded.size());
    return result;
}
extern "C" bool kami_sl_host_machine_hash(
    char* output,std::size_t capacity) noexcept {
    if(!output||capacity<33)return false;
    output[0]='\0';
    std::wstring serial;
    // Linden Lab's Windows priority: BIOS/motherboard product UUID first,
    // operating-system serial second. Physical adapter is a caller fallback.
    if(fetch_wmi_serial(L"SELECT UUID FROM Win32_ComputerSystemProduct",
                        L"UUID",serial)&&uuid_like(serial)){
        const bool ok=kami_sl_hash_machine_serial(serial.c_str(),
                                                   output,capacity);
        SecureZeroMemory(serial.data(),serial.size()*sizeof(wchar_t));
        return ok;
    }
    if(!serial.empty())SecureZeroMemory(serial.data(),serial.size()*sizeof(wchar_t));
    serial.clear();
    if(fetch_wmi_serial(L"SELECT SerialNumber FROM Win32_OperatingSystem",
                        L"SerialNumber",serial)){
        const bool ok=kami_sl_hash_machine_serial(serial.c_str(),
                                                   output,capacity);
        SecureZeroMemory(serial.data(),serial.size()*sizeof(wchar_t));
        return ok;
    }
    return false;
}
