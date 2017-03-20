//
// Created by IntelliJ IDEA.
// User: AppleTree
// Date: 17/2/26
// Time: 下午8:54
// To change this template use File | Settings | File Templates.
//

#include "mainworker.h"

NS_HIVE_BEGIN

MainWorker::MainWorker(void) : ActiveWorker(0),
	m_pListenerPool(NULL), m_nodeID(0) {

}
MainWorker::~MainWorker(void){
	MainWorker::destroy();
}

static MainWorker* g_pMainWorker = NULL;
MainWorker* MainWorker::getInstance(void){
	return g_pMainWorker;
}
MainWorker* MainWorker::createInstance(void){
	if(g_pMainWorker == NULL){
		g_pMainWorker = new MainWorker();
		SAFE_RETAIN(g_pMainWorker)
	}
	return g_pMainWorker;
}
void MainWorker::destroyInstance(void){
    SAFE_RELEASE(g_pMainWorker)
}

bool MainWorker::closeListener(uint32 handle){
	fprintf(stderr, "--MainWorker::closeListener try handle=%d\n", handle);
	Listener* pListener = getListener(handle);
	if(NULL == pListener){
		return false;
	}
	m_pEpoll->objectRemove(pListener);
	pListener->resetData();
	m_pListenerPool->idle(handle);
	fprintf(stderr, "--MainWorker::closeListener OK handle=%d\n", handle);
	return true;
}
uint32 MainWorker::openListener(const char* ip, uint16 port, AcceptSocketFunction pFunc, bool isNeedEncrypt, bool isNeedDecrypt){
	fprintf(stderr, "--MainWorker::openListener try to open Listener ip=%s port=%d\n", ip, port);
	Listener* pListener = (Listener*)m_pListenerPool->create();
	if(NULL == pListener){
		fprintf(stderr, "--MainWorker::openListener NULL == pListener\n");
		return 0;
	}
	uint32 handle = pListener->getHandle();
	pListener->setEpoll(m_pEpoll);
	pListener->setSocket(ip, port);
	pListener->setAcceptSocketFunction(pFunc);
	if( !pListener->openSocket() ){
		closeListener(handle);
		fprintf(stderr, "--MainWorker::openListener Listener openSocket failed\n");
		return 0;
	}
	if( !m_pEpoll->objectAdd(pListener, EPOLLIN) ){
		pListener->closeSocket();
		closeListener(handle);
		fprintf(stderr, "--MainWorker::openListener Listener objectAdd to epoll failed\n");
		return 0;
	}
	pListener->setIsNeedEncrypt(isNeedEncrypt);
	pListener->setIsNeedDecrypt(isNeedDecrypt);
	fprintf(stderr, "--MainWorker::openListener handle=%d fd=%d ip=%s port=%d\n", handle, pListener->getSocketFD(), ip, port);
	return handle;
}
uint32 MainWorker::openHttpListener(const char* ip, uint16 port){
	return this->openListener(ip, port, MainWorker::onAcceptHttp, false, false);
}
uint32 MainWorker::openHttpsListener(const char* ip, uint16 port){
	return this->openListener(ip, port, MainWorker::onAcceptHttps, false, false);
}
uint32 MainWorker::openSocketListener(const char* ip, uint16 port, bool isNeedEncrypt, bool isNeedDecrypt){
	return this->openListener(ip, port, MainWorker::onAcceptSocket, isNeedEncrypt, isNeedDecrypt);
}

void MainWorker::onAcceptSocket(int fd, const char* ip, uint16 port, Listener* pListener){
	OpenAcceptTask* pTask = new OpenAcceptTask();
	pTask->retain();
	pTask->m_fd = fd;
	pTask->setSocket(ip, port);
	pTask->m_isNeedEncrypt = pListener->isNeedEncrypt();
	pTask->m_isNeedDecrypt = pListener->isNeedDecrypt();
	GlobalService::getInstance()->dispatchTaskEqualToEpollWorker(pTask);
	pTask->release();
}
void MainWorker::onAcceptHttp(int fd, const char* ip, uint16 port, Listener* pListener){
	OpenHttpTask* pTask = new OpenHttpTask();
	pTask->retain();
	pTask->m_fd = fd;
	pTask->setSocket(ip, port);
	GlobalService::getInstance()->dispatchTaskEqualToEpollWorker(pTask);
	pTask->release();
}
void MainWorker::onAcceptHttps(int fd, const char* ip, uint16 port, Listener* pListener){
	OpenHttpsTask* pTask = new OpenHttpsTask();
	pTask->retain();
	pTask->m_fd = fd;
	pTask->setSocket(ip, port);
	GlobalService::getInstance()->dispatchTaskEqualToEpollWorker(pTask);
	pTask->release();
}

void MainWorker::update(void){
	fprintf(stderr, "--MainWorker start nodeID=%d serviceID=%d \n", m_nodeID, getServiceID());
	int64 timeout;
	while(1){
		timeout = m_pTimer->getWaitTimeout();
		m_pEpoll->update(timeout);
		m_pTimer->update();
	}
	fprintf(stderr, "--MainWorker exit nodeID=%d serviceID=%d \n", m_nodeID, getServiceID());
}

void MainWorker::initialize(uint16 nodeID, uint16 epollWorkerNumber, uint16 workerNumber){
	// 初始化基类数据
	ActiveWorker::initialize();

	m_nodeID = nodeID;
	if(NULL == m_pListenerPool){
		m_pListenerPool = new DestinationPool();
		m_pListenerPool->retain();
		m_pListenerPool->registerFunction(getNodeID(), getServiceID(), 0, Listener::createObject, Listener::releaseObject);
	}
	TimerManager::createInstance()->setTimer(m_pTimer);
	GlobalHandler::createInstance()->initialize();
	GlobalService::createInstance()->initialize(epollWorkerNumber);
	HandlerQueue::createInstance()->createWorker(workerNumber);
}
void MainWorker::destroy(void){
	SAFE_RELEASE(m_pListenerPool)

	GlobalService::destroyInstance();
	HandlerQueue::destroyInstance();
}

NS_HIVE_END
