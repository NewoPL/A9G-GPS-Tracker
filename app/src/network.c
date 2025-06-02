#include <stdio.h>
#include <stdlib.h>

#include <api_os.h>
#include <api_socket.h>
#include <api_ssl.h>

#include "gps_tracker.h"
#include "network.h"
#include "debug.h"

const char *ca_cert = "-----BEGIN CERTIFICATE-----\n\
MIICjjCCAjOgAwIBAgIQf/NXaJvCTjAtkOGKQb0OHzAKBggqhkjOPQQDAjBQMSQw\n\
IgYDVQQLExtHbG9iYWxTaWduIEVDQyBSb290IENBIC0gUjQxEzARBgNVBAoTCkds\n\
b2JhbFNpZ24xEzARBgNVBAMTCkdsb2JhbFNpZ24wHhcNMjMxMjEzMDkwMDAwWhcN\n\
MjkwMjIwMTQwMDAwWjA7MQswCQYDVQQGEwJVUzEeMBwGA1UEChMVR29vZ2xlIFRy\n\
dXN0IFNlcnZpY2VzMQwwCgYDVQQDEwNXRTEwWTATBgcqhkjOPQIBBggqhkjOPQMB\n\
BwNCAARvzTr+Z1dHTCEDhUDCR127WEcPQMFcF4XGGTfn1XzthkubgdnXGhOlCgP4\n\
mMTG6J7/EFmPLCaY9eYmJbsPAvpWo4IBAjCB/zAOBgNVHQ8BAf8EBAMCAYYwHQYD\n\
VR0lBBYwFAYIKwYBBQUHAwEGCCsGAQUFBwMCMBIGA1UdEwEB/wQIMAYBAf8CAQAw\n\
HQYDVR0OBBYEFJB3kjVnxP+ozKnme9mAeXvMk/k4MB8GA1UdIwQYMBaAFFSwe61F\n\
uOJAf/sKbvu+M8k8o4TVMDYGCCsGAQUFBwEBBCowKDAmBggrBgEFBQcwAoYaaHR0\n\
cDovL2kucGtpLmdvb2cvZ3NyNC5jcnQwLQYDVR0fBCYwJDAioCCgHoYcaHR0cDov\n\
L2MucGtpLmdvb2cvci9nc3I0LmNybDATBgNVHSAEDDAKMAgGBmeBDAECATAKBggq\n\
hkjOPQQDAgNJADBGAiEAokJL0LgR6SOLR02WWxccAq3ndXp4EMRveXMUVUxMWSMC\n\
IQDspFWa3fj7nLgouSdkcPy1SdOR2AGm9OQWs7veyXsBwA==\n\
-----END CERTIFICATE-----";

static int http_send_receive(const char *ip, const char *port, char *sendBuffer, int sendBufferLen, char *retBuffer, int retBufferSize)
{
    int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(fd < 0) {
        LOGE("socket fail");
        return -1;
    }

    struct sockaddr_in sockaddr;
    memset(&sockaddr, 0, sizeof(sockaddr));
    sockaddr.sin_family = AF_INET;
    sockaddr.sin_port = htons(atoi(port));
    inet_pton(AF_INET, ip, &sockaddr.sin_addr);

    int ret = connect(fd, (struct sockaddr*)&sockaddr, sizeof(struct sockaddr_in));
    if(ret < 0){
        LOGE("socket connect fail");
        close(fd);
        return -1;
    }
    ret = send(fd, sendBuffer, sendBufferLen, 0);
    if(ret < 0){
        LOGE("socket send fail");
        close(fd);
        return -1;
    }

    struct fd_set fds;
    struct timeval timeout = {12, 0};
    FD_ZERO(&fds);
    FD_SET(fd, &fds);
    uint16_t recvLen = 0;

    while (recvLen < retBufferSize)
    {
        ret = select(fd+1, &fds, NULL, NULL, &timeout);
        if (ret == -1) {
            LOGE("HTTP response error");
            close(fd);
            return -1;
        }
        if (ret == 0) {
            LOGE("HTTP response timeout");
            close(fd);
            return -1;
        }
        if(FD_ISSET(fd, &fds))
        {
            int toRead = retBufferSize - recvLen;
            ret = recv(fd, retBuffer + recvLen, toRead, 0);
            if(ret < 0)
            {
                LOGE("recv error");
                close(fd);
                return -1;
            }
            if(ret == 0)
            {
                LOGI("connection closed by peer");
                break;
            }
            recvLen += ret;
        }
    }
    close(fd);
    return recvLen;
}


static int https_send_receive(SSL_Config_t *sslConfig,
                              const char   *hostName,
                              const char   *port,
                              char         *buffer,
                              int           bufferLen,
                              char         *retBuffer,
                              int           retBufferSize)
{
    SSL_Error_t error;
    sslConfig->caCert = ca_cert;
    sslConfig->hostName = hostName;

    error = SSL_Init(sslConfig);
    if(error != SSL_ERROR_NONE) {
        LOGD("SSL init error: %d", error);
        OS_Free(buffer);
        return error;
    }

    // Connect to server
    error = SSL_Connect(sslConfig, hostName, port);
    if(error != SSL_ERROR_NONE) {
        LOGD("SSL connect error: %d", error);
        SSL_Destroy(sslConfig);
        OS_Free(buffer);
        return error;
    }

    // Send package
    error = SSL_Write(sslConfig, buffer, bufferLen, SSL_WRITE_TIMEOUT);
    if(error <= 0) {
        LOGD("SSL Write error: %d", error);
        SSL_Close(sslConfig);
        SSL_Destroy(sslConfig);
        OS_Free(buffer);
        return error;
    }

    // Read response
    memset(retBuffer, 0, retBufferSize);
    error = SSL_Read(sslConfig, retBuffer, retBufferSize, SSL_READ_TIMEOUT);
    if(error < 0) {
        LOGD("SSL Read error: %d", error);
        SSL_Close(sslConfig);
        SSL_Destroy(sslConfig);
        OS_Free(buffer);
        return error;
    }
    if(error == 0) {
        LOGD("SSL no receive response");
        error = SSL_ERROR_INTERNAL;
        SSL_Close(sslConfig);
        SSL_Destroy(sslConfig);
        OS_Free(buffer);
        return error;
    }

    if (SSL_Close(sslConfig) != SSL_ERROR_NONE) {
        LOGD("SSL close error: %d", error);
    }

    if (SSL_Destroy(sslConfig) != SSL_ERROR_NONE) {
        LOGD("SSL destroy error: %d", error);
    }

    return error;
}


int Http_Post(SSL_Config_t *sslConfig,
              const char   *hostName, 
              const char   *port, 
              const char   *path, 
              const char   *data, 
              uint16_t      dataLen,
              char*         retBuffer, 
              int           retBufferSize)
{
    if (strlen(data) != dataLen) {
        LOGE("dataLen mismatch with actual data length");
        return -1;
    }

    // Get IP from DNS server.
    uint8_t IPAddr[16];
    memset(IPAddr, 0, sizeof(IPAddr));
    if(DNS_GetHostByName2(hostName, IPAddr) != 0) {
        LOGE("Cannot resolve the hostName name");
        return -1;
    }
    LOGD("Resolved IP for %s -> %s", hostName, IPAddr);
    
    // Create the HTTP POST request package
    const char* fmt = "POST %s HTTP/1.1\r\n"
                      "Host: %s\r\n"
                      "Content-Type: application/x-www-form-urlencoded\r\n"
                      "Connection: close\r\n"
                      "Content-Length: %d\r\n\r\n%s";

    // Calculate the required buffer size
    int bufferLen = snprintf(NULL, 0, fmt, path, hostName, dataLen, data);
    if (bufferLen <= 0) {
        LOGE("Failed to calculate buffer size");
        return -1;
    }

    // Allocate the buffer dynamically
    char* buffer = (char*)OS_Malloc(bufferLen + 1);
    if (!buffer) {
        LOGE("Failed to allocate memory for HTTP package");
        return -1;
    }
     
    // Build the package
    snprintf(buffer, bufferLen, fmt, path, hostName, dataLen, data);
    buffer[bufferLen] = 0;

#if 0
    UART_Printf("HTTP Package:\r\n");
    UART_Write(UART1, buffer, bufferLen);
    UART_Printf("\r\n");
#endif

    int returnVal =  -1;

    if (sslConfig == NULL)
        // Use HTTP for non-secure connection
        returnVal = http_send_receive(IPAddr,
                                      port,
                                      buffer,
                                      bufferLen,
                                      retBuffer,
                                      retBufferSize);
    else
        // Use SSL for secure connection        
        returnVal = https_send_receive(sslConfig,
                                       IPAddr,
                                       port,
                                       buffer,
                                       bufferLen,
                                       retBuffer,
                                       retBufferSize);
        
    OS_Free(buffer);
    return returnVal;
}

