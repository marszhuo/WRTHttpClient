#include "WRTHttpRequestCallBack.h"
#include "WRTHttpResponse.h"
#include "WRTHttpRequest.h"

HttpRequestCallback::HttpRequestCallback(IXMLHTTPRequest2* httpRequest,WRTHttpResponse* response,cancellation_token ct )
	: request(httpRequest), cancellationToken(ct),_httpResponse(response)
{
	// ע��ص����������Ե�ȡ��token��Чʱ��ֹhttp����
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
    // ע���ص�
    if (cancellationToken != cancellation_token::none())
    {
        cancellationToken.deregister_callback(registrationToken);
    }
}
// �ض���
IFACEMETHODIMP HttpRequestCallback::OnRedirect(IXMLHTTPRequest2*, PCWSTR) 
{
    return S_OK;
}

// ���󷵻�ͷ��Ϣ�ص�
IFACEMETHODIMP HttpRequestCallback::OnHeadersAvailable(IXMLHTTPRequest2*, DWORD statusCode, PCWSTR reasonPhrase)
{
    HRESULT hr = S_OK;
    // ����Ҫ���쳣�ش���IXHR2.
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

// ���յ��������ݻص�
IFACEMETHODIMP HttpRequestCallback::OnDataAvailable(IXMLHTTPRequest2*, ISequentialStream* responseStream)
{
	HRESULT hr = S_OK;
    // ����Ҫ���쳣�ش���IXHR2.
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
        
// ���յ��������ݻص�
IFACEMETHODIMP HttpRequestCallback::OnResponseReceived(IXMLHTTPRequest2*, ISequentialStream* responseStream)
{   
	HRESULT hr = S_OK;
    // ����Ҫ���쳣�ش���IXHR2.
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

// �������ص�
IFACEMETHODIMP HttpRequestCallback::OnError(IXMLHTTPRequest2*, HRESULT hrError) 
{
    HRESULT hr = S_OK;	
    // ��Ҫ���쳣�ش���IXHR2.
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
		// ��ȡutf-8��ʽ����Ӧ��������utf-8�ַ��ĳ������ƣ�����Ҫȷ��ֻ��ȡ�����ַ�����Ҫ��4093 B����ʼ
        hr = readStream->Read(buffer, sizeof(buffer) - 3, &readedCount);
        if (FAILED(hr) || (readedCount == 0)) {
            break; // ����������ݴ����˳�ѭ��.
        }

        if (readedCount == sizeof(buffer) - 3)
        {
            ULONG subsequentBytesRead;
            unsigned int i, cl;

			// ��λ�����һ��utf-8�ַ��ĵ�һ���ֽ�
            for (i = readedCount - 1; (i >= 0) && ((buffer[i] & 0xC0) == 0x80); i--);

            // Calculate the number of subsequent bytes in the UTF-8 character.
			// ����utf-8��ʽ�ַ����е��ֽ���
            if (((unsigned char)buffer[i]) < 0x80) {
                cl = 1;
            } else if (((unsigned char)buffer[i]) < 0xE0){
                cl = 2;
            } else if (((unsigned char)buffer[i]) < 0xF0){
                cl = 3;
            } else {
                cl = 4;
            }

            // ��ȡʣ���ֽ�
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

