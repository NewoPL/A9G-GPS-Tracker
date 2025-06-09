#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include <api_os.h>
#include <api_network.h>

#include "system.h"
#include "gps_tracker.h"
#include "config_store.h"
#include "network.h"
#include "debug.h"

uint8_t g_RSSI = 0;
char    g_cellInfo[128] = "\0";
static Network_Status_t      g_NetworkStatus = 0;
static Network_PDP_Context_t NetContextArr[2]; // Two-element array: [0]=real APN, [1]=dummy APN
static bool apn_workaround_pending = false;  

static void Network_SetCellInfoTimer(HANDLE);

void NetworkCellInfoGet(void* param)
{
    if (param == NULL) return;

    if (IS_GSM_REGISTERED()) {
        if(!Network_GetCellInfoRequst()) {
            g_cellInfo[0] = '\0';
            LOGE("network get cell info fail");
        }
    } else {
        g_cellInfo[0] = '\0';
    }

    HANDLE taskHandle = (HANDLE)param;
    Network_SetCellInfoTimer(taskHandle);
}

void NetworkCellInfoCallback(Network_Location_t* loc, int number)
{
    g_cellInfo[0] = '\0';
    if (number <= 0) return;
    snprintf(g_cellInfo,  sizeof(g_cellInfo), "%u%u%u,%u%u%u,%u,%u,%d",
             loc->sMcc[0], loc->sMcc[1], loc->sMcc[2], loc->sMnc[0], loc->sMnc[1], loc->sMnc[2], loc->sLac, loc->sCellID, loc->iRxLev);
}

static void Network_SetCellInfoTimer(HANDLE taskHandle)
{  
    OS_StartCallbackTimer(taskHandle, 15000, NetworkCellInfoGet, (void*)taskHandle);
}

// OS Calls this function whenever the network state changes
void NetworkUpdateStatus(Network_Status_t status)
{
    const char* status_str = "UNKNOWN";
    switch (status) {
        case NETWORK_STATUS_OFFLINE:        status_str = "OFFLINE"; break;
        case NETWORK_STATUS_REGISTERING:    status_str = "REGISTERING"; break;
        case NETWORK_STATUS_REGISTERED:     status_str = "REGISTERED"; break;
        case NETWORK_STATUS_DETACHED:       status_str = "DETACHED"; break;
        case NETWORK_STATUS_ATTACHING:      status_str = "ATTACHING"; break;
        case NETWORK_STATUS_ATTACHED:       status_str = "ATTACHED"; break;
        case NETWORK_STATUS_DEACTIVED:      status_str = "DEACTIVED"; break;
        case NETWORK_STATUS_ACTIVATING:     status_str = "ACTIVATING"; break;
        case NETWORK_STATUS_ACTIVATED:      status_str = "ACTIVATED"; break;
        case NETWORK_STATUS_ATTACH_FAILED:  status_str = "ATTACH_FAILED"; break;
        case NETWORK_STATUS_ACTIVATE_FAILED:status_str = "ACTIVATE_FAILED"; break;
        default:                            status_str = "INVALID"; break;
    }
    LOGI("[Network] UpdateStatus called with status=%d (%s)", status, status_str);
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

    Network_SetCellInfoTimer(taskHandle);

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
    bool ret = Network_GetAttachStatus(&status);
    if(!ret)
    {
        LOGE("get attach status failed");
        return false;
    }
    if(!status)
    {
        ret = Network_StartAttach();
        LOGI("attaching to the network");
        if(!ret)
        {
            LOGE("network attach failed");
            return false;
        }
    }
    else
    {
        ret = Network_GetActiveStatus(&status);
        if(!ret)
        {
            LOGE("get activate status failed");
            return false;
        }
        if(!status)
        {
            LOGI("activating the network");

            // Implements the APN re-activation workaround:
            // - If workaround is pending, activate the real APN (NetContextArr[0]) and clear the flag.
            // - Otherwise, activate the dummy APN (NetContextArr[1]) and set the flag.
            // This ensures the modem can recover from the re-activation defect.
            if (!apn_workaround_pending) {
                SetApnContext();
                ret = Network_StartActive(NetContextArr[0]); // real APN
                apn_workaround_pending = true;
            } else {
                LOGW("Trying to activate dummy APN to workaround re-activation defect");
                apn_workaround_pending = false;
                ret = Network_StartActive(NetContextArr[1]); // dummy
            }
            if (!ret)
                LOGE("Failed to activate APN");
        }
    }
    return true;
}
