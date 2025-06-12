#include <stdio.h>
#include <stdlib.h>

#include <api_os.h>
#include <api_socket.h>
#include <api_ssl.h>


#include "utils.h"
#include "http.h"
#include "config_store.h"
#include "debug.h"

#define MODULE_TAG "Network"

const char ca_cert[] = "-----BEGIN CERTIFICATE-----\n\
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

static SSL_Config_t SSLconfig;

static inline int http_send_receive(const char *hostName,
                                    const char *port,
                                    char       *sendBuffer,
                                    int         sendBufferLen,
                                    char       *retBuffer,
                                    int         retBufferSize)
{
    if (!hostName || !port || !sendBuffer || sendBufferLen <= 0 || !retBuffer || retBufferSize <= 0) {
        LOGE("Invalid input parameters");
        return -1;
    }

    // Get IP from DNS server.
    char IPAddr[INET_ADDRSTRLEN];
    memset(IPAddr, 0, sizeof(IPAddr));
    if(DNS_GetHostByName2(hostName, IPAddr) != 0) {
        LOGE("Cannot resolve the hostName name");
        return -1;
    }
    LOGD("Resolved IP for %s -> %s", hostName, IPAddr);

    int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(fd < 0) {
        LOGE("socket fail");
        return -1;
    }

    int port_num = strtol(port, NULL, 10);
    if (port_num <= 0 || port_num > 65535) {
        LOGE("Invalid port number");
        close(fd);
        return -1;
    }

    struct sockaddr_in sockaddr;
    memset(&sockaddr, 0, sizeof(sockaddr));
    sockaddr.sin_family = AF_INET;
    sockaddr.sin_port = htons(port_num);
    inet_pton(AF_INET, IPAddr, &sockaddr.sin_addr);

    int ret = connect(fd, (struct sockaddr*)&sockaddr, sizeof(struct sockaddr_in));
    if(ret < 0){
        LOGE("socket connect fail");
        close(fd);
        return -1;
    }

    int totalSent = 0;
    while (totalSent < sendBufferLen) {
        ret = send(fd, sendBuffer + totalSent, sendBufferLen - totalSent, 0);
        if (ret <= 0) {
            LOGE("socket send fail");
            close(fd);
            return -1;
        }
        totalSent += ret;
    }

    uint16_t recvLen = 0;
    while (recvLen < retBufferSize)
    {
        struct fd_set fds;
        struct timeval timeout = {12, 0};
        FD_ZERO(&fds);
        FD_SET(fd, &fds);

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


static inline int https_send_receive(const char   *hostName,
                                     const char   *port,
                                     char         *buffer,
                                     int           bufferLen,
                                     char         *retBuffer,
                                     int           retBufferSize)
{
    SSL_Error_t error;

    // Setup full SSL config
    memset(&SSLconfig, 0, sizeof(SSL_Config_t));
    SSLconfig.caCert          = ca_cert;
    SSLconfig.caCrl           = NULL;
    SSLconfig.clientCert      = NULL;
    SSLconfig.clientKey       = NULL;
    SSLconfig.clientKeyPasswd = NULL;
    SSLconfig.hostName        = hostName;
    SSLconfig.minVersion      = SSL_VERSION_SSLv3;
    SSLconfig.maxVersion      = SSL_VERSION_TLSv1_2;
    SSLconfig.verifyMode      = SSL_VERIFY_MODE_OPTIONAL;
    SSLconfig.entropyCustom   = "GPRS";

    error = SSL_Init(&SSLconfig);
    if(error != SSL_ERROR_NONE) {
        LOGI("SSL init error: %d", error);
        return error;
    }

    // Connect to server using IP address
    error = SSL_Connect(&SSLconfig, hostName, port);
    if(error != SSL_ERROR_NONE) {
        LOGI("SSL connect error: %d", error);
        goto err_ssl_destroy;
    }

    // Send package
    error = SSL_Write(&SSLconfig, buffer, bufferLen, SSL_WRITE_TIMEOUT);
    if(error <= 0) {
        LOGI("SSL Write error: %d", error);
        goto err_ssl_close;
    }

    // Read response
    memset(retBuffer, 0, retBufferSize);
    error = SSL_Read(&SSLconfig, retBuffer, retBufferSize, SSL_READ_TIMEOUT);
    if(error < 0) {
        LOGI("SSL Read error: %d", error);
        goto err_ssl_close;
    }
    if(error == 0) {
        LOGI("SSL no receive response");
        error = SSL_ERROR_INTERNAL;
        goto err_ssl_close;
    }

err_ssl_close:
    if (SSL_Close(&SSLconfig) != SSL_ERROR_NONE) {
        LOGI("SSL close error: %d", error);
    }

err_ssl_destroy:
    if (SSL_Destroy(&SSLconfig) != SSL_ERROR_NONE) {
        LOGI("SSL destroy error: %d", error);
    }

    return error;
}


int Http_Post(bool          secure,
              const char   *hostName, 
              const char   *port, 
              const char   *path, 
              const char   *data, 
              uint16_t      dataLen,
              char*         retBuffer, 
              int           retBufferSize)
{
    if (strlen(data) != dataLen) {
        LOGE("DataLen mismatch with actual data length");
        return -1;
    }

    // Create the HTTP POST request template
    const char* fmt = "POST %s HTTP/1.1\r\n"
                      "Host: %s\r\n"
                      "Content-Type: application/x-www-form-urlencoded\r\n"
                      "Connection: Keep-Alive\r\n"
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
     
    // Build the complete HTTP POST request
    snprintf(buffer, bufferLen, fmt, path, hostName, dataLen, data);
    buffer[bufferLen] = 0;

#if 0
    UART_Printf("HTTP Package:\r\n");
    UART_Write(UART1, buffer, bufferLen);
    UART_Printf("\r\n");
#endif

    int returnVal =  -1;

    if (!secure)
        // Use HTTP for non-secure connection
        returnVal = http_send_receive(hostName,
                                      port,
                                      buffer,
                                      bufferLen,
                                      retBuffer,
                                      retBufferSize);
    else
        // Use SSL for secure connection        
        returnVal = https_send_receive(hostName,
                                       port,
                                       buffer,
                                       bufferLen,
                                       retBuffer,
                                       retBufferSize);
        
    OS_Free(buffer);
    return returnVal;
}
