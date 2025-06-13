#ifndef NETWORK_H
#define NETWORK_H

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
 * This function should be called by the event handler function for each
 * API_EVENT_ID_NETWORK_CELL_INFO event. It formats the cell information into a string
 * and stores it in the global `g_cellInfo` variable.
 */
void NetworkCellInfoCallback(Network_Location_t* loc, int number);

/**
 * @brief Callback function to handle network signal quality updates.
 * 
 * This function converts the CSQ value to a percentage and logs it.
 * it is called whenever the signal quality changes.
 * It updates the global signal strength variable `g_RSSI` based on the received CSQ value.
 *
 * @param CSQ The received signal quality value (CSQ).
 * @note The CSQ value is typically in the range of 0-31, where 0 indicates no signal and 31 indicates the best signal quality.
 * 
 */
void NetworkSigQualityCallback(int CSQ);

/**
 * @brief Gets the current cell information as a formatted string.
 *
 * This function retrieves the cell information last received and 
 * formats it into a string representation.
 * 
 * @return A pointer to a string containing the cell information in the format "MCC,MNC,LAC,CellID,RxLev". 
 */
const char* Network_GetCellInfoString(void);

/**
 * @brief Prints information on all visible BaseStations to the UART.
 * 
 * This function prints  to the UART information on all basestations seen by the device.
 * It includes details such as MCC, MNC, LAC, Cell ID, and RxLev.
 */
void NetworkPrintCellInfo(void);

#endif
