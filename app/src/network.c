#include <string.h>
#include <stdio.h>
#include <api_os.h>
#include <api_socket.h>
#include <api_ssl.h>
#include <api_hal_uart.h>
#include <api_debug.h>

#include "network.h"
#include "gps_tracker.h"

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

int Https_Post(SSL_Config_t *sslConfig,
               const char   *hostName, 
               const char   *port, 
               const char   *path, 
               const char   *data, 
               uint16_t      dataLen,
               char*         retBuffer, 
               int           retBufferSize)
{
    if (strlen(data) != dataLen) {
        Trace(1, "dataLen mismatch with actual data length");
        return -1;
    }

    // Get IP from DNS server.
    uint8_t ip[16];
    memset(ip, 0, sizeof(ip));
    if(DNS_GetHostByName2(hostName, ip) != 0) {
        Trace(1,"Cannot resolve the hostName name");
        return -1;
    } else {
        Trace(1,"Resolved IP for %s -> %s", hostName, ip);
    }

    // Create the HTTP POST request package
    const char* fmt = "POST %s HTTP/1.1\r\n"
                      "Host: %s\r\n"
                      "Content-Type: application/x-www-form-urlencoded\r\n"
                      "Connection: close\r\n"
                      "Content-Length: %d\r\n\r\n%s";

    // Calculate the required buffer size
    int bufferLen = snprintf(NULL, 0, fmt, path, hostName, dataLen, data);
    if (bufferLen <= 0) {
        UART_Log("ERROR - Failed to calculate buffer size\r\n");
        return -1;
    }

    // Allocate the buffer dynamically
    char* buffer = (char*)OS_Malloc(bufferLen + 1);
    if (!buffer) {
        UART_Log("ERROR - Failed to allocate memory for HTTP package\r\n");
        return -1;
    }
     
    // Build the package
    snprintf(buffer, bufferLen, fmt, path, hostName, dataLen, data);
    buffer[bufferLen] = 0;
//  UART_Log("HTTP Package:\r\n");
//  UART_Write(UART1, buffer, bufferLen);
//  UART_Log("\r\n");

    SSL_Error_t error;
    sslConfig->caCert = ca_cert;
    sslConfig->hostName = hostName;

    error = SSL_Init(sslConfig);
    if(error != SSL_ERROR_NONE) {
        UART_Log("ERROR - SSL init error: %d\r\n",error);
        goto err_free_buf;
    }

    // Connect to server
    error = SSL_Connect(sslConfig, hostName, port);
    if(error != SSL_ERROR_NONE) {
        UART_Log("ERROR - SSL connect error: %d\r\n",error);
        goto err_ssl_destroy;
    }

    // Send package
    // UART_Log("SSL Write len:%d data:%s\r\n", bufferLen, buffer);
    error = SSL_Write(sslConfig, buffer, bufferLen, SSL_WRITE_TIMEOUT);
    if(error <= 0) {
        UART_Log("ERROR - SSL Write error: %d\r\n", error);
        goto err_ssl_close;
    }

    // Read response
    memset(retBuffer, 0, retBufferSize);
    error = SSL_Read(sslConfig, retBuffer, retBufferSize, SSL_READ_TIMEOUT);
    if(error < 0) {
        UART_Log("ERROR - SSL Read error: %d\r\n", error);
        goto err_ssl_close;
    }
    if(error == 0) {
        UART_Log("ERROR - SSL no receive response\r\n");
        error = SSL_ERROR_INTERNAL;
        goto err_ssl_close;
    }
    // UART_Log("SSL Read: len:%d, data:%s\r\n", error, retBuffer);

err_ssl_close:
    // Close the SSL connection
    if (SSL_Close(sslConfig) != SSL_ERROR_NONE) {
        UART_Log("ERROR - ssl close error: %d\r\n", error);
    }

err_ssl_destroy:
    // Destroy the SSL context
    if (SSL_Destroy(sslConfig) != SSL_ERROR_NONE) {
        UART_Log("ERROR - ssl destroy error: %d\r\n", error);
    } 

err_free_buf:
    OS_Free(buffer);
    return error;
}


int Http_Post(const char  *domain, 
              int          port,
              const char  *path,
              uint8_t     *body, 
              uint16_t     bodyLen, 
              char        *retBuffer, 
              int          bufferLen)
{
    uint8_t ip[16];
    bool flag = false;
    uint16_t recvLen = 0;

    //connect server
    memset(ip,0,sizeof(ip));
    if(DNS_GetHostByName2(domain,ip) != 0)
    {
        Trace(2,"get ip error");
        return -1;
    }
    // Trace(2,"get ip success:%s -> %s",domain,ip);
    char* servInetAddr = ip;
    char* temp = OS_Malloc(2048);
    if(!temp)
    {
        Trace(2,"malloc fail");
        return -1;
    }
    snprintf(temp,2048,"POST %s HTTP/1.1\r\nContent-Type: application/x-www-form-urlencoded\r\nConnection: Keep-Alive\r\nHost: %s\r\nContent-Length: %d\r\n\r\n",
                            path,domain,bodyLen);
    char* pData = temp;
    int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(fd < 0){
        Trace(2,"socket fail");
        OS_Free(temp);
        return -1;
    }
    // Trace(2,"fd:%d",fd);

    struct sockaddr_in sockaddr;
    memset(&sockaddr,0,sizeof(sockaddr));
    sockaddr.sin_family = AF_INET;
    sockaddr.sin_port = htons(port);
    inet_pton(AF_INET,servInetAddr,&sockaddr.sin_addr);

    int ret = connect(fd, (struct sockaddr*)&sockaddr, sizeof(struct sockaddr_in));
    if(ret < 0){
        Trace(2,"socket connect fail");
        OS_Free(temp);
        close(fd);
        return -1;
    }
    // Trace(2,"socket connect success");
    Trace(2,"send request:%s",pData);
    ret = send(fd, pData, strlen(pData), 0);
    if(ret < 0){
        Trace(2,"socket send fail");
        OS_Free(temp);
        close(fd);
        return -1;
    }
    ret = send(fd, body, bodyLen, 0);
    if(ret < 0){
        Trace(2,"socket send fail");
        OS_Free(temp);
        close(fd);
        return -1;
    }
    // Trace(2,"socket send success");

    struct fd_set fds;
    struct timeval timeout={12,0};
    FD_ZERO(&fds);
    FD_SET(fd,&fds);
    while(!flag)
    {
        ret = select(fd+1,&fds,NULL,NULL,&timeout);
        // Trace(2,"select return:%d",ret);
        switch(ret)
        {
            case -1:
                Trace(2,"select error");
                flag = true;
                break;
            case 0:
                Trace(2,"select timeout");
                flag = true;
                break;
            default:
                if(FD_ISSET(fd,&fds))
                {
                    memset(retBuffer,0,bufferLen);
                    ret = recv(fd,retBuffer,bufferLen,0);
                    recvLen += ret;
                    if(ret < 0)
                    {
                        Trace(2,"recv error");
                        flag = true;
                        break;
                    }
                    else if(ret == 0)
                    {
                        Trace(2,"ret == 0");
                        break;
                    }
                    else if(ret < 1352)
                    {
                        UART_Log("recv len:%d, data:%s \r\n", recvLen, retBuffer);
                        close(fd);
                        OS_Free(temp);
                        return recvLen;
                    }
                }
                break;
        }
    }
    close(fd);
    OS_Free(temp);
    return -1;
}
