/*
 * BLEServer.cpp
 *
 *  Created on: Apr 16, 2017
 *      Author: kolban
 */

#include "BLEDevice.h"
#include "BLEServer.h"
//#include "BLEService.h"
#include <string.h>
#include <string>
#include <unordered_set>

/**
 * @brief Construct a %BLE Server
 *
 * This class is not designed to be individually instantiated.  Instead one should create a server by asking
 * the BLEDevice class.
 */
BLEServer::BLEServer() {
	m_appId            = 0xff;
	m_gatts_if         = 0xff;
	m_connectedCount   = 0;
	m_connId           = 0xff;
	m_pServerCallbacks = nullptr;
} // BLEServer


void BLEServer::createApp(uint16_t appId) {
	m_appId = appId;
	registerApp(appId);
} // createApp


/**
 * @brief Create a %BLE Service.
 *
 * With a %BLE server, we can host one or more services.  Invoking this function causes the creation of a definition
 * of a new service.  Every service must have a unique UUID.
 * @param [in] uuid The UUID of the new service.
 * @return A reference to the new service object.
 */
BLEService* BLEServer::createService(const char* uuid) {
	return createService(BLEUUID(uuid));
}


/**
 * @brief Create a %BLE Service.
 *
 * With a %BLE server, we can host one or more services.  Invoking this function causes the creation of a definition
 * of a new service.  Every service must have a unique UUID.
 * @param [in] uuid The UUID of the new service.
 * @param [in] numHandles The maximum number of handles associated with this service.
 * @param [in] inst_id With multiple services with the same UUID we need to provide inst_id value different for each service.
 * @return A reference to the new service object.
 */
BLEService* BLEServer::createService(BLEUUID uuid, uint32_t numHandles, uint8_t inst_id) {
	m_semaphoreCreateEvt.take("createService");

	BLEService* pService = new BLEService(uuid, numHandles);
	pService->m_instId = inst_id;
	m_serviceMap.setByUUID(uuid, pService); // Save a reference to this service being on this server.
	pService->executeCreate(this);          // Perform the API calls to actually create the service.

	m_semaphoreCreateEvt.wait("createService");

	return pService;
} // createService




/**
 * @brief Register the app.
 *
 * @return N/A
 */
void BLEServer::registerApp(uint16_t m_appId) {
	m_semaphoreRegisterAppEvt.take("registerApp"); // Take the mutex, will be released by ESP_GATTS_REG_EVT event.
//	::esp_ble_gatts_app_register(m_appId);
	m_semaphoreRegisterAppEvt.wait("registerApp");
} // registerApp


/**
 * @brief Set the server callbacks.
 *
 * As a %BLE server operates, it will generate server level events such as a new client connecting or a previous client
 * disconnecting.  This function can be called to register a callback handler that will be invoked when these
 * events are detected.
 *
 * @param [in] pCallbacks The callbacks to be invoked.
 */
void BLEServer::setCallbacks(BLEServerCallbacks* pCallbacks) {
	m_pServerCallbacks = pCallbacks;
} // setCallbacks



void BLEServerCallbacks::onConnect(BLEServer* pServer) {
	Serial.printf("BLEServerCallbacks", ">> onConnect(): Default");
//	Serial.printf("BLEServerCallbacks", "Device: %s", BLEDevice::toString().c_str());
	Serial.printf("BLEServerCallbacks", "<< onConnect()");
} // onConnect


void BLEServerCallbacks::onDisconnect(BLEServer* pServer) {
	Serial.printf("BLEServerCallbacks", ">> onDisconnect(): Default");
//	Serial.printf("BLEServerCallbacks", "Device: %s", BLEDevice::toString().c_str());
	Serial.printf("BLEServerCallbacks", "<< onDisconnect()");
} // onDisconnect


void BLEServer::addPeerDevice(void* peer, bool _client, uint16_t conn_id) {
	conn_status_t status = {
		.peer_device = peer,
		.connected = true,
		.mtu = 23
	};

	m_connectedServersMap.insert(std::pair<uint16_t, conn_status_t>(conn_id, status));	
}


/**
 * @brief Handle a GATT Server Event.
 *
 * @param [in] event
 * @param [in] gatts_if
 * @param [in] param
 *
 */
void BLEServer::handleGATTServerEvent(T_SERVER_ID service_id, void *p_datas) {

#if 0
	switch(event) {
	
		case ESP_GATTS_ADD_CHAR_EVT: {
			break;
		} // ESP_GATTS_ADD_CHAR_EVT

		case ESP_GATTS_MTU_EVT:
			updatePeerMTU(param->mtu.conn_id, param->mtu.mtu);
			break;

		case ESP_GATTS_CONNECT_EVT: {
			m_connId = param->connect.conn_id;
			addPeerDevice((void*)this, false, m_connId);
			if (m_pServerCallbacks != nullptr) {
				m_pServerCallbacks->onConnect(this);
//				m_pServerCallbacks->onConnect(this, param);			
			}
			m_connectedCount++;   // Increment the number of connected devices count.	
			break;
		} // ESP_GATTS_CONNECT_EVT

		case ESP_GATTS_CREATE_EVT: {
			BLEService* pService = m_serviceMap.getByUUID(param->create.service_id.id.uuid, param->create.service_id.id.inst_id);  // <--- very big bug for multi services with the same uuid
			m_serviceMap.setByHandle(param->create.service_handle, pService);
			m_semaphoreCreateEvt.give();
			break;
		} // ESP_GATTS_CREATE_EVT


		case ESP_GATTS_DISCONNECT_EVT: {
			if (m_pServerCallbacks != nullptr) {         // If we have callbacks, call now.
				m_pServerCallbacks->onDisconnect(this);
			}
            if(m_connId == ESP_GATT_IF_NONE) {
                return;
            }

            // only decrement if connection is found in map and removed
            // sometimes this event triggers w/o a valid connection
			if(removePeerDevice(param->disconnect.conn_id, false)) {
                m_connectedCount--;                          // Decrement the number of connected devices count.
            }
            break;
		} // ESP_GATTS_DISCONNECT_EVT


		//
		case ESP_GATTS_READ_EVT: {
			break;
		} // ESP_GATTS_READ_EVT


		case ESP_GATTS_REG_EVT: {
			m_gatts_if = gatts_if;
			m_semaphoreRegisterAppEvt.give(); // Unlock the mutex waiting for the registration of the app.
			break;
		} // ESP_GATTS_REG_EVT

		//
		case ESP_GATTS_WRITE_EVT: {
			break;
		}

		case ESP_GATTS_OPEN_EVT:
			m_semaphoreOpenEvt.give(param->open.status);
			break;

		default:
			break;
	}
	// Invoke the handler for every Service we have.
	m_serviceMap.handleGATTServerEvent(event, gatts_if, param);
#endif
} // handleGATTServerEvent
