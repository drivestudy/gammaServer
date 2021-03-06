//
// Created by IntelliJ IDEA.
// User: AppleTree
// Date: 17/3/2
// Time: 上午7:16
// To change this template use File | Settings | File Templates.
//

#include "globalhandler.h"
#include "globalsetting.h"

NS_HIVE_BEGIN

GlobalHandler::GlobalHandler(void) : RefObject(), Sync() ,m_pGroup(NULL) {

}
GlobalHandler::~GlobalHandler(void){

}

static GlobalHandler* g_pGlobalHandler = NULL;
GlobalHandler* GlobalHandler::getInstance(void){
	return g_pGlobalHandler;
}
GlobalHandler* GlobalHandler::createInstance(void){
	if(g_pGlobalHandler == NULL){
		g_pGlobalHandler = new GlobalHandler();
		SAFE_RETAIN(g_pGlobalHandler)
	}
	return g_pGlobalHandler;
}
void GlobalHandler::destroyInstance(void){
    SAFE_RELEASE(g_pGlobalHandler)
}

bool GlobalHandler::dispatchTask(uint32 handle, Task* pTask){
	Handler* pHandler;
	lock();
	pHandler = this->getDestination<Handler>(handle);
	if(NULL != pHandler){
		pHandler->retain();
	}
	unlock();
	if(NULL == pHandler){
		LOG_ERROR("Handler not found=%d", handle);
		return false;
	}
	LOG_DEBUG("GlobalHandler dispatchTask to  handle=%d", handle);
	pHandler->acceptTask(pTask);
	pHandler->release();
	return true;
}
bool GlobalHandler::dispatchToHandler(Packet* pPacket){
	// 查找Handler对象，派发给Handler去执行
	PacketHead* pHead = pPacket->getHead();
	uint32 handle = pHead->destination.handle;
	return this->dispatchToHandler(pPacket, handle);
}
bool GlobalHandler::dispatchToHandler(Packet* pPacket, uint32 handle){
	Handler* pHandler;
	lock();
	pHandler = this->getDestination<Handler>(handle);
	if(NULL != pHandler){
		pHandler->retain();
	}
	unlock();
	if(NULL == pHandler){
		LOG_ERROR("Handler not found=%d", handle);
		return false;
	}
	bool result = pHandler->receivePacket(pPacket);
	pHandler->release();
	return result;
}
int64 GlobalHandler::activeTimer(uint32 handle, uint32 callbackID){
	Handler* pHandler;
	lock();
	pHandler = this->getDestination<Handler>(handle);
	if(NULL != pHandler){
		pHandler->retain();
	}
	unlock();
	if(NULL == pHandler){
		LOG_ERROR("Handler not found=%d", handle);
		return false;
	}
	int64 timeCount = pHandler->timerActiveCallback(callbackID);
	pHandler->release();
	return timeCount;
}
// 创建全局的HandlerPool
bool GlobalHandler::createPool(uint32 poolType,
	HandlerDestinationGroup::CreateFunction create,
	HandlerDestinationGroup::DestroyFunction destroy)
{
	bool result;
	lock();
	result = (NULL != m_pGroup->createPool(poolType, create, destroy));
	unlock();
	return result;
}
// 创建一个目标Handler
uint32 GlobalHandler::createDestination(uint32 poolType, uint32 index, uint32 moduleType, uint32 moduleIndex, const std::string& param){
	Handler* pHandler;
	uint32 handle = 0;
	lock();
	pHandler = (Handler*)m_pGroup->createDestination(poolType, index);
	if(NULL != pHandler){
        pHandler->setModuleType(moduleType);
        pHandler->setModuleIndex(moduleIndex);
		handle = pHandler->getHandle();
		pHandler->retain();
	}
	unlock();
	if(NULL != pHandler){
		pHandler->onInitialize(param);
		pHandler->release();
	}
	return handle;
}
uint32 GlobalHandler::getNextDestinationIndex(uint32 poolType, uint32 number){
	uint32 index = 0;
	lock();
	index = m_pGroup->getNextDestinationIndex(poolType, number);
	unlock();
	return index;
}
void GlobalHandler::checkOnDestroy(uint32 handle){
	Handler* pHandler;
	lock();
	pHandler = getDestination<Handler>(handle);
	if(NULL != pHandler){
		pHandler->retain();
	}
	unlock();
	if(NULL != pHandler){
		pHandler->onDestroy();
		pHandler->release();
	}
}
void GlobalHandler::checkOnDestroy(uint32 poolType, uint32 index){
	Handler* pHandler;
	lock();
	pHandler = getDestinationByIndex<Handler>(poolType, index);
	if(NULL != pHandler){
		pHandler->retain();
	}
	unlock();
	if(NULL != pHandler){
		pHandler->onDestroy();
		pHandler->release();
	}
}
bool GlobalHandler::idleDestination(uint32 handle){
	bool result;
	checkOnDestroy(handle);
	lock();
	result = m_pGroup->idleDestination(handle);
	unlock();
	return result;
}
bool GlobalHandler::removeDestination(uint32 handle){
	bool result;
	checkOnDestroy(handle);
	lock();
	result = m_pGroup->removeDestination(handle);
	unlock();
	return result;
}
bool GlobalHandler::removeDestinationByIndex(uint32 poolType, uint32 index){
	bool result;
	checkOnDestroy(poolType, index);
	lock();
	result = m_pGroup->removeDestinationByIndex(poolType, index);
	unlock();
	return result;
}
void GlobalHandler::initialize(void){
	if(NULL == m_pGroup){
		uint32 nodeID = GlobalSetting::getInstance()->getNodeID();
		m_pGroup = new HandlerDestinationGroup(nodeID, 0);
		m_pGroup->retain();
	}
}
void GlobalHandler::destroy(void){
	SAFE_RELEASE(m_pGroup)
}

NS_HIVE_END
