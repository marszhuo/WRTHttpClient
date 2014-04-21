/*
ʹ�ô�����û�ע�����
1.������ɻ���ȡ������ʱ�������delegate�ÿգ���ֹ��ͬ��http���󷵻����ݺ��͸������delegate
2.Ϊ������󷵻ص���Ϣ����Ҫ�̳�WRTRequestClientDelegate��
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
	void cancel();	//ȡ������	

private:
	WRTHttpClient();
    virtual ~WRTHttpClient();
	bool lazyInitThreadSemphore();
	void dispatchResponseCallbacks(float delta);	
private:	
	concurrency::cancellation_token_source cancellationTokenSource;
};


#endif 