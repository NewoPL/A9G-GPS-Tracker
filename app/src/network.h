#ifndef NETWORK_H
#define NETWORK_H

#define SSL_WRITE_TIMEOUT 5000
#define SSL_READ_TIMEOUT  2000

/**
 * @brief Sends an HTTP/HTTPs POST request to the specified server.
 * 
 * @param hostName The hostName name of the server (e.g., "example.com").
 * @param port The port number to connect to (e.g., 443 for HTTPS).
 * @param path The path of the resource on the server (e.g., "/api/data").
 * @param data The data to be sent in the POST request body.
 * @param dataLen The length of the data to be sent.
 * @param retBuffer A buffer to store the response data received from the server.
 * @param retBufferSize The size of the buffer allocated for the response data.
 * @return int Returns the length of the response on success, or -1 on failure.
 */
int Http_Post(SSL_Config_t *sslConfig,
              const char   *hostName,
              const char   *port,
              const char   *path,
              const char   *data,
              uint16_t      dataLen,
              char*         retBuffer,
              int           retBufferSize);

/**
 * @brief Processes the network cell information and updates the global cell info string.
 * 
 * This function formats the provided network location data into a string representation
 * and stores it in the global variable `g_cellInfo`.
 * 
 * @param loc Pointer to a Network_Location_t structure containing cell information.
 * @param number The number of cells to process (used for validation).
 */
void processNetworkCellInfo(Network_Location_t* loc, int number);

/*
 * @brief Global variable to store cell information.
 *
 * This variable is used to hold the formatted cell information string
 * that can be accessed by other parts of the application.  
 */
extern char g_cellInfo[];

void network_info_start(HANDLE taskHandle);

#endif
