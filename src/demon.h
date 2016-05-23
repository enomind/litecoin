// Copyright (c) 2016 Cardano Labo
// Copyright (c) 2016 Dung Tran <tad88.dev@gmail.com>
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
static const char* DEMON_CLIENT_HOST = "localhost";
static const unsigned short DEMON_CLIENT_PORT = 8080;
static const int DEMON_CLIENT_TIMEOUT = 30 * 1000;

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
    DemonClient();

    /**
     * Deconstruction
     */
    ~DemonClient();

    /**
     * Connect to configed server
     * @return <bool>
     */
    bool Connect();

    /**
     * Get socket state
     * @return <bool>
     */
    int getState();

    /**
     * Recv data from socket handler
     * @param <char*> data
     * @param <size_t> len
     * @return <bool>
     */
    bool Recv(char* data, size_t len);

    /**
     * Send data through socket handler
     * @param <char*> data
     * @param <size_t> len
     * @return <bool>
     */
    bool Send(char* data, size_t len);

    /**
     * Close socket
     * @return <bool>
     */
    bool Disconnect();

};

#endif /* DEMON_H */