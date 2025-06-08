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

char g_cellInfo[128]  = "\0";

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
    networkCellInfoTimer(taskHandle);
}

void networkCellInfoTimer(HANDLE taskHandle)
{  
    OS_StartCallbackTimer(taskHandle, 15000, NetworkCellInfoGet, (void*)taskHandle);
}

void networkCellInfoCallback(Network_Location_t* loc, int number)
{
    g_cellInfo[0] = '\0';
    if (number <= 0) return;
    snprintf(g_cellInfo,  sizeof(g_cellInfo), "%u%u%u,%u%u%u,%u,%u,%d",
             loc->sMcc[0], loc->sMcc[1], loc->sMcc[2], loc->sMnc[0], loc->sMnc[1], loc->sMnc[2], loc->sLac, loc->sCellID, loc->iRxLev);
}

// --- APN Re-activation Workaround ---
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


// Two-element array: [0]=real APN, [1]=dummy APN
static Network_PDP_Context_t NetContextArr[2];
static bool apn_workaround_pending = false;  

void apnWorkaround_init(void) 
{
    // set Workaround state for APN re-activation
    apn_workaround_pending = false;
    // Dummy APN (index 1) context is set once at system init
    memset(&NetContextArr[1], 0, sizeof(NetContextArr[1]));
    strncpy(NetContextArr[1].apn, "dummy_apn", sizeof(NetContextArr[1].apn)-1);
}

// Helper to initialize the real APN context
static void SetApnContext() {
    // Real APN (index 0)
    memset(&NetContextArr[0], 0, sizeof(NetContextArr[0]));
    strncpy(NetContextArr[0].apn, g_ConfigStore.apn, sizeof(NetContextArr[0].apn)-1);
    strncpy(NetContextArr[0].userName, g_ConfigStore.apn_user, sizeof(NetContextArr[0].userName)-1);
    strncpy(NetContextArr[0].userPasswd, g_ConfigStore.apn_pass, sizeof(NetContextArr[0].userPasswd)-1);
}

bool gsm_AttachActivate()
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
                apn_workaround_pending = false;
            } else {
                LOGW("Trying to activate dummy APN to workaround re-activation defect");
                apn_workaround_pending = true;
                ret = Network_StartActive(NetContextArr[1]); // dummy
            }
            if (!ret)
                LOGE("Failed to activate APN");
        }
    }
    return true;
}
