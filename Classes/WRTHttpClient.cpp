#include "WRTHttpClient.h"
#include "WRTHttpResponse.h"

#include <stdio.h>
#include <queue>
#include <errno.h>
#include <thread>
#include <condition_variable> 
#include <mutex>
#include <locale.h>

#include <msxml6.h>
#include <collection.h>
#include <ppltasks.h>
#include <wrl.h>
#include <sstream>
#include <robuffer.h>

using namespace concurrency;
using namespace Microsoft::WRL;
using namespace Platform;
using namespace std;
using namespace Windows::Foundation;
using namespace Windows::Storage::Streams;

#define CURL_ERROR_SIZE			256

static WRTHttpClient *s_pHttpClient = NULL;					//单例对象

static CCArray* s_requestQueue = NULL;						//请求队列，保存所有请求对象
static CCArray* s_responseQueue = NULL;						//响应队列，由于一次只能允许一个请求，因此响应队列中暂最多只有一个响应对象

static char s_errorBuffer[CURL_ERROR_SIZE];					//错误消息缓存队列

static std::mutex		s_requestQueueMutex;
static std::mutex		s_responseQueueMutex;
static std::mutex		s_SleepMutex;
static std::condition_variable			s_SleepCondition;

static void networkThread();
static std::thread	*s_networkThread =NULL;

static bool need_quit = false;
static unsigned long    s_asyncRequestCount = 0;


// Worker thread
static void networkThread()
{    
    WRTHttpRequest *request = NULL;
    
    while (true) 
    {
        if (need_quit)
        {
            break;
        }
        
        // step 1: send http request if the requestQueue isn't empty
        request = NULL;
        
		s_requestQueueMutex.lock(); //Get request task from queue
        if (0 != s_requestQueue->count())
        {
            request = dynamic_cast<WRTHttpRequest*>(s_requestQueue->objectAtIndex(0));
            s_requestQueue->removeObjectAtIndex(0);  
            // request's refcount = 1 here
        }
		s_requestQueueMutex.unlock();
        
        if (NULL == request)
        {
        	// Wait for http request tasks from main thread
			std::unique_lock <std::mutex> lck(s_SleepMutex);
			s_SleepCondition.wait(lck);
            continue;
        }
     
        // Create a HttpResponse object, the default setting is http access failed
        WRTHttpResponse *response = new WRTHttpResponse(request);

        // request's refcount = 2 here, it's retained by HttpRespose constructor
        request->release();	
		request->send(response); 
    }
    
    // cleanup: if worker thread received quit signal, clean up un-completed request queue
	s_requestQueueMutex.lock();
    s_requestQueue->removeAllObjects();
   	s_requestQueueMutex.unlock();

    s_asyncRequestCount -= s_requestQueue->count();
    
    if (s_requestQueue != NULL) {    				
        s_requestQueue->release();
        s_requestQueue = NULL;
        s_responseQueue->release();
        s_responseQueue = NULL;
    }
}


WRTHttpClient* WRTHttpClient::getInstance()
{
    if (s_pHttpClient == NULL) {
        s_pHttpClient = new WRTHttpClient();	
    }

    return s_pHttpClient;
}

void WRTHttpClient::destroyInstance()
{
    CCAssert(s_pHttpClient, "unavailable http client object");
    CCDirector::sharedDirector()->getScheduler()->unscheduleSelector(schedule_selector(WRTHttpClient::dispatchResponseCallbacks), s_pHttpClient);
	if(s_pHttpClient){
		s_pHttpClient->release();
		s_pHttpClient=NULL;
	}
}

WRTHttpClient::WRTHttpClient()
{	
	CCDirector::sharedDirector()->getScheduler()->scheduleSelector(
                    schedule_selector(WRTHttpClient::dispatchResponseCallbacks), this, 0, false);
    CCDirector::sharedDirector()->getScheduler()->pauseTarget(this);
}

WRTHttpClient::~WRTHttpClient()
{
	if(&cancellationTokenSource){
		cancellationTokenSource.cancel();
	}

	s_requestQueueMutex.lock();
	if(s_requestQueue && s_requestQueue->count()>0){
		s_requestQueue->removeAllObjects();
		s_requestQueue->release();
		s_requestQueue=NULL;
	}
	s_requestQueueMutex.unlock();	

	s_responseQueueMutex.lock();
	if(s_responseQueue && s_responseQueue->count()>0){
		s_responseQueue->removeAllObjects();
		s_responseQueue->release();
		s_responseQueue=NULL;
	}
	s_responseQueueMutex.unlock();
	
	if(s_networkThread)
	{
		delete s_networkThread;
		s_networkThread = NULL;
	}

	need_quit = true;
}

bool WRTHttpClient::lazyInitThreadSemphore(void)
{
	if (s_requestQueue != NULL) {
        return true;
    } else {
        
        s_requestQueue = new CCArray();
        s_requestQueue->init();
        
        s_responseQueue = new CCArray();
        s_responseQueue->init();        		

		s_networkThread = new std::thread(networkThread);
		s_networkThread->detach();		

        need_quit = false;
    }    
    return true;
}

void WRTHttpClient::send(WRTHttpRequest * request)
{
	if (false == lazyInitThreadSemphore()) 
    {
        return;
    }

	if (!request)
    {
        return;
    }
        
    ++s_asyncRequestCount;
    
    request->retain();
        
	s_requestQueueMutex.lock();
    s_requestQueue->addObject(request);
    s_requestQueueMutex.unlock();
    
    // Notify thread start to work
	std::unique_lock <std::mutex> lck(s_SleepMutex);
	s_SleepCondition.notify_one();    
}

//将请求的url和请求发送的内容添加到请求队列中
// pURL：请求的url
// pContent：请求的包体内容
void WRTHttpClient::send(const std::string pURL,const std::string pContent,
						 WRTHttpRequest::HttpRequestType pType,
						 CCObject* target,
						 SEL_WRTHttpResponse selector
						 )
{
	WRTHttpRequest *request = new WRTHttpRequest();

	request->setUrl(pURL.c_str());
	request->setRequestData(pContent.c_str(),pContent.length());
	request->setRequestType(pType);
	request->setResponseCallback(target,selector);
	send(request);	
}

void WRTHttpClient::response(WRTHttpResponse * response)
{
	if(response)
	{
	      // add response packet into queue
		s_responseQueueMutex.lock();
        s_responseQueue->addObject(response);
		s_responseQueueMutex.unlock();
        
        // resume dispatcher selector
        CCDirector::sharedDirector()->getScheduler()->resumeTarget(WRTHttpClient::getInstance());
	}
}

// Poll and notify main thread if responses exists in queue
void WRTHttpClient::dispatchResponseCallbacks(float delta)
{
    // CCLog("CCHttpClient::dispatchResponseCallbacks is running");
    
    WRTHttpResponse* response = NULL;
    
	s_responseQueueMutex.lock();
    if (s_responseQueue->count())
    {
        response = dynamic_cast<WRTHttpResponse*>(s_responseQueue->objectAtIndex(0));
        s_responseQueue->removeObjectAtIndex(0);
    }
	s_responseQueueMutex.unlock();
    
    if (response)
    {
        --s_asyncRequestCount;
        
        WRTHttpRequest *request = response->getHttpRequest();
        CCObject *pTarget = request->getTarget();
        SEL_WRTHttpResponse pSelector = request->getSelector();

        if (pTarget && pSelector) 
        {
            (pTarget->*pSelector)(this, response);
        }
        
        response->release();
    }
    
    if (0 == s_asyncRequestCount) 
    {
        CCDirector::sharedDirector()->getScheduler()->pauseTarget(this);
    }
    
}
void WRTHttpClient::cancel(){
	if(&cancellationTokenSource){
		cancellationTokenSource.cancel();
	}

	s_requestQueueMutex.lock();
	if(s_requestQueue && s_requestQueue->count()>0){
		s_requestQueue->removeAllObjects();
	}
	s_requestQueueMutex.unlock();

	s_responseQueueMutex.lock();
	if(s_responseQueue && s_responseQueue->count()>0){
		s_responseQueue->removeAllObjects();
	}
	s_responseQueueMutex.unlock();
}

//#endif


