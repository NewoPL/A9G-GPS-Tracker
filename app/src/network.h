#ifndef NETWORK_H
#define NETWORK_H

#define SSL_WRITE_TIMEOUT 5000
#define SSL_READ_TIMEOUT  5000

/*
 * @brief Global variable to store cell information.
 *
 * This variable is used to hold the formatted cell information string
 * that can be accessed by other parts of the application.  
 */
extern char g_cellInfo[];

/**
 * @brief Setups capturing network state.
 * 
 * This function should be called during system initialization to set up the network status callback.
 * It registers a callback function that will be called whenever the network status changes.
 * It sets up the dummy APN context and initializes the workaround state.
 */
void NetworkInit(HANDLE taskHandle);

/**
 * @brief Attach and activate the GSM network.
 *
 * This function checks the current attach and activate status of the GSM network.
 * If not attached, it attempts to attach. If attached but not activated, it activates the network.
 * It handles the APN re-activation workaround by toggling between a dummy APN and the real APN.
 * @return true if the operation was successful, false otherwise.
 */
bool NetworkAttachActivate(void);

/**
 * @brief Gets the current network status.
 * 
 * This function retrieves the current network status as defined by the Network_Status_t enum.
 * @return The current network status.
 */
Network_Status_t NetworkGetStatus(void);

/**
 * @brief Callback function to handle network cell information updates.
 * 
 * This function should be called by the event handler functionfor each
 * API_EVENT_ID_NETWORK_CELL_INFO event. It formats the cell information into a string
 * and stores it in the global `g_cellInfo` variable.
 */
void NetworkCellInfoCallback(Network_Location_t* loc, int number);

#endif
