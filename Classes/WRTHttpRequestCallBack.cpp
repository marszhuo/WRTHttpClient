#include "WRTHttpRequestCallBack.h"
#include "WRTHttpResponse.h"
#include "WRTHttpRequest.h"

HttpRequestCallback::HttpRequestCallback(IXMLHTTPRequest2* httpRequest,WRTHttpResponse* response,cancellation_token ct )
	: request(httpRequest), cancellationToken(ct),_httpResponse(response)
{
	// 注册回调方法，用以当取消token无效时终止http请求
    if (cancellationToken != cancellation_token::none())
    {
        registrationToken = cancellationToken.register_callback([this]() 
        {
            if (request != nullptr) 
            {
                request->Abort();
            }
        });
    }	
}

 HttpRequestCallback::~HttpRequestCallback()
{
    // 注销回调
    if (cancellationToken != cancellation_token::none())
    {
        cancellationToken.deregister_callback(registrationToken);
    }
}
// 重定向
IFACEMETHODIMP HttpRequestCallback::OnRedirect(IXMLHTTPRequest2*, PCWSTR) 
{
    return S_OK;
}

// 请求返回头信息回调
IFACEMETHODIMP HttpRequestCallback::OnHeadersAvailable(IXMLHTTPRequest2*, DWORD statusCode, PCWSTR reasonPhrase)
{
    HRESULT hr = S_OK;
    // 不需要将异常回传给IXHR2.
    try
    {
		if(_httpResponse)
        {
			_httpResponse->setSucceed(true);
			_httpResponse->setResponseCode(statusCode);
			_httpResponse->setReasonPhrase(reasonPhrase);			
		}
    }
    catch (std::bad_alloc&)
    {
        hr = E_OUTOFMEMORY;
    }

    return hr;
}

// 接收到返回数据回调
IFACEMETHODIMP HttpRequestCallback::OnDataAvailable(IXMLHTTPRequest2*, ISequentialStream* responseStream)
{
	HRESULT hr = S_OK;
    // 不需要讲异常回传给IXHR2.
    try
    {
		if(_httpResponse)
		{
			hr = ReadData(responseStream, _httpResponse->getResponseData() );			
		}		
    }
    catch (std::bad_alloc&)
    {
        hr = E_OUTOFMEMORY;
    }

    return hr;
}
        
// 接收到返回数据回调
IFACEMETHODIMP HttpRequestCallback::OnResponseReceived(IXMLHTTPRequest2*, ISequentialStream* responseStream)
{   
	HRESULT hr = S_OK;
    // 不需要讲异常回传给IXHR2.
    try
    {
		if(_httpResponse)
		{
			hr = ReadData(responseStream, _httpResponse->getResponseData() );
			completionEvent.set(hr);
		}		
    }
    catch (std::bad_alloc&)
    {
        hr = E_OUTOFMEMORY;
    }

    return hr;
}

// 请求错误回调
IFACEMETHODIMP HttpRequestCallback::OnError(IXMLHTTPRequest2*, HRESULT hrError) 
{
    HRESULT hr = S_OK;	
    // 不要将异常回传给IXHR2.
    try
    {
		hr = hrError ;
        completionEvent.set(hrError);
    }
    catch (std::bad_alloc&)
    {
        hr = E_OUTOFMEMORY;
    }

    return hr;
}

       
HRESULT HttpRequestCallback::ReadData(ISequentialStream* readStream, std::vector<char>* outdata)
{
	if(!outdata) return -1;

    HRESULT hr;   
    while (true)
    {
        ULONG readedCount = 0 ;
        char buffer[4096];
		// 读取utf-8格式的响应数据由于utf-8字符的长度限制，必须要确保只读取整数字符，需要从4093 B处开始
        hr = readStream->Read(buffer, sizeof(buffer) - 3, &readedCount);
        if (FAILED(hr) || (readedCount == 0)) {
            break; // 错误或无数据处理，退出循环.
        }

        if (readedCount == sizeof(buffer) - 3)
        {
            ULONG subsequentBytesRead;
            unsigned int i, cl;

			// 定位到最后一个utf-8字符的第一个字节
            for (i = readedCount - 1; (i >= 0) && ((buffer[i] & 0xC0) == 0x80); i--);

            // Calculate the number of subsequent bytes in the UTF-8 character.
			// 计算utf-8格式字符序列的字节数
            if (((unsigned char)buffer[i]) < 0x80) {
                cl = 1;
            } else if (((unsigned char)buffer[i]) < 0xE0){
                cl = 2;
            } else if (((unsigned char)buffer[i]) < 0xF0){
                cl = 3;
            } else {
                cl = 4;
            }

            // 读取剩余字节
            if (readedCount < i + cl)
            {
                hr = readStream->Read(buffer + readedCount, i + cl - readedCount, &subsequentBytesRead);
                if (FAILED(hr)){
                    break;
                }
                readedCount += subsequentBytesRead;
            }
        }		

		outdata->insert(outdata->end(),buffer,buffer+readedCount);
    }

    return (SUCCEEDED(hr)) ? S_OK : hr;
}

