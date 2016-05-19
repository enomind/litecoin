// Copyright (c) 2016 Cardano Labo <dungta@cardano-labo.com>
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "demon.h"

/**
 * Data convertion
 * @param <int64_t> nTimeout
 * @return <timeval>
 */
struct timeval static MillisToTimeval(int64_t nTimeout)
{
    struct timeval timeout;
    timeout.tv_sec  = nTimeout / 1000;
    timeout.tv_usec = (nTimeout % 1000) * 1000;
    return timeout;
}

/**
 * Create and connect socket
 * @param <CService> addrConnect
 * @param <SOCKET> hSocketRet
 * @param <int> nTimeout
 * @return <bool>
 */
bool static ConnectSocketDirectly(const CService &addrConnect, SOCKET& hSocketRet, int nTimeout)
{
    hSocketRet = INVALID_SOCKET;

    struct sockaddr_storage sockaddr;
    socklen_t len = sizeof(sockaddr);
    if (!addrConnect.GetSockAddr((struct sockaddr*)&sockaddr, &len)) {
        LogPrintf("Cannot connect to %s: unsupported network\n", addrConnect.ToString());
        return false;
    }

    SOCKET hSocket = socket(((struct sockaddr*)&sockaddr)->sa_family, SOCK_STREAM, IPPROTO_TCP);
    if (hSocket == INVALID_SOCKET)
        return false;

    int set = 1;
#ifdef SO_NOSIGPIPE
    // Different way of disabling SIGPIPE on BSD
    setsockopt(hSocket, SOL_SOCKET, SO_NOSIGPIPE, (void*)&set, sizeof(int));
#endif

    //Disable Nagle's algorithm
#ifdef WIN32
    setsockopt(hSocket, IPPROTO_TCP, TCP_NODELAY, (const char*)&set, sizeof(int));
#else
    setsockopt(hSocket, IPPROTO_TCP, TCP_NODELAY, (void*)&set, sizeof(int));
#endif

    // Set to non-blocking
    if (!SetSocketNonBlocking(hSocket, true))
        return error("ConnectSocketDirectly: Setting socket to non-blocking failed, error %s\n", NetworkErrorString(WSAGetLastError()));

    if (connect(hSocket, (struct sockaddr*)&sockaddr, len) == SOCKET_ERROR)
    {
        int nErr = WSAGetLastError();
        // WSAEINVAL is here because some legacy version of winsock uses it
        if (nErr == WSAEINPROGRESS || nErr == WSAEWOULDBLOCK || nErr == WSAEINVAL)
        {
            struct timeval timeout = MillisToTimeval(nTimeout);
            fd_set fdset;
            FD_ZERO(&fdset);
            FD_SET(hSocket, &fdset);
            int nRet = select(hSocket + 1, NULL, &fdset, NULL, &timeout);
            if (nRet == 0)
            {
                LogPrint("net", "connection to %s timeout\n", addrConnect.ToString());
                CloseSocket(hSocket);
                return false;
            }
            if (nRet == SOCKET_ERROR)
            {
                LogPrintf("select() for %s failed: %s\n", addrConnect.ToString(), NetworkErrorString(WSAGetLastError()));
                CloseSocket(hSocket);
                return false;
            }
            socklen_t nRetSize = sizeof(nRet);
#ifdef WIN32
            if (getsockopt(hSocket, SOL_SOCKET, SO_ERROR, (char*)(&nRet), &nRetSize) == SOCKET_ERROR)
#else
            if (getsockopt(hSocket, SOL_SOCKET, SO_ERROR, &nRet, &nRetSize) == SOCKET_ERROR)
#endif
            {
                LogPrintf("getsockopt() for %s failed: %s\n", addrConnect.ToString(), NetworkErrorString(WSAGetLastError()));
                CloseSocket(hSocket);
                return false;
            }
            if (nRet != 0)
            {
                LogPrintf("connect() to %s failed after select(): %s\n", addrConnect.ToString(), NetworkErrorString(nRet));
                CloseSocket(hSocket);
                return false;
            }
        }
#ifdef WIN32
        else if (WSAGetLastError() != WSAEISCONN)
#else
        else
#endif
        {
            LogPrintf("connect() to %s failed: %s\n", addrConnect.ToString(), NetworkErrorString(WSAGetLastError()));
            CloseSocket(hSocket);
            return false;
        }
    }

    hSocketRet = hSocket;
    return true;
}

/**
 * Demon methods
 */

Demon::Demon() {

}

std::string Demon::GetTxJsonFromMempool(CTransaction& tx) {
    TxToJSON(tx, 0, objTx);
    return write_string(Value(objTx), false) + "\n";
}

std::string Demon::GetBlock(const CBlock& block, const CBlockIndex* blockindex, bool txDetails) {
    objBlock = blockToJSON(block, blockindex, txDetails);
    return write_string(Value(objBlock), false) + "\n";
}

std::string Demon::GetBlockDetail(const CBlock& block, const CBlockIndex* blockindex) {
    return GetBlock(block, blockindex, true);
}

bool Demon::ReadBlockFromDisk(CBlock& block, CBlockIndex* blockindex) {
    return ::ReadBlockFromDisk(block, blockindex);
}

Demon::~Demon() {

}

/**
 * Demon client methods
 */

DemonClient::DemonClient() : socketHandler(INVALID_SOCKET) {

}

DemonClient::~DemonClient() {
    DemonClient::Disconnect();
}

bool DemonClient::Connect() {
    CService remoteHost(DEMON_CLIENT_HOST, DEMON_CLIENT_PORT, true);
    return ::ConnectSocketDirectly(remoteHost, socketHandler, DEMON_CLIENT_TIMEOUT);
}

int DemonClient::getState() {
    //Don't try more to do nothing
    if (socketHandler == INVALID_SOCKET) {
        return false;
    }

    //Determine socket state
    int errorCode = 0;
    int errorCodeSize = sizeof (errorCode);
    getsockopt(socketHandler, SOL_SOCKET, SO_ERROR, &errorCode, (socklen_t *) & errorCodeSize);
    return errorCode <= 0;
}

bool DemonClient::Recv(char* data, size_t len) {
    int64_t curTime = GetTimeMillis();
    int64_t endTime = curTime + DEMON_CLIENT_TIMEOUT;
    // Maximum time to wait in one select call. It will take up until this time (in millis)
    // to break off in case of an interruption.
    const int64_t maxWait = 1000;
    while (len > 0 && curTime < endTime) {
        ssize_t ret = recv(socketHandler, data, len, 0); // Optimistically try the recv first
        if (ret > 0) {
            len -= ret;
            data += ret;
        } else if (ret == 0) { // Unexpected disconnection
            return false;
        } else { // Other error or blocking
            int nErr = WSAGetLastError();
            if (nErr == WSAEINPROGRESS || nErr == WSAEWOULDBLOCK || nErr == WSAEINVAL) {
                if (!IsSelectableSocket(socketHandler)) {
                    return false;
                }
                struct timeval tval = MillisToTimeval(std::min(endTime - curTime, maxWait));
                fd_set fdset;
                FD_ZERO(&fdset);
                FD_SET(socketHandler, &fdset);
                int nRet = select(socketHandler + 1, &fdset, NULL, NULL, &tval);
                if (nRet == SOCKET_ERROR) {
                    return false;
                }
            } else {
                return false;
            }
        }
        curTime = GetTimeMillis();
    }
    return len == 0;
}

bool DemonClient::Send(char* data, size_t len) {
    return send(socketHandler, data, len, MSG_NOSIGNAL | MSG_DONTWAIT) > 0;
}

bool DemonClient::Disconnect() {
    return ::CloseSocket(socketHandler);
}