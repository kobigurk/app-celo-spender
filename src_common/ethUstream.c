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

#include "ethUstream.h"
#include "rlp.h"

#include <stdint.h>
#include <string.h>

#ifdef TESTING
#define PRINTF(...)
#endif

static int readTxByte(txContext_t *context, uint8_t *byte) {
    uint8_t data;

    if (context->commandLength < 1) {
        PRINTF("readTxByte Underflow\n");
        return -1;
    }
    data = *context->workBuffer;
    context->workBuffer++;
    context->commandLength--;
    if (context->processingField) {
        context->currentFieldPos++;
    }
#ifndef TESTING
    if (!(context->processingField && context->fieldSingleByte)) {
        cx_hash((cx_hash_t*)context->sha3, 0, &data, 1, NULL, 0);
    }
#endif
    if (byte) {
        *byte = data;
    }
    return 0;
}

int copyTxData(txContext_t *context, uint8_t *out, size_t length)  {
    if (context->commandLength < length) {
        PRINTF("copyTxData Underflow\n");
        return -1;
    }
    if (out != NULL) {
        memcpy(out, context->workBuffer, length);
    }
#ifndef TESTING
    if (!(context->processingField && context->fieldSingleByte)) {
        cx_hash((cx_hash_t*)context->sha3, 0, context->workBuffer, length, NULL, 0);
    }
#endif
    context->workBuffer += length;
    context->commandLength -= length;
    if (context->processingField) {
        context->currentFieldPos += length;
    }
    return 0;
}

static int processContent(txContext_t *context) {
    // Keep the full length for sanity checks, move to the next field
    if (!context->currentFieldIsList) {
        PRINTF("Invalid type for RLP_CONTENT\n");
        return -1;
    }
    context->dataLength = context->currentFieldLength;
    context->currentField++;
    context->processingField = false;
    return 0;
}


static int processType(txContext_t *context) {
    if (context->currentFieldIsList) {
        PRINTF("Invalid type for RLP_TYPE\n");
        return -1;
    }
    if (context->currentFieldLength > MAX_INT256) {
        PRINTF("Invalid length for RLP_TYPE\n");
        return -1;
    }
    if (context->currentFieldPos < context->currentFieldLength) {
        uint32_t copySize =
            (context->commandLength <
                     ((context->currentFieldLength - context->currentFieldPos))
                 ? context->commandLength
                 : context->currentFieldLength - context->currentFieldPos);
        if (copyTxData(context, NULL, copySize)) {
            return -1;
        }
    }
    if (context->currentFieldPos == context->currentFieldLength) {
        context->currentField++;
        context->processingField = false;
    }
    return 0;
}

static int processNonce(txContext_t *context) {
    if (context->currentFieldIsList) {
        PRINTF("Invalid type for RLP_NONCE\n");
        return -1;
    }
    if (context->currentFieldLength > MAX_INT256) {
        PRINTF("Invalid length for RLP_NONCE\n");
        return -1;
    }
    if (context->currentFieldPos < context->currentFieldLength) {
        uint32_t copySize =
            (context->commandLength <
                     ((context->currentFieldLength - context->currentFieldPos))
                 ? context->commandLength
                 : context->currentFieldLength - context->currentFieldPos);
        if (copyTxData(context, NULL, copySize)) {
            return -1;
        }
    }
    if (context->currentFieldPos == context->currentFieldLength) {
        context->currentField++;
        context->processingField = false;
    }
    return 0;
}

static int processStartGas(txContext_t *context) {
    if (context->currentFieldIsList) {
        PRINTF("Invalid type for RLP_STARTGAS\n");
        return -1;
    }
    if (context->currentFieldLength > MAX_INT256) {
        PRINTF("Invalid length for RLP_STARTGAS %d\n",
                      context->currentFieldLength);
        return -1;
    }
    if (context->currentFieldPos < context->currentFieldLength) {
        uint32_t copySize =
            (context->commandLength <
                     ((context->currentFieldLength - context->currentFieldPos))
                 ? context->commandLength
                 : context->currentFieldLength - context->currentFieldPos);
        if (copyTxData(context, context->content->startgas.value + context->currentFieldPos, copySize)) {
            return -1;
        }
    }
    if (context->currentFieldPos == context->currentFieldLength) {
        context->content->startgas.length = context->currentFieldLength;
        context->currentField++;
        context->processingField = false;
    }
    return 0;
}

static int processGasprice(txContext_t *context) {
    if (context->currentFieldIsList) {
        PRINTF("Invalid type for RLP_GASPRICE\n");
        return -1;
    }
    if (context->currentFieldLength > MAX_INT256) {
        PRINTF("Invalid length for RLP_GASPRICE\n");
        return -1;
    }
    if (context->currentFieldPos < context->currentFieldLength) {
        uint32_t copySize =
            (context->commandLength <
                     ((context->currentFieldLength - context->currentFieldPos))
                 ? context->commandLength
                 : context->currentFieldLength - context->currentFieldPos);
        if (copyTxData(context, context->content->gasprice.value + context->currentFieldPos, copySize)) {
            return -1;
        }
    }
    if (context->currentFieldPos == context->currentFieldLength) {
        context->content->gasprice.length = context->currentFieldLength;
        context->currentField++;
        context->processingField = false;
    }
    return 0;
}

static int processGatewayFee(txContext_t *context) {
    if (context->currentFieldIsList) {
        PRINTF("Invalid type for RLP_GATEWAYFEE\n");
        return -1;
    }
    if (context->currentFieldLength > MAX_INT256) {
        PRINTF("Invalid length for RLP_GATEWAYFEE\n");
        return -1;
    }
    if (context->currentFieldPos < context->currentFieldLength) {
        uint32_t copySize =
            (context->commandLength <
                     ((context->currentFieldLength - context->currentFieldPos))
                 ? context->commandLength
                 : context->currentFieldLength - context->currentFieldPos);
        if (copyTxData(context, context->content->gatewayFee.value + context->currentFieldPos, copySize)) {
            return -1;
        }
    }
    if (context->currentFieldPos == context->currentFieldLength) {
        context->content->gatewayFee.length = context->currentFieldLength;
        context->currentField++;
        context->processingField = false;
    }
    return 0;
}

static int processFeeCurrency(txContext_t *context) {
    if (context->currentFieldIsList) {
        PRINTF("Invalid type for RLP_FEECURRENCY\n");
        return -1;
    }
    if (context->currentFieldLength > MAX_ADDRESS) {
        PRINTF("Invalid length for RLP_FEECURRENCY\n");
        return -1;
    }
    if (context->currentFieldPos < context->currentFieldLength) {
        uint32_t copySize =
            (context->commandLength <
                     ((context->currentFieldLength - context->currentFieldPos))
                 ? context->commandLength
                 : context->currentFieldLength - context->currentFieldPos);
        if (copyTxData(context, context->content->feeCurrency + context->currentFieldPos, copySize)) {
            return -1;
        }
    }
    if (context->currentFieldPos == context->currentFieldLength) {
        context->content->feeCurrencyLength = context->currentFieldLength;    
        context->currentField++;
        context->processingField = false;
    }
    return 0;
}

static int processGatewayTo(txContext_t *context) {
    if (context->currentFieldIsList) {
        PRINTF("Invalid type for RLP_GATEWAYTO\n");
        return -1;
    }
    if (context->currentFieldLength != 0 && context->currentFieldLength != MAX_ADDRESS) {
        PRINTF("Invalid length for RLP_GATEWAYTO\n");
        return -1;
    }
    if (context->currentFieldPos < context->currentFieldLength) {
        uint32_t copySize =
            (context->commandLength <
                     ((context->currentFieldLength - context->currentFieldPos))
                 ? context->commandLength
                 : context->currentFieldLength - context->currentFieldPos);
        if (copyTxData(context, context->content->gatewayDestination + context->currentFieldPos, copySize)) {
            return -1;
        }
    }
    if (context->currentFieldPos == context->currentFieldLength) {
        context->content->gatewayDestinationLength = context->currentFieldLength;
        context->currentField++;
        context->processingField = false;
    }
    return 0;
}

static int processValue(txContext_t *context) {
    if (context->currentFieldIsList) {
        PRINTF("Invalid type for RLP_VALUE\n");
        return -1;
    }
    if (context->currentFieldLength > MAX_INT256) {
        PRINTF("Invalid length for RLP_VALUE\n");
        return -1;
    }
    if (context->currentFieldPos < context->currentFieldLength) {
        uint32_t copySize =
            (context->commandLength <
                     ((context->currentFieldLength - context->currentFieldPos))
                 ? context->commandLength
                 : context->currentFieldLength - context->currentFieldPos);
        if (copyTxData(context, context->content->value.value + context->currentFieldPos, copySize)) {
            return -1;
        }
    }
    if (context->currentFieldPos == context->currentFieldLength) {
        context->content->value.length = context->currentFieldLength;
        context->currentField++;
        context->processingField = false;
    }
    return 0;
}

static int processTo(txContext_t *context) {
    if (context->currentFieldIsList) {
        PRINTF("Invalid type for RLP_TO\n");
        return -1;
    }
    if (context->currentFieldLength != 0 && context->currentFieldLength != MAX_ADDRESS) {
        PRINTF("Invalid length for RLP_TO\n");
        return -1;
    }
    if (context->currentFieldPos < context->currentFieldLength) {
        uint32_t copySize =
            (context->commandLength <
                     ((context->currentFieldLength - context->currentFieldPos))
                 ? context->commandLength
                 : context->currentFieldLength - context->currentFieldPos);
        if (copyTxData(context, context->content->destination + context->currentFieldPos, copySize)) {
            return -1;
        }
    }
    if (context->currentFieldPos == context->currentFieldLength) {
        context->content->destinationLength = context->currentFieldLength;
        context->currentField++;
        context->processingField = false;
    }
    return 0;
}

static int processData(txContext_t *context) {
    if (context->currentFieldIsList) {
        PRINTF("Invalid type for RLP_DATA\n");
        return -1;
    }
    if (context->currentFieldPos < context->currentFieldLength) {
        uint32_t copySize =
            (context->commandLength <
                     ((context->currentFieldLength - context->currentFieldPos))
                 ? context->commandLength
                 : context->currentFieldLength - context->currentFieldPos);
        if (copyTxData(context, NULL, copySize)) {
            return -1;
        }
    }
    if (context->currentFieldPos == context->currentFieldLength) {
        context->currentField++;
        context->processingField = false;
    }
    return 0;
}

static int processV(txContext_t *context) {
    if (context->currentFieldIsList) {
        PRINTF("Invalid type for RLP_V\n");
        return -1;
    }
    if (context->currentFieldLength > MAX_V) {
        PRINTF("Invalid length for RLP_V\n");
        return -1;
    }
    if (context->currentFieldPos < context->currentFieldLength) {
        uint32_t copySize =
            (context->commandLength <
                     ((context->currentFieldLength - context->currentFieldPos))
                 ? context->commandLength
                 : context->currentFieldLength - context->currentFieldPos);
        if (copyTxData(context, context->content->v + context->currentFieldPos, copySize)) {
            return -1;
        }
    }
    if (context->currentFieldPos == context->currentFieldLength) {
        context->content->vLength = context->currentFieldLength;
        context->currentField++;
        context->processingField = false;
    }
    return 0;
}


static parserStatus_e processTxInternal(txContext_t *context) {
    for (;;) {
        customStatus_e customStatus = CUSTOM_NOT_HANDLED;
        // EIP 155 style transasction
        if (context->currentField == TX_RLP_DONE) {
            return USTREAM_FINISHED;
        }
        // Old style transaction
        if ((context->currentField == TX_RLP_V) && (context->commandLength == 0)) {
            context->content->vLength = 0;
            // We don't want to support old style transactions. We treat an empty V as a false positive
            // - data ended exactly on the APDU boundary, and so we tell the processing to continue.
            return USTREAM_PROCESSING;
        }
        if (context->commandLength == 0) {
            return USTREAM_PROCESSING;
        }
        if (!context->processingField) {
            bool canDecode = false;
            uint32_t offset;
            while (context->commandLength != 0) {
                bool valid;
                // Feed the RLP buffer until the length can be decoded
                uint8_t byte;
                if (readTxByte(context, &byte)) {
                    return USTREAM_FAULT;
                }
                context->rlpBuffer[context->rlpBufferPos++] = byte;
                if (rlpCanDecode(context->rlpBuffer, context->rlpBufferPos,
                                 &valid)) {
                    // Can decode now, if valid
                    if (!valid) {
                        PRINTF("RLP pre-decode error\n");
                        return USTREAM_FAULT;
                    }
                    canDecode = true;
                    break;
                }
                // Cannot decode yet
                // Sanity check
                if (context->rlpBufferPos == sizeof(context->rlpBuffer)) {
                    PRINTF("RLP pre-decode logic error\n");
                    return USTREAM_FAULT;
                }
            }
            if (!canDecode) {
                return USTREAM_PROCESSING;
            }
            // Ready to process this field
            if (!rlpDecodeLength(context->rlpBuffer, context->rlpBufferPos,
                                 &context->currentFieldLength, &offset,
                                 &context->currentFieldIsList)) {
                PRINTF("RLP decode error\n");
                return USTREAM_FAULT;
            }
            if (offset == 0) {
                // Hack for single byte, self encoded
                context->workBuffer--;
                context->commandLength++;
                context->fieldSingleByte = true;
            } else {
                context->fieldSingleByte = false;
            }
            context->currentFieldPos = 0;
            context->rlpBufferPos = 0;
            context->processingField = true;
        }
        if (context->customProcessor != NULL) {
            customStatus = context->customProcessor(context);
            switch(customStatus) {
                case CUSTOM_NOT_HANDLED:
                case CUSTOM_HANDLED:
                    break;
                case CUSTOM_SUSPENDED:
                    return USTREAM_SUSPENDED;
                case CUSTOM_FAULT:
                    PRINTF("Custom processor aborted\n");
                    return USTREAM_FAULT;
                default:
                    PRINTF("Unhandled custom processor status\n");
                    return USTREAM_FAULT;
            }
        }
        if (customStatus == CUSTOM_NOT_HANDLED) {
            switch (context->currentField) {
            case TX_RLP_CONTENT:
                if (processContent(context)) {
                    return USTREAM_FAULT;
                }
                context->currentField++;
                break;
            case TX_RLP_TYPE:
                if (processType(context)) {
                    return USTREAM_FAULT;
                }
                break;
            case TX_RLP_NONCE:
                if (processNonce(context)) {
                    return USTREAM_FAULT;
                }
                break;
            case TX_RLP_GASPRICE:
                if (processGasprice(context)) {
                    return USTREAM_FAULT;
                }
                break;
            case TX_RLP_STARTGAS:
                if (processStartGas(context)) {
                    return USTREAM_FAULT;
                }
                break;
            case TX_RLP_VALUE:
                if (processValue(context)) {
                    return USTREAM_FAULT;
                }
                break;
            case TX_RLP_TO:
                if (processTo(context)) {
                    return USTREAM_FAULT;
                }
                break;
            case TX_RLP_FEECURRENCY:
                //if this is an Ethereum transaction, skip the Celo fields
                if (context->isEthereum) {
                    context->currentField+=3;
                    if (processTo(context)) {
                        return USTREAM_FAULT;
                    }
                } else {
                    if (processFeeCurrency(context)) {
                        return USTREAM_FAULT;
                    }
                }
                break;
            case TX_RLP_GATEWAYTO:
                if (processGatewayTo(context)) {
                    return USTREAM_FAULT;
                }
                break;
            case TX_RLP_GATEWAYFEE:
                if (processGatewayFee(context)) {
                    return USTREAM_FAULT;
                }
                break;
            case TX_RLP_DATA:
            case TX_RLP_R:
            case TX_RLP_S:
                if (processData(context)) {
                    return USTREAM_FAULT;
                }
                break;
            case TX_RLP_V:
                if (processV(context)) {
                    return USTREAM_FAULT;
                }
                break;
            default:
                PRINTF("Invalid RLP decoder context\n");
                return USTREAM_FAULT;
            }
        }
    }
}

parserStatus_e processTx(txContext_t *context, const uint8_t *buffer, size_t length) {
    context->workBuffer = buffer;
    context->commandLength = length;
    return processTxInternal(context);
}

parserStatus_e continueTx(txContext_t *context) {
    return processTxInternal(context);
}

void initTx(txContext_t *context, cx_sha3_t *sha3, txContent_t *content,
            ustreamProcess_t customProcessor, bool isEthereum, void *extra) {
    memset(context, 0, sizeof(txContext_t));
    context->sha3 = sha3;
    context->content = content;
    context->customProcessor = customProcessor;
    context->isEthereum = isEthereum;
    context->extra = extra;
    context->currentField = TX_RLP_CONTENT;
#ifndef TESTING
    cx_keccak_init(context->sha3, 256);
#endif
}
