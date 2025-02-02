/*******************************************************************************
*   Ledger Ethereum App
*   (c) 2016-2019 Ledger
*
*  Licensed under the Apache License, Version 2.0 (the "License");
*  you may not use this file except in compliance with the License.
*  You may obtain a copy of the License at
*
*      http://www.apache.org/licenses/LICENSE-2.0
*
*  Unless required by applicable law or agreed to in writing, software
*  distributed under the License is distributed on an "AS IS" BASIS,
*  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*  See the License for the specific language governing permissions and
*  limitations under the License.
********************************************************************************/

#ifndef _ETHUSTREAM_H_
#define _ETHUSTREAM_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef TESTING
typedef void * cx_sha3_t;
#else
#include "os.h"
#include "cx.h"
#endif

#define MAX_INT256 32
#define MAX_ADDRESS 20
#define MAX_V 4

struct txContext_t;

typedef enum customStatus_e {
    CUSTOM_NOT_HANDLED,
    CUSTOM_HANDLED,
    CUSTOM_SUSPENDED,
    CUSTOM_FAULT
} customStatus_e;

typedef customStatus_e (*ustreamProcess_t)(struct txContext_t *context);

typedef enum rlpTxField_e {
    TX_RLP_NONE = 0,
    TX_RLP_CONTENT,
    TX_RLP_TYPE,
    TX_RLP_NONCE,
    TX_RLP_GASPRICE,
    TX_RLP_STARTGAS,
    TX_RLP_FEECURRENCY,
    TX_RLP_GATEWAYTO,
    TX_RLP_GATEWAYFEE,
    TX_RLP_TO,
    TX_RLP_VALUE,
    TX_RLP_DATA,
    TX_RLP_V,
    TX_RLP_R,
    TX_RLP_S,
    TX_RLP_DONE
} rlpTxField_e;

typedef enum parserStatus_e {
    USTREAM_PROCESSING,
    USTREAM_SUSPENDED,
    USTREAM_FINISHED,
    USTREAM_FAULT
} parserStatus_e;

typedef struct txInt256_t {
    uint8_t value[MAX_INT256];
    uint8_t length;
} txInt256_t;

typedef struct txContent_t {
    txInt256_t gasprice;
    txInt256_t startgas;
    txInt256_t value;
    txInt256_t gatewayFee;
    uint8_t destination[MAX_ADDRESS];
    uint8_t destinationLength;
    uint8_t gatewayDestination[MAX_ADDRESS];
    uint8_t gatewayDestinationLength;
    uint8_t feeCurrency[20];
    uint8_t feeCurrencyLength;
    uint8_t v[MAX_V];
    uint8_t vLength;
    bool dataPresent;
} txContent_t;

typedef struct txContext_t {
    rlpTxField_e currentField;
    bool isEthereum;
    cx_sha3_t *sha3;
    uint32_t currentFieldLength;
    uint32_t currentFieldPos;
    bool currentFieldIsList;
    bool processingField;
    bool fieldSingleByte;
    uint32_t dataLength;
    uint8_t rlpBuffer[5];
    uint32_t rlpBufferPos;
    const uint8_t *workBuffer;
    uint32_t commandLength;
    ustreamProcess_t customProcessor;
    txContent_t *content;
    void *extra;
} txContext_t;

void initTx(txContext_t *context, cx_sha3_t *sha3, txContent_t *content,
            ustreamProcess_t customProcessor, bool isEthereum, void *extra);
parserStatus_e processTx(txContext_t *context, const uint8_t *buffer, size_t length);
parserStatus_e continueTx(txContext_t *context);
int copyTxData(txContext_t *context, uint8_t *out, size_t length);

#endif /* _ETHUSTREAM_H_ */
