/****************************************************************************
 Copyright (c) 2010-2012 cocos2d-x.org
 
 http://www.cocos2d-x.org
 
 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:
 
 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 ****************************************************************************/

#ifndef __HTTP_REQUEST_CALLBACK__
#define __HTTP_REQUEST_CALLBACK__

#include <msxml6.h>
#include <collection.h>
#include <ppltasks.h>
#include <wrl.h>
#include <sstream>

using namespace concurrency;
using namespace Microsoft::WRL;
using namespace Platform;
using namespace std;
using namespace Windows::Foundation;
using namespace Windows::Storage::Streams;

class WRTHttpResponse;

// 需要完整响应数据的IXMLHTTPRequest2Callback回调类，对于边接收边处理的请求，采用HttpRequestBuffersCallback类
class HttpRequestCallback : public RuntimeClass<RuntimeClassFlags<ClassicCom>, IXMLHTTPRequest2Callback, FtmBase>
{
public:
    HttpRequestCallback(IXMLHTTPRequest2* httpRequest, WRTHttpResponse* response,
        cancellation_token ct = concurrency::cancellation_token::none());
		
	~HttpRequestCallback();

    // 重定向
    IFACEMETHODIMP OnRedirect(IXMLHTTPRequest2*, PCWSTR) ;
    // 请求返回头信息回调
    IFACEMETHODIMP OnHeadersAvailable(IXMLHTTPRequest2*, DWORD statusCode, PCWSTR reasonPhrase) ;
    // 接收到返回数据回调
    IFACEMETHODIMP OnDataAvailable(IXMLHTTPRequest2*, ISequentialStream*)  ;
    // 接收完成回调
    IFACEMETHODIMP OnResponseReceived(IXMLHTTPRequest2*, ISequentialStream* responseStream) ;
	// 请求错误回调
    IFACEMETHODIMP OnError(IXMLHTTPRequest2*, HRESULT hrError);
   
	HRESULT ReadData(ISequentialStream* readStream, std::vector<char>* outdata);
	
    // 检查http请求完成事件
    task_completion_event<HRESULT> const& GetCompletionEvent() const
	{
		return completionEvent; 
	}
    
	// Create a task that completes when data is available, in an exception-safe way.
    concurrency::task<void> CreateDataTask()
	{
		concurrency::critical_section::scoped_lock lock(dataEventLock);
		return create_task(dataEvent, cancellationToken);
	}
	
	//获得错误码
    HRESULT GetError() const 
	{
        return dataHResult;
    }
	
private:
    cancellation_token cancellationToken;				// 取消下载操作的标记    
    cancellation_token_registration registrationToken;	// 用以注销取消token的回调
    ComPtr<IXMLHTTPRequest2> request;					// 真正处理http请求的IXMLHTTPRequest2对象    
    task_completion_event<HRESULT> completionEvent; // 下载完成后设置任务完成事件
    WRTHttpResponse* _httpResponse;
	// 当数据有效或者请求错误时设置的任务事件
    concurrency::task_completion_event<void> dataEvent;
    concurrency::critical_section dataEventLock;

    // We cannot store the error obtained from IXHR2 in the dataEvent since any value there is first-writer-wins,
    // whereas we want a subsequent error to override an initial success.
    HRESULT dataHResult;	
};




#endif //__HTTP_REQUEST_CALLBACK__
