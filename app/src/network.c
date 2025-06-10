#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>

#include <api_os.h>
#include <api_network.h>

#include "system.h"
#include "gps_tracker.h"
#include "config_store.h"
#include "network.h"
#include "debug.h"

uint8_t g_RSSI = 0;
char    g_cellInfo[128] = "\0";
static Network_Status_t g_NetworkStatus = 0;
static Network_PDP_Context_t NetContextArr[2]; // Two-element array: [0]=real APN, [1]=dummy APN
static bool apn_workaround_pending = false;  

static void NetworkMonitorTimer(HANDLE);

void NetworkMonitor(void* param)
{
    if (param == NULL) return;

    if (IS_GSM_ACTIVE() && (g_trackerloop_tick > 0))
    {
        uint32_t now = time(NULL);
        if (now - g_trackerloop_tick > 150000) {
            LOGE("gps_tracker watchdog: loop stuck, deactivating network!");
            Network_StartDeactive(1);
        }
    } else if (g_trackerloop_tick > 0) {
        NetworkAttachActivate();
    }

    if (IS_GSM_REGISTERED()) {
        if(!Network_GetCellInfoRequst()) {
            g_cellInfo[0] = '\0';
            LOGE("network get cell info fail");
        }
    } else {
        g_cellInfo[0] = '\0';
    }
    HANDLE taskHandle = (HANDLE)param;
    NetworkMonitorTimer(taskHandle);
}

static void NetworkMonitorTimer(HANDLE taskHandle)
{  
    OS_StartCallbackTimer(taskHandle, 15000, NetworkMonitor, (void*)taskHandle);
}

void NetworkCellInfoCallback(Network_Location_t* loc, int number)
{
    g_cellInfo[0] = '\0';
    if (number <= 0) return;
    snprintf(g_cellInfo,  sizeof(g_cellInfo), "%u%u%u,%u%u%u,%u,%u,%d",
             loc->sMcc[0], loc->sMcc[1], loc->sMcc[2], loc->sMnc[0], loc->sMnc[1], loc->sMnc[2], loc->sLac, loc->sCellID, loc->iRxLev);
}

// the function is called whenever the network state changes
void NetworkUpdateStatus(Network_Status_t status)
{
    switch (status) {
        case NETWORK_STATUS_OFFLINE:        
            LOGE("network offline");
            break;
        case NETWORK_STATUS_REGISTERING:
            LOGI("state: registering to the network");
            GSM_REGISTERED_OFF();
            break;
        case NETWORK_STATUS_REGISTERED:     
            LOGW("state: network registered successfully");
            GSM_REGISTERED_ON();
            NetworkAttachActivate();
            break;
        case NETWORK_STATUS_DETACHED:
            LOGW("state: network detached");
            GSM_ACTIVE_OFF();
            break;
        case NETWORK_STATUS_ATTACHING:
            LOGI("state: attaching to the network");
            GSM_ACTIVE_OFF();
            break;
        case NETWORK_STATUS_ATTACHED:       
            LOGW("state: network attached successfully");
            GSM_ACTIVE_OFF(); 
            NetworkAttachActivate();    
            break;
        case NETWORK_STATUS_DEACTIVED:      
            LOGE("state: network deactived");
            GSM_ACTIVE_OFF(); 
            NetworkAttachActivate();
            break;
        case NETWORK_STATUS_ACTIVATING:
            LOGW("state: activating the network");
            GSM_ACTIVE_OFF(); 
            break;        
        case NETWORK_STATUS_ACTIVATED:
            LOGW("state: network activated successfully");
            GSM_ACTIVE_ON();
            break;
        case NETWORK_STATUS_ATTACH_FAILED: 
            LOGE("state: network attach failed");
            GSM_ACTIVE_OFF(); 
            break;
        case NETWORK_STATUS_ACTIVATE_FAILED:
            LOGE("state: network activation failed");
            break;

        default:
            break;
    }
    g_NetworkStatus = status;
}

Network_Status_t NetworkGetStatus()
{
    return g_NetworkStatus;
}

void NetworkInit(HANDLE taskHandle)
{
    Network_SetStatusChangedCallback(NetworkUpdateStatus);

    // APN Re-activation Workaround
    //
    // the firmware has a defect where re-activating the same APN after deactivation fails.
    // To work around this, we:
    //   1. Attempt to activate a dummy/incorrect APN first (NetContextArr[1]).
    //   2. Wait for a network deactivation event (which occurs because the dummy APN fails).
    //   3. Then activate the real APN (NetContextArr[0]).
    //
    // This workaround is managed by the apn_workaround_pending flag and the AttachActivate() function.
    // The dummy APN context is set once at system initialization. The real APN context is set as needed.
    // All logic for the workaround is centralized in AttachActivate; 
    apn_workaround_pending = false;

    // Dummy APN (index 1) context is set once at system init
    memset(&NetContextArr[1], 0, sizeof(NetContextArr[1]));
    strncpy(NetContextArr[1].apn, "dummy_apn", sizeof(NetContextArr[1].apn)-1);

    NetworkMonitorTimer(taskHandle);

    return;
}


// Helper to initialize the real APN context
static void SetApnContext() {
    // Real APN (index 0)
    memset(&NetContextArr[0], 0, sizeof(NetContextArr[0]));
    strncpy(NetContextArr[0].apn, g_ConfigStore.apn, sizeof(NetContextArr[0].apn)-1);
    strncpy(NetContextArr[0].userName, g_ConfigStore.apn_user, sizeof(NetContextArr[0].userName)-1);
    strncpy(NetContextArr[0].userPasswd, g_ConfigStore.apn_pass, sizeof(NetContextArr[0].userPasswd)-1);
}

bool NetworkAttachActivate()
{
    uint8_t status;
    if (!IS_GSM_REGISTERED()) {
        LOGE("Activation skipped. Network not registered");
        return false;
    }

    bool ret = Network_GetAttachStatus(&status);
    if(!ret)
    {
        LOGE("get attach status failed");
        return false;
    }
    if(!status)
    {
        NetworkUpdateStatus(NETWORK_STATUS_ATTACHING);
        ret = Network_StartAttach();
        if(!ret)
        {
            LOGE("network attach failed");
            return false;
        }
        return true;
    }

    if (NetworkGetStatus() == NETWORK_STATUS_ACTIVATING)
    {
        LOGW("Activation skipped as it is already in progress");
        return false;
    }
    ret = Network_GetActiveStatus(&status);
    if(!ret)
    {
        LOGE("Get activate status failed");
        return false;
    }
    if (!status)
    {
        // Implements the APN re-activation workaround:
        // - If workaround is pending, activate the real APN (NetContextArr[0]) and clear the flag.
        // - Otherwise, activate the dummy APN (NetContextArr[1]) and set the flag.
        // This ensures the modem can recover from the re-activation defect.
        if (!apn_workaround_pending) {
            LOGI("Using valid APN to activate the network");
            SetApnContext();
            NetworkUpdateStatus(NETWORK_STATUS_ACTIVATING);
            ret = Network_StartActive(NetContextArr[0]); // real APN
            apn_workaround_pending = true;
        } else {
            LOGI("Using dummy APN to workaround re-activation defect");
            NetworkUpdateStatus(NETWORK_STATUS_ACTIVATING);
            ret = Network_StartActive(NetContextArr[1]); // dummy
            apn_workaround_pending = false;
        }
        if (!ret) {
            LOGE("Failed to activate APN");
            return false;
        }
    }
    return true;
}
