/*
使用此类的用户注意事项：
1.请求完成或者取消请求时将请求的delegate置空，防止不同的http请求返回数据后发送给错误的delegate
2.为获得请求返回的消息，需要继承WRTRequestClientDelegate类
*/

#ifndef __WRT_HTTP_CLIENT_H__
#define __WRT_HTTP_CLIENT_H__

#include "cocos2d.h"
#include "ExtensionMacros.h"
#include <collection.h>

#include "WRTHttpRequest.h"
#include "WRTHttpResponse.h"

USING_NS_CC;

class WRTHttpClient : public cocos2d::CCObject
{
public:
	/** Return the shared instance **/
    static WRTHttpClient *getInstance();
    
    /** Relase the shared instance **/
    static void destroyInstance();

	/**
     * Add a get request to task queue
     * @param request a url string & request body
     * @return NULL
     */
	void send(WRTHttpRequest * request);
	void send(const std::string pURL,
				const std::string pContent,
				WRTHttpRequest::HttpRequestType pType,
				CCObject* target,
				SEL_WRTHttpResponse selector);
	
	void response(WRTHttpResponse * response);
	void cancel();	//取消请求	

private:
	WRTHttpClient();
    virtual ~WRTHttpClient();
	bool lazyInitThreadSemphore();
	void dispatchResponseCallbacks(float delta);	
private:	
	concurrency::cancellation_token_source cancellationTokenSource;
};


#endif 