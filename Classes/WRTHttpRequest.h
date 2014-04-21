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
#ifndef __WRT_HTTP_REQUEST_H__
#define __WRT_HTTP_REQUEST_H__

#include "cocos2d.h"
#include <msxml6.h>
#include <collection.h>
#include <ppltasks.h>
#include <wrl.h>
#include <sstream>

USING_NS_CC;

using namespace concurrency;
using namespace Microsoft::WRL;
using namespace Platform;
using namespace std;
using namespace Windows::Foundation;
using namespace Windows::Storage::Streams;

class WRTHttpClient;
class WRTHttpResponse;
typedef void (CCObject::*SEL_WRTHttpResponse)(WRTHttpClient* client, WRTHttpResponse* response);
#define httpresponse_selector(_SELECTOR) (SEL_WRTHttpResponse)(&_SELECTOR)

// 异步HTTP请求类
class WRTHttpRequest : public CCObject
{
public:
	/** Use this enum type as param in setReqeustType(param) */
    typedef enum
    {
        kHttpGet,
        kHttpPost,
        kHttpPut,
        kHttpDelete,
        kHttpUnkown,
    } HttpRequestType;
public:
    WRTHttpRequest();   
	virtual ~WRTHttpRequest();

	void send(WRTHttpResponse* response);

	// 创建一个HTTP GET请求，一旦接收到响应数据就返回task结构，task生成响应数据文本
    concurrency::task<HRESULT> GetAsync(
        Windows::Foundation::Uri^ uri, 
        cancellation_token cancellationToken = cancellation_token::none());

	// 创建一个HTTP POST请求，参数采用string
    concurrency::task<HRESULT> PostAsync(
        Windows::Foundation::Uri^ uri,
        PCWSTR contentType,
        IStream* postStream,
        uint64 postStreamSizeToSend,
        cancellation_token cancellationToken = cancellation_token::none());

	// 创建一个HTTP POST请求，参数采用流
    concurrency::task<HRESULT> PostAsync(
        Windows::Foundation::Uri^ uri,
        const std::wstring& str,
        cancellation_token cancellationToken = cancellation_token::none());

	// 根据指定的请求方法创建一个下载请求
    concurrency::task<HRESULT> DownloadAsync(
        PCWSTR httpMethod,
        PCWSTR uri, 
        cancellation_token cancellationToken,
        PCWSTR contentType,
        IStream* postStream,
        uint64 postStreamBytesToSend);

    static void CreateMemoryStream(IStream **stream);
	static bool convertURL(Platform::String^,Windows::Foundation::Uri^* uri);
    
    /** Override autorelease method to avoid developers to call it */
    CCObject* autorelease(void)
    {
        CCAssert(false, "HttpResponse is used between network thread and ui thread \
                 therefore, autorelease is forbidden here");
        return NULL;
    }
            
    // setter/getters for properties
     
    /** Required field for HttpRequest object before being sent.
        kHttpGet & kHttpPost is currently supported
     */
    inline void setRequestType(HttpRequestType type)
    {
        _requestType = type;
    };
    /** Get back the kHttpGet/Post/... enum value */
    inline HttpRequestType getRequestType()
    {
        return _requestType;
    };
    
    /** Required field for HttpRequest object before being sent.
     */
    inline void setUrl(const char* url)
    {
        _url = url;
    };
    /** Get back the setted url */
    inline const char* getUrl()
    {
        return _url.c_str();
    };
    
    /** Option field. You can set your post data here
     */
    inline void setRequestData(const char* buffer, unsigned int len)
    {
        _requestData.assign(buffer, buffer + len);
    };
    /** Get the request data pointer back */
    inline char* getRequestData()
    {
        return &(_requestData.front());
    }
    /** Get the size of request data back */
    inline int getRequestDataSize()
    {
        return _requestData.size();
    }
    
    /** Option field. You can set a string tag to identify your request, this tag can be found in HttpResponse->getHttpRequest->getTag()
     */
    inline void setTag(const char* tag)
    {
        _tag = tag;
    };
    /** Get the string tag back to identify the request. 
        The best practice is to use it in your MyClass::onMyHttpRequestCompleted(sender, HttpResponse*) callback
     */
    inline const char* getTag()
    {
        return _tag.c_str();
    };
    
    /** Option field. You can attach a customed data in each request, and get it back in response callback.
        But you need to new/delete the data pointer manully
     */
    inline void setUserData(void* pUserData)
    {
        _pUserData = pUserData;
    };
    /** Get the pre-setted custom data pointer back.
        Don't forget to delete it. HttpClient/HttpResponse/HttpRequest will do nothing with this pointer
     */
    inline void* getUserData()
    {
        return _pUserData;
    };
    
    /** Required field. You should set the callback selector function at ack the http request completed
     */
    CC_DEPRECATED_ATTRIBUTE inline void setResponseCallback(CCObject* pTarget, SEL_CallFuncND pSelector)
    {
        setResponseCallback(pTarget, (SEL_WRTHttpResponse) pSelector);
    }

    inline void setResponseCallback(CCObject* pTarget, SEL_WRTHttpResponse pSelector)
    {
        _pTarget = pTarget;
        _pSelector = pSelector;
        
        if (_pTarget)
        {
            _pTarget->retain();
        }
    }    
    /** Get the target of callback selector funtion, mainly used by CCHttpClient */
    inline CCObject* getTarget()
    {
        return _pTarget;
    }

    /* This sub class is just for migration SEL_CallFuncND to SEL_HttpResponse, 
       someday this way will be removed */
    class _prxy
    {
    public:
        _prxy( SEL_WRTHttpResponse cb ) :_cb(cb) {}
        ~_prxy(){};
        operator SEL_WRTHttpResponse() const { return _cb; }
        CC_DEPRECATED_ATTRIBUTE operator SEL_CallFuncND()   const { return (SEL_CallFuncND) _cb; }
    protected:
        SEL_WRTHttpResponse _cb;
    };
    
    /** Get the selector function pointer, mainly used by CCHttpClient */
    inline _prxy getSelector()
    {
        return _prxy(_pSelector);
    }
    
    /** Set any custom headers **/
    inline void setHeaders(std::vector<std::string> pHeaders)
   	{
   		_headers=pHeaders;
   	}
   
    /** Get custom headers **/
   	inline std::vector<std::string> getHeaders()
   	{
   		return _headers;
   	}

	//字符串转换
	std::wstring &stringToWstring(const std::string &src,std::wstring &dest);
	std::string &wstringToString(const std::wstring &src,std::string &dest);
private:
	 // properties
    HttpRequestType             _requestType;    /// kHttpRequestGet, kHttpRequestPost or other enums
    std::string                 _url;            /// target url that this request is sent to
    std::vector<char>           _requestData;    /// used for POST
    std::string                 _tag;            /// user defined tag, to identify different requests in response callback
    CCObject*					_pTarget;        /// callback target of pSelector function
    SEL_WRTHttpResponse         _pSelector;      /// callback function, e.g. MyLayer::onHttpResponse(CCHttpClient *sender, CCHttpResponse * response)
    void*                       _pUserData;      /// You can add your customed data here 
    std::vector<std::string>    _headers;		      /// custom http headers
private:
    int statusCode;
    std::wstring reasonPhrase;
	// 响应数据是否接收完毕的标记ReadAsync
    bool responseComplete;
	WRTHttpResponse* _response;
};


#endif