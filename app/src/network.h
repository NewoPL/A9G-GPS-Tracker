#ifndef NETWORK_H
#define NETWORK_H

#define SSL_WRITE_TIMEOUT 5000
#define SSL_READ_TIMEOUT  2000

/*
 * @brief Global variable to store cell information.
 *
 * This variable is used to hold the formatted cell information string
 * that can be accessed by other parts of the application.  
 */
extern char g_cellInfo[];

void networkCellInfoTimer(HANDLE taskHandle);

/**
 * @brief Callback to process network cell information and update the global cell info string.
 * 
 * This function formats the provided network location data into a string representation
 * and stores it in the global variable `g_cellInfo`.
 * 
 * @param loc Pointer to a Network_Location_t structure containing cell information.
 * @param number The number of cells to process (used for validation).
 */
void networkCellInfoCallback(Network_Location_t* loc, int number);

/**
 * @brief Initializes the APN re-activation workaround.
 *
 * This function sets up the dummy APN context and initializes the workaround state.
 * It should be called once during system initialization. 
 */
void apnWorkaround_init(void);

/**
 * @brief Attach and activate the GSM network.
 *
 * This function checks the current attach and activate status of the GSM network.
 * If not attached, it attempts to attach. If attached but not activated, it activates the network.
 * It handles the APN re-activation workaround by toggling between a dummy APN and the real APN.
 * @return true if the operation was successful, false otherwise.
 */
bool gsm_AttachActivate(void);


#endif
