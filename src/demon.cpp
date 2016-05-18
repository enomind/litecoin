// Copyright (c) 2016 Cardano Labo <dungta@cardano-labo.com>
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "demon.h"

Demon::Demon() {

}

std::string Demon::GetTxJsonFromMempool(CTransaction& tx) {
    TxToJSON(tx, 0, objTx);
    return write_string(Value(objTx), false) + "\n";
}

Demon::~Demon() {

}
