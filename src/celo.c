#include "celo.h"
#include "ethUtils.h"
#include "globals.h"
#include "os.h"
#include "utils.h"

// TODO: remove this. This module should be independant from ui
#include "ui_flow.h"

#include <string.h>

static const uint8_t TOKEN_TRANSFER_ID[] = { 0xa9, 0x05, 0x9c, 0xbb };

void io_seproxyhal_send_status(uint32_t sw) {
    G_io_apdu_buffer[0] = ((sw >> 8) & 0xff);
    G_io_apdu_buffer[1] = (sw & 0xff);
    io_exchange(CHANNEL_APDU | IO_RETURN_AFTER_TX, 2);
}

void format_signature_out(const uint8_t* signature) {
  memset(G_io_apdu_buffer + 1, 0x00, 64);
  uint8_t offset = 1;
  uint8_t xoffset = 4; //point to r value
  //copy r
  uint8_t xlength = signature[xoffset-1];
  if (xlength == 33) {
    xlength = 32;
    xoffset ++;
  }
  memmove(G_io_apdu_buffer+offset+32-xlength,  signature+xoffset, xlength);
  offset += 32;
  xoffset += xlength +2; //move over rvalue and TagLEn
  //copy s value
  xlength = signature[xoffset-1];
  if (xlength == 33) {
    xlength = 32;
    xoffset ++;
  }
  memmove(G_io_apdu_buffer+offset+32-xlength, signature+xoffset, xlength);
}

uint32_t set_result_get_publicKey() {
    uint32_t tx = 0;
    G_io_apdu_buffer[tx++] = 65;
    memcpy(G_io_apdu_buffer + tx, tmpCtx.publicKeyContext.publicKey.W, 65);
    tx += 65;
    G_io_apdu_buffer[tx++] = 40;
    memcpy(G_io_apdu_buffer + tx, tmpCtx.publicKeyContext.address, 40);
    tx += 40;
    if (tmpCtx.publicKeyContext.getChaincode) {
      memcpy(G_io_apdu_buffer + tx, tmpCtx.publicKeyContext.chainCode, 32);
      tx += 32;
    }
    return tx;
}

volatile uint8_t appState;

void reset_app_context() {
  appState = APP_STATE_IDLE;
  PRINTF("Resetting context\n");
  memset(tmpCtx.transactionContext.tokenSet, 0, MAX_TOKEN);
  memset(&txContext, 0, sizeof(txContext));
  memset(&tmpContent, 0, sizeof(tmpContent));
}

#include "uint256.h"

#define WEI_TO_ETHER 18

tokenDefinition_t* getKnownToken(uint8_t *tokenAddr) {
    tokenDefinition_t *currentToken = NULL;

    for(int i=0; i < MAX_TOKEN; i++) {
      currentToken = &tmpCtx.transactionContext.tokens[i];
      if (tmpCtx.transactionContext.tokenSet[i] && (memcmp(currentToken->address, tokenAddr, 20) == 0)) {
        PRINTF("Token found at index %d\n", i);
        return currentToken;
      }
    }

    return NULL;
}

static uint32_t splitBinaryParameterPart(char *result, uint8_t *parameter) {
    uint32_t i;
    for (i=0; i<8; i++) {
        if (parameter[i] != 0x00) {
            break;
        }
    }
    if (i == 8) {
        result[0] = '0';
        result[1] = '0';
        result[2] = '\0';
        return 2;
    }
    else {
        array_hexstr(result, parameter + i, 8 - i);
        return ((8 - i) * 2);
    }
}

customStatus_e customProcessor(txContext_t *context) {
    if ((context->currentField == TX_RLP_DATA) &&
        (context->currentFieldLength != 0)) {
        dataPresent = true;
        // If handling a new contract rather than a function call, abort immediately
        if (tmpContent.txContent.destinationLength == 0) {
            return CUSTOM_NOT_HANDLED;
        }
        if (context->currentFieldPos == 0) {
            // If handling the beginning of the data field, assume that the function selector is present
            if (context->commandLength < 4) {
                PRINTF("Missing function selector\n");
                return CUSTOM_FAULT;
            }
            // Initial check to see if the token content can be processed
            tokenProvisioned =
                (context->currentFieldLength == sizeof(dataContext.tokenContext.data)) &&
                (memcmp(context->workBuffer, TOKEN_TRANSFER_ID, 4) == 0) &&
                (getKnownToken(tmpContent.txContent.destination) != NULL);
        }
        if (tokenProvisioned) {
            if (context->currentFieldPos < context->currentFieldLength) {
                uint32_t copySize = (context->commandLength <
                                        ((context->currentFieldLength -
                                                   context->currentFieldPos))
                                        ? context->commandLength
                                            : context->currentFieldLength -
                                                   context->currentFieldPos);
                copyTxData(context,
                    dataContext.tokenContext.data + context->currentFieldPos,
                    copySize);
            }
            if (context->currentFieldPos == context->currentFieldLength) {
                context->currentField++;
                context->processingField = false;
            }
            return CUSTOM_HANDLED;
        }
        else {
            uint32_t blockSize;
            uint32_t copySize;
            uint32_t fieldPos = context->currentFieldPos;
            if (fieldPos == 0) {
                if (!N_storage.dataAllowed) {
                  PRINTF("Data field forbidden\n");
                  return CUSTOM_FAULT;
                }
                if (!N_storage.contractDetails) {
                  return CUSTOM_NOT_HANDLED;
                }
                dataContext.rawDataContext.fieldIndex = 0;
                dataContext.rawDataContext.fieldOffset = 0;
                blockSize = 4;
            }
            else {
                if (!N_storage.contractDetails) {
                  return CUSTOM_NOT_HANDLED;
                }
                blockSize = 32 - (dataContext.rawDataContext.fieldOffset % 32);
            }

            // Sanity check
            if ((context->currentFieldLength - fieldPos) < blockSize) {
                PRINTF("Unconsistent data\n");
                return CUSTOM_FAULT;
            }

            copySize = (context->commandLength < blockSize ? context->commandLength : blockSize);
            copyTxData(context,
                        dataContext.rawDataContext.data + dataContext.rawDataContext.fieldOffset,
                        copySize);

            if (context->currentFieldPos == context->currentFieldLength) {
                context->currentField++;
                context->processingField = false;
            }

            dataContext.rawDataContext.fieldOffset += copySize;

            if (copySize == blockSize) {
                // Can display
                if (fieldPos != 0) {
                    dataContext.rawDataContext.fieldIndex++;
                }
                dataContext.rawDataContext.fieldOffset = 0;
                if (fieldPos == 0) {
                    array_hexstr(strings.tmp.tmp, dataContext.rawDataContext.data, 4);
                    ux_flow_init(0, ux_confirm_selector_flow, NULL);
                }
                else {
                    uint32_t offset = 0;
                    uint32_t i;
                    snprintf(strings.tmp.tmp2, sizeof(strings.tmp.tmp2), "Field %d", dataContext.rawDataContext.fieldIndex);
                    for (i=0; i<4; i++) {
                        offset += splitBinaryParameterPart(strings.tmp.tmp + offset, dataContext.rawDataContext.data + 8 * i);
                        if (i != 3) {
                            strings.tmp.tmp[offset++] = ':';
                        }
                    }
                    ux_flow_init(0, ux_confirm_parameter_flow, NULL);
                }
            }
            else {
                return CUSTOM_HANDLED;
            }

            return CUSTOM_SUSPENDED;
        }
    }
    return CUSTOM_NOT_HANDLED;
}

void finalizeParsing(bool direct) {
  uint256_t gasPrice, startGas, uint256;
  uint32_t i;
  uint8_t decimals = WEI_TO_ETHER;
  uint8_t feeDecimals = WEI_TO_ETHER;
  char *ticker = CHAINID_COINNAME " ";
  char *feeTicker = CHAINID_COINNAME " ";
  uint8_t tickerOffset = 0;

  // Display correct currency if fee currency field sent
  if (tmpContent.txContent.feeCurrencyLength != 0) {
    tokenDefinition_t *feeCurrencyToken = getKnownToken(tmpContent.txContent.feeCurrency);
    if (feeCurrencyToken == NULL) {
      reset_app_context();
      PRINTF("Invalid fee currency");
      if (direct) {
          THROW(0x6A80);
      }
      else {
          io_seproxyhal_send_status(0x6A80);
          ui_idle();
          return;
      }
    } else {
      feeTicker = feeCurrencyToken->ticker;
      feeDecimals = feeCurrencyToken->decimals;
    }
  }

  // Store the hash
  cx_hash((cx_hash_t *)&sha3, CX_LAST, tmpCtx.transactionContext.hash, 0, tmpCtx.transactionContext.hash, 32);
    // If there is a token to process, check if it is well known
    if (tokenProvisioned) {
        tokenDefinition_t *currentToken = getKnownToken(tmpContent.txContent.destination);
        if (currentToken != NULL) {
            dataPresent = false;
            decimals = currentToken->decimals;
            ticker = currentToken->ticker;
            tmpContent.txContent.destinationLength = 20;
            memcpy(tmpContent.txContent.destination, dataContext.tokenContext.data + 4 + 12, 20);
            memcpy(tmpContent.txContent.value.value, dataContext.tokenContext.data + 4 + 32, 32);
            tmpContent.txContent.value.length = 32;
        }
    }
    else {
      if (dataPresent && !N_storage.dataAllowed) {
          reset_app_context();
          PRINTF("Data field forbidden\n");
          if (direct) {
            THROW(0x6A80);
          }
          else {
            io_seproxyhal_send_status(0x6A80);
            ui_idle();
            return;
          }
      }
    }
  // Add address
  if (tmpContent.txContent.destinationLength != 0) {
    char address[41];
    getEthAddressStringFromBinary(tmpContent.txContent.destination, address, CHAIN_ID, &sha3);
    strings.common.fullAddress[0] = '0';
    strings.common.fullAddress[1] = 'x';
    memcpy(strings.common.fullAddress+2, address, 40);
    strings.common.fullAddress[42] = '\0';
  }
  else
  {
    strcpy(strings.common.fullAddress, "New Contract");
  }
  // Add gateway fee recipient address
  if (tmpContent.txContent.gatewayDestinationLength != 0) {
    char gatewayAddress[41];
    getEthAddressStringFromBinary(tmpContent.txContent.gatewayDestination, gatewayAddress, CHAIN_ID, &sha3);
    strings.common.fullGatewayAddress[0] = '0';
    strings.common.fullGatewayAddress[1] = 'x';
    memcpy(strings.common.fullGatewayAddress+2, gatewayAddress, 40);
    strings.common.fullGatewayAddress[42] = '\0';
  }
  // Add amount in ethers or tokens
  convertUint256BE(tmpContent.txContent.value.value, tmpContent.txContent.value.length, &uint256);
  tostring256(&uint256, 10, (char *)(G_io_apdu_buffer + 100), 100);
  i = 0;
  while (G_io_apdu_buffer[100 + i]) {
    i++;
  }
  adjustDecimals((char *)(G_io_apdu_buffer + 100), i, (char *)G_io_apdu_buffer, 100, decimals);
  i = 0;
    tickerOffset = 0;
    while (ticker[tickerOffset]) {
        strings.common.fullAmount[tickerOffset] = ticker[tickerOffset];
        tickerOffset++;
    }
    while (G_io_apdu_buffer[i]) {
        strings.common.fullAmount[tickerOffset + i] = G_io_apdu_buffer[i];
        i++;
    }
  strings.common.fullAmount[tickerOffset + i] = '\0';
  // Add gateway fee
  convertUint256BE(tmpContent.txContent.gatewayFee.value, tmpContent.txContent.gatewayFee.length, &uint256);
  tostring256(&uint256, 10, (char *)(G_io_apdu_buffer + 100), 100);
  i = 0;
  while (G_io_apdu_buffer[100 + i]) {
    i++;
  }
  adjustDecimals((char *)(G_io_apdu_buffer + 100), i, (char *)G_io_apdu_buffer, 100, feeDecimals);
  i = 0;
    tickerOffset = 0;
    while (feeTicker[tickerOffset]) {
        strings.common.gatewayFee[tickerOffset] = feeTicker[tickerOffset];
        tickerOffset++;
    }
    while (G_io_apdu_buffer[i]) {
        strings.common.gatewayFee[tickerOffset + i] = G_io_apdu_buffer[i];
        i++;
    }
  strings.common.gatewayFee[tickerOffset + i] = '\0';
  // Compute maximum fee
  convertUint256BE(tmpContent.txContent.gasprice.value, tmpContent.txContent.gasprice.length, &gasPrice);
  convertUint256BE(tmpContent.txContent.startgas.value, tmpContent.txContent.startgas.length, &startGas);
  mul256(&gasPrice, &startGas, &uint256);
  tostring256(&uint256, 10, (char *)(G_io_apdu_buffer + 100), 100);
  i = 0;
  while (G_io_apdu_buffer[100 + i]) {
    i++;
  }
  adjustDecimals((char *)(G_io_apdu_buffer + 100), i, (char *)G_io_apdu_buffer, 100, feeDecimals);
  i = 0;
  tickerOffset=0;
  while (feeTicker[tickerOffset]) {
      strings.common.maxFee[tickerOffset] = feeTicker[tickerOffset];
      tickerOffset++;
  }
  while (G_io_apdu_buffer[i]) {
    strings.common.maxFee[tickerOffset + i] = G_io_apdu_buffer[i];
    i++;
  }
  strings.common.maxFee[tickerOffset + i] = '\0';

#ifdef NO_CONSENT
  io_seproxyhal_touch_tx_ok(NULL);
#else // NO_CONSENT
  if (tmpContent.txContent.gatewayDestinationLength != 0) {
    ux_flow_init(0,
      ((dataPresent && !N_storage.contractDetails) ? ux_approval_celo_data_warning_gateway_tx_flow : ux_approval_celo_gateway_tx_flow),
      NULL);
  } else {
    ux_flow_init(0,
      ((dataPresent && !N_storage.contractDetails) ? ux_approval_celo_data_warning_tx_flow : ux_approval_celo_tx_flow),
      NULL);
  }
#endif // NO_CONSENT
}
