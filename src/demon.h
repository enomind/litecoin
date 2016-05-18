// Copyright (c) 2016 Cardano Labo <dungta@cardano-labo.com>
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef DEMON_H
#define DEMON_H

#include "base58.h"
#include "primitives/transaction.h"
#include "core_io.h"
#include "init.h"
#include "keystore.h"
#include "main.h"
#include "net.h"
#include "rpcserver.h"
#include "script/script.h"
#include "script/sign.h"
#include "script/standard.h"
#include "uint256.h"
#include <iostream>
#include <string>
#include "util.h"

#include <stdint.h>

#include <boost/assign/list_of.hpp>
#include "json/json_spirit_utils.h"
#include "json/json_spirit_value.h"

using namespace boost::assign;
using namespace json_spirit;

void TxToJSON(const CTransaction& tx, const uint256 hashBlock, Object& entry);
Object blockToJSON(const CBlock& block, const CBlockIndex* blockindex, bool txDetails = false);
bool ReadBlockFromDisk(CBlock& block, const CDiskBlockPos& pos);

int64_t GetTimeMillis();
extern bool CloseSocket(SOCKET& hSocket);
struct timeval static MillisToTimeval(int64_t nTimeout)
{
    struct timeval timeout;
    timeout.tv_sec  = nTimeout / 1000;
    timeout.tv_usec = (nTimeout % 1000) * 1000;
    return timeout;
}

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

const static char* DEMON_CLIENT_HOST = "www.google.com";
const static unsigned short DEMON_CLIENT_PORT = 80;
const static int DEMON_CLIENT_TIMEOUT = 30 * 1000;

/**
 * Demon is object which is inject to litecoin daemon
 */
class Demon {
private:
    /* Private vector<std::string> buffer for tx */
    Object objTx;

    /* Private vector<std::string> buffer for block */
    Object objBlock;

public:

    /**
     * Construction
     */
    Demon();

    /**
     * Render tx from mempool which blockhash = 0x00
     * @param <CTransaction&> tx
     * @return <string>
     */
    std::string GetTxJsonFromMempool(CTransaction& tx);

    /**
     * Get block
     * @param <CBlock&> block
     * @param <CBlockIndex*> blockindex
     * @param <txDetail> txDetails
     * @return <string>
     */
    std::string GetBlock(const CBlock& block, const CBlockIndex* blockindex, bool txDetails);

    /**
     * Get block detail
     * @param <CBlock&> block
     * @param <CBlockIndex*> blockindex
     * @return <string>
     */
    std::string GetBlockDetail(const CBlock& block, const CBlockIndex* blockindex);

    /**
     * Read block from disk
     * @param <CBlock&> block
     * @param <CBlockIndex*> blockindex
     * @return <bool>
     */
    bool ReadBlockFromDisk(CBlock& block, CBlockIndex* blockindex);

    /**
     * Deconstruction
     */
    ~Demon();

};

/**
 * Demon client push data to outside
 */
class DemonClient {
private:
    /* Internal socket handler */
    SOCKET socketHandler;
public:

    /**
     * Construction
     */
    DemonClient() : socketHandler(INVALID_SOCKET) {

    }

    /**
     * Deconstruction
     */
    ~DemonClient() {
        DemonClient::Disconnect();
    }

    /**
     * Connect to configed server
     * @return <bool>
     */
    bool Connect() {
        CService remoteHost(DEMON_CLIENT_HOST, DEMON_CLIENT_PORT, true);
        return ::ConnectSocketDirectly(remoteHost, socketHandler, DEMON_CLIENT_TIMEOUT);
    }

    /**
     * Get socket state
     * @return <bool>
     */
    int getState() {
        //Don't try more to do nothing
        if (socketHandler == INVALID_SOCKET) {
            return false;
        }

        //Determine socket state
        int errorCode = 0;
        int errorCodeSize = sizeof (errorCode);
        getsockopt(socketHandler, SOL_SOCKET, SO_ERROR, &errorCode, (socklen_t *)&errorCodeSize);
        return errorCode <= 0;
    }

    /**
     * Recv data from socket handler
     * @param <char*> data
     * @param <size_t> len
     * @return <bool>
     */
    bool Recv(char* data, size_t len) {
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

    /**
     * Send data through socket handler
     * @param <char*> data
     * @param <size_t> len
     * @return <bool>
     */
    bool Send(char* data, size_t len) {
        return send(socketHandler, data, len, MSG_NOSIGNAL | MSG_DONTWAIT) > 0;
    }

    /**
     * Close socket
     * @return <bool>
     */
    bool Disconnect() {
        return ::CloseSocket(socketHandler);
    }

};

#endif /* DEMON_H */