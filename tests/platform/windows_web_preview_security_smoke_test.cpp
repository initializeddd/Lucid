#include "platform/browser/WindowsWebPreviewSession.hpp"
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <chrono>
#include <string>
#include <thread>
#include <vector>
using Microsoft::WRL::ComPtr;
static bool redCenter(const std::string& png) {
    ComPtr<IStream> stream; if (FAILED(CreateStreamOnHGlobal(nullptr,TRUE,&stream))) return false;
    ULONG written=0; if (FAILED(stream->Write(png.data(),static_cast<ULONG>(png.size()),&written)) || written!=png.size()) return false;
    LARGE_INTEGER zero{}; if (FAILED(stream->Seek(zero,STREAM_SEEK_SET,nullptr))) return false;
    ComPtr<IWICImagingFactory> factory;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_IWICImagingFactory,reinterpret_cast<void**>(factory.GetAddressOf())))) return false;
    ComPtr<IWICBitmapDecoder> decoder; if (FAILED(factory->CreateDecoderFromStream(stream.Get(),nullptr,WICDecodeMetadataCacheOnLoad,&decoder))) return false;
    ComPtr<IWICBitmapFrameDecode> frame; if (FAILED(decoder->GetFrame(0,&frame))) return false;
    UINT width=0,height=0; if (FAILED(frame->GetSize(&width,&height)) || !width || !height || width>2048 || height>2048) return false;
    ComPtr<IWICFormatConverter> converter; if (FAILED(factory->CreateFormatConverter(&converter)) || FAILED(converter->Initialize(frame.Get(),GUID_WICPixelFormat32bppRGBA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom))) return false;
    WICRect pixel{static_cast<INT>(width/2),static_cast<INT>(height/2),1,1}; BYTE rgba[4]{};
    return SUCCEEDED(converter->CopyPixels(&pixel,4,4,rgba)) && rgba[0]>240 && rgba[1]<15 && rgba[2]<15;
}
int main() {
    wchar_t enabled[4]{}; if (!GetEnvironmentVariableW(L"LUCID_WEBVIEW2_SMOKE",enabled,4) || std::wstring(enabled)!=L"1") return 0;
    wchar_t loader[32768]{}; const DWORD n=GetEnvironmentVariableW(L"LUCID_WEBVIEW2_LOADER",loader,32768); if (!n || n>=32768) return 1;
    WSADATA winsock{}; if (WSAStartup(MAKEWORD(2,2),&winsock)) return 2;
    SOCKET server=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP); if (server==INVALID_SOCKET) { WSACleanup(); return 3; }
    sockaddr_in address{}; address.sin_family=AF_INET; address.sin_addr.s_addr=htonl(INADDR_LOOPBACK); address.sin_port=0;
    int length=sizeof(address); if (bind(server,reinterpret_cast<sockaddr*>(&address),sizeof(address)) || listen(server,4) || getsockname(server,reinterpret_cast<sockaddr*>(&address),&length)) { closesocket(server); WSACleanup(); return 4; }
    u_long nonblocking=1; if (ioctlsocket(server,FIONBIO,&nonblocking)) { closesocket(server); WSACleanup(); return 5; }
    if (FAILED(CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED))) { closesocket(server); WSACleanup(); return 6; }
    int status=0;
    {
        HWND window=CreateWindowExW(0,L"STATIC",L"Lucid WebView2 network test",WS_OVERLAPPEDWINDOW,0,0,400,300,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
        if (!window) status=7;
        else {
            ShowWindow(window,SW_SHOW); lucid::modules::WebPreviewAssets assets; std::string error; std::vector<lucid::modules::WebPreviewAsset> files;
            const auto origin=std::string("127.0.0.1:")+std::to_string(ntohs(address.sin_port));
            files.push_back({"/index.html","<!doctype html><html><body style=\"margin:0;background:black\"><script src=\"/app.js\"></script></body></html>"});
            files.push_back({"/app.js","document.body.style.background='rgb(255,0,0)'; fetch('http://"+origin+"/fetch').catch(()=>{}); try { new WebSocket('ws://"+origin+"/socket'); } catch(e) {} window.open('http://"+origin+"/popup');"});
            lucid::platform::WindowsWebPreviewSession session;
            if (!assets.load(std::move(files),error) || !session.start(window,loader,std::move(assets),"/index.html",256,128,true,error)) status=8;
            else {
                auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(25);
                while (!session.finished() && std::chrono::steady_clock::now()<deadline) { MSG msg{}; while (PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); } session.poll(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
                if (!session.finished() || !session.result().error.empty() || !redCenter(session.result().png)) status=9;
                if (session.running()) session.cancel();
                deadline=std::chrono::steady_clock::now()+std::chrono::seconds(15);
                while (!session.retainedProfile().empty() && std::chrono::steady_clock::now()<deadline) { MSG msg{}; while (PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); } session.poll(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
                if (!session.retainedProfile().empty() && !status) status=10;
                const SOCKET accepted=accept(server,nullptr,nullptr); if (accepted!=INVALID_SOCKET) { closesocket(accepted); status=11; }
            }
            DestroyWindow(window);
        }
    }
    CoUninitialize(); closesocket(server); WSACleanup(); return status;
}
#else
int main() { return 0; }
#endif
