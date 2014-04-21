//-----------------------------------------------------------------------
// 
//  Copyright (C) Microsoft Corporation.  All rights reserved.
// 
// 
// THIS CODE AND INFORMATION ARE PROVIDED AS-IS WITHOUT WARRANTY OF ANY
// KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//-----------------------------------------------------------------------
//#include "pch.h"
#include "WRTHttpRequest.h"
#include "WRTHttpResponse.h"
#include "WRTHttpRequestCallBack.h"
#include "WRTHttpClient.h"
#include <robuffer.h>

void CheckHResult(HRESULT hResult)
{
    if (hResult == E_ABORT)
    {
        concurrency::cancel_current_task();
    }
    else if (FAILED(hResult))
    {
        throw Platform::Exception::CreateException(hResult);
    }
}

WRTHttpRequest::WRTHttpRequest() : responseComplete(true)
{
	_requestType = kHttpUnkown;
    _url.clear();
    _requestData.clear();
    _tag.clear();
    _pTarget = NULL;
    _pSelector = NULL;
    _pUserData = NULL;
	_response = NULL;
}

WRTHttpRequest::~WRTHttpRequest()
{
	if (_pTarget)
    {
        _pTarget->release();
    }
}

void  WRTHttpRequest::send(WRTHttpResponse* response)
{
	_response = response;
	std::wstring sURL;
	Uri^ uri;	
	switch (getRequestType())
    {
        case WRTHttpRequest::kHttpGet:	// HTTP GET
		{			
			sURL=stringToWstring(_url.c_str(),sURL);
			String^ tUrl= ref new String(sURL.c_str());					
			convertURL(tUrl,&uri);				
            GetAsync(uri);
		}
            break;            
        case WRTHttpRequest::kHttpPost: // HTTP POST
		{
			sURL=stringToWstring(_url.c_str(),sURL);
			String^ tUrl= ref new String(sURL.c_str());					
			convertURL(tUrl,&uri);
			std::wstring content;
			content=stringToWstring(_requestData.data(),content);
            PostAsync(uri,content);
		}
            break;

        case WRTHttpRequest::kHttpPut:
                
            break;

        case WRTHttpRequest::kHttpDelete:
                
            break;            
        default:
            CCAssert(true, "CCHttpClient: unkown request type, only GET and POSt are supported");
            break;
    }                
}
 

// �����ƶ������ʹ����������񣬷���������http��Ӧ����
task<HRESULT> WRTHttpRequest::DownloadAsync(PCWSTR httpMethod, 
											PCWSTR uri, 
											cancellation_token cancellationToken,
											PCWSTR contentType, 
											IStream* postStream, 
											uint64 postStreamSizeToSend)
{
	// ����IXMLHTTPRequest2����
    ComPtr<IXMLHTTPRequest2> xhr;
    CheckHResult(CoCreateInstance(CLSID_XmlHttpRequest, nullptr, CLSCTX_INPROC, IID_PPV_ARGS(&xhr)));

    // �����ص�
    auto httpCallback = Make<HttpRequestCallback>(xhr.Get(),_response,cancellationToken);
    CheckHResult(httpCallback ? S_OK : E_OUTOFMEMORY);

    auto completionTask = create_task(httpCallback->GetCompletionEvent());

    // ��������
    CheckHResult(xhr->Open(httpMethod, uri, httpCallback.Get(), nullptr, nullptr, nullptr, nullptr));

    if (postStream != nullptr && contentType != nullptr)
    {
        CheckHResult(xhr->SetRequestHeader(L"Content-Type", contentType));
    }

    // ��������
    CheckHResult(xhr->Send(postStream, postStreamSizeToSend));

	// http�������ʱ�������task�������������Ҫ���ûص�������Ϊ��ȷ������������ȷ���ؽ��ص�������Ϊ�������ݽ���
    return completionTask.then([this, httpCallback](HRESULT hr)
    {
		if(SUCCEEDED(hr))
			WRTHttpClient::getInstance()->response(_response);        	
        return hr;
    });
}



// ����һ��GET����
task<HRESULT> WRTHttpRequest::GetAsync(Uri^ uri, 
									   cancellation_token cancellationToken)
{
    return DownloadAsync(L"GET",
                         uri->AbsoluteUri->Data(),
                         cancellationToken,
                         nullptr,
                         nullptr,
                         0 );
}

// ����POST����string
task<HRESULT> WRTHttpRequest::PostAsync(Uri^ uri, 
										const wstring& body, 
										cancellation_token cancellationToken)
{
    int length = 0;
    ComPtr<IStream> postStream;
    CreateMemoryStream(&postStream);

    if (body.length() > 0)
    {
        // �����Ҫ��buffer��С
        int size = WideCharToMultiByte(CP_UTF8,                         // UTF-8
                                       0,                               // Conversion type
                                       body.c_str(),                    // Unicode string to convert
                                       static_cast<int>(body.length()), // Size
                                       nullptr,                         // Output buffer
                                       0,                               // Output buffer size
                                       nullptr,
                                       nullptr);
        CheckHResult((size != 0) ? S_OK : HRESULT_FROM_WIN32(GetLastError()));

        std::unique_ptr<char[]> tempData(new char[size]);
        length = WideCharToMultiByte(CP_UTF8,                         // UTF-8
                                     0,                               // Conversion type
                                     body.c_str(),                    // Unicode string to convert
                                     static_cast<int>(body.length()), // Size
                                     tempData.get(),                  // Output buffer
                                     size,                            // Output buffer size
                                     nullptr,
                                     nullptr);
        CheckHResult((length != 0) ? S_OK : HRESULT_FROM_WIN32(GetLastError()));
        CheckHResult(postStream->Write(tempData.get(), length, nullptr));
    }

    return DownloadAsync(L"POST",
                         uri->AbsoluteUri->Data(),
                         cancellationToken,
                         L"text/plain;charset=utf-8",
                         postStream.Get(),
                         length);
}

// ����POST����stream
task<HRESULT> WRTHttpRequest::PostAsync(Uri^ uri, 
										PCWSTR contentType, 
										IStream* postStream,    
										uint64 postStreamSizeToSend, 
										cancellation_token cancellationToken)
{
    return DownloadAsync(L"POST", 
						uri->AbsoluteUri->Data(), 
						cancellationToken, 
						contentType, 
						postStream, 
						postStreamSizeToSend);
}

void WRTHttpRequest::CreateMemoryStream(IStream **stream)
{	
	CheckHResult(::CreateStreamOnHGlobal(0, TRUE, stream));
}

//��url�ַ���ת��ΪWindows::Foundation::Uri
//pURLStr��url�ַ���
bool WRTHttpRequest::convertURL(String^ pURLStr,Uri^* uri)
{
	try
    {
		const char16* first = pURLStr->Begin();
        const char16* last = pURLStr->End();
        
        while (first != last && iswspace(*first))
        {
            ++first;
        }
        
        while (first != last && iswspace(last[-1]))
        {
            --last;
        }
		String^ tStr=ref new String(first, (unsigned int)(last - first));
        *uri = ref new Uri(tStr);
        return true;
    }
    catch (NullReferenceException^ exception)
    {
		CCLOG("Error: URI must not be null or empty.");
    }
    catch (InvalidArgumentException^ exception)
    {
		CCLOG("Error: Invalid URI");
    }	
    return false;	
}


//��stringת����wstring
std::wstring & WRTHttpRequest::stringToWstring(const std::string &src,std::wstring &dest)
{
	//std::setlocale(LC_CTYPE, "");

	size_t const wcs_len = mbstowcs(NULL, src.c_str(), 0);
    std::vector<wchar_t> tmp(wcs_len + 1);
    mbstowcs(&tmp[0], src.c_str(), src.size());

    dest.assign(tmp.begin(), tmp.end() - 1);

    return dest;
}

//��wstringת����string
std::string & WRTHttpRequest::wstringToString(const std::wstring &src,std::string &dest)
{
	//std::setlocale(LC_CTYPE, "");
    size_t const mbs_len = wcstombs(NULL, src.c_str(), 0);
    std::vector<char> tmp(mbs_len + 1);
    wcstombs(&tmp[0], src.c_str(), tmp.size());

    dest.assign(tmp.begin(), tmp.end() - 1);

    return dest;
}
