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

#include <stdint.h>

#include <boost/assign/list_of.hpp>
#include "json/json_spirit_utils.h"
#include "json/json_spirit_value.h"

using namespace boost::assign;
using namespace json_spirit;

extern void TxToJSON(const CTransaction& tx, const uint256 hashBlock, Object& entry);

#endif /* DEMON_H */

/**
 * Demon is hook object which is inject to litecoind
 */
class Demon {
    
private:
    /* Private string buffer for tx */
    Object objTx;

public:
    
    /**
     * Construction
     */
    Demon();
    
    /**
     * Render tx from mempool which blockhash = 0x00
     * @param CTransaction& tx
     * @return string 
     */
    std::string GetTxJsonFromMempool(CTransaction& tx);
    
    /**
     * Deconstruction
     */
    ~Demon();
    
};