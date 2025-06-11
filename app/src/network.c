#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>

#include <api_os.h>
#include <api_network.h>
#include <api_hal_uart.h>

#include "system.h"
#include "gps_tracker.h"
#include "config_store.h"
#include "network.h"
#include "minmea.h"
#include "utils.h"
#include "debug.h"

#define MODULE_TAG "Network"

static bool apn_workaround_pending = false;  
#define MAX_CELLINFO_COUNT 8

static Network_Status_t      g_NetworkStatus = 0;
static Network_PDP_Context_t g_NetContextArr[2]; // Two-element array: [0]=real APN, [1]=dummy APN
static Network_Location_t    g_CellInfo[MAX_CELLINFO_COUNT];
static uint8_t               g_CellInfoCount = 0;
static uint8_t               g_RSSI = 0;

/*
 * @brief Global variable to store cell information.
 *
 * This variable is used to hold the formatted cell information string
 * that can be accessed by other parts of the application.  
 */
static char g_cellInfoStr[128] = "\0";

static void NetworkMonitorTimer(HANDLE);

static void NetworkMonitor(void* param)
{
    if (param == NULL) return;

    if (g_trackerloop_tick > 0) {
        if (IS_GSM_ACTIVE())
        {
            uint32_t now = time(NULL);
            if (now - g_trackerloop_tick > 20) {
                LOGE("watchdog: connection is taking too long, deactivating network!");
                Network_StartDeactive(1);
            }
        } else {
            NetworkAttachActivate();
        }
    }

    if (IS_GSM_REGISTERED()) {
        if(!Network_GetCellInfoRequst()) {
            g_cellInfoStr[0] = '\0';
            LOGE("network get cell info fail");
        }
    } else {
        g_cellInfoStr[0] = '\0';
    }
    HANDLE taskHandle = (HANDLE)param;
    NetworkMonitorTimer(taskHandle);
}

static void NetworkMonitorTimer(HANDLE taskHandle)
{  
    OS_StartCallbackTimer(taskHandle, NETWORK_MONITOR_INTERVAL_MS, NetworkMonitor, (void*)taskHandle);
}

void NetworkSigQualityCallback(int CSQ)
{
    g_RSSI = csq_to_percent(CSQ);
    LOGD("Signal Quality: %d%%", g_RSSI);
    return;
}

void NetworkCellInfoCallback(Network_Location_t* loc, int number)
{
    g_cellInfoStr[0] = '\0';
    g_CellInfoCount = 0;
    if (number <= 0) return;
    int count = (number > MAX_CELLINFO_COUNT) ? MAX_CELLINFO_COUNT : number;
    for (int i = 0; i < count; ++i) {
        g_CellInfo[i] = loc[i];
    }
    g_CellInfoCount = count;
    // Format the serving cell for UART output (first entry)
    snprintf(g_cellInfoStr, sizeof(g_cellInfoStr), "%u%u%u,%u%u%u,%u,%u,%d",
             g_CellInfo[0].sMcc[0], g_CellInfo[0].sMcc[1], g_CellInfo[0].sMcc[2],
             g_CellInfo[0].sMnc[0], g_CellInfo[0].sMnc[1], g_CellInfo[0].sMnc[2],
             g_CellInfo[0].sLac, g_CellInfo[0].sCellID, g_CellInfo[0].iRxLev);
}

void NetworkPrintCellInfo(void)
{
    if (g_CellInfoCount > 0) {
        UART_Printf("Base stations seen: %d\r\n", g_CellInfoCount);
        for (uint8_t i = 0; i < g_CellInfoCount; ++i) {
            UART_Printf("  [%d] MCC: %u%u%u, MNC: %u%u%u, LAC: %u, CellID: %u, RxLev: %d\r\n", i,
                g_CellInfo[i].sMcc[0], g_CellInfo[i].sMcc[1], g_CellInfo[i].sMcc[2],
                g_CellInfo[i].sMnc[0], g_CellInfo[i].sMnc[1], g_CellInfo[i].sMnc[2],
                g_CellInfo[i].sLac, g_CellInfo[i].sCellID, g_CellInfo[i].iRxLev);
        }
    } else {
        UART_Printf("Cell info not available\r\n");
    }
}

const char* Network_GetCellInfoString(void)
{
    return g_cellInfoStr;
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
    memset(&g_NetContextArr[1], 0, sizeof(g_NetContextArr[1]));
    strncpy(g_NetContextArr[1].apn, "dummy_apn", sizeof(g_NetContextArr[1].apn)-1);

    NetworkMonitorTimer(taskHandle);

    return;
}


// Helper to initialize the real APN context
static void SetApnContext() {
    // Real APN (index 0)
    memset(&g_NetContextArr[0], 0, sizeof(g_NetContextArr[0]));
    strncpy(g_NetContextArr[0].apn, g_ConfigStore.apn, sizeof(g_NetContextArr[0].apn)-1);
    strncpy(g_NetContextArr[0].userName, g_ConfigStore.apn_user, sizeof(g_NetContextArr[0].userName)-1);
    strncpy(g_NetContextArr[0].userPasswd, g_ConfigStore.apn_pass, sizeof(g_NetContextArr[0].userPasswd)-1);
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
            ret = Network_StartActive(g_NetContextArr[0]); // real APN
            apn_workaround_pending = true;
        } else {
            LOGI("Using dummy APN to workaround re-activation defect");
            NetworkUpdateStatus(NETWORK_STATUS_ACTIVATING);
            ret = Network_StartActive(g_NetContextArr[1]); // dummy
            apn_workaround_pending = false;
        }
        if (!ret) {
            LOGE("Failed to activate APN");
            return false;
        }
    }
    return true;
}

