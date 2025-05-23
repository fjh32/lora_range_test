/*
 * lora_ack.c
 *
 * Implementation of LoRa ACK Framework
 */

#include "lora_ack.h"
#include <string.h>
#include <stdlib.h>

// Create ACK framework instance
ack_framework_t* ack_create(LoRa* lora) {
    ack_framework_t* ack = malloc(sizeof(ack_framework_t));
    if (!ack) return NULL;

    memset(ack, 0, sizeof(ack_framework_t));
    ack->lora = lora;
    ack->next_msg_id = 1;
    ack->ack_timeout_ms = ACK_TIMEOUT_MS;
    ack->max_retries = ACK_MAX_RETRIES;

    return ack;
}

// Initialize with required dependencies
void ack_init(ack_framework_t* ack,
              ack_delay_fn_t delay_fn,
              ack_get_time_fn_t get_time_fn) {
    ack->delay_fn = delay_fn;
    ack->get_time_fn = get_time_fn;
}

// Set callback functions
void ack_set_callbacks(ack_framework_t* ack,
                       ack_on_data_received_fn_t on_data_received,
                       ack_on_ack_received_fn_t on_ack_received,
                       ack_on_timeout_fn_t on_timeout) {
    ack->on_data_received = on_data_received;
    ack->on_ack_received = on_ack_received;
    ack->on_timeout = on_timeout;
}

// Configure timeouts and retries
void ack_set_config(ack_framework_t* ack, uint32_t timeout_ms, uint8_t max_retries) {
    ack->ack_timeout_ms = timeout_ms;
    ack->max_retries = max_retries;
}

// Calculate simple checksum
uint8_t ack_calculate_checksum(const uint8_t* data, uint8_t len) {
    uint8_t checksum = 0;
    for (uint8_t i = 0; i < len; i++) {
        checksum ^= data[i];
    }
    return checksum;
}

// Send ACK/NACK message
static ack_result_t ack_send_response(ack_framework_t* ack, uint8_t msg_id, ack_msg_type_t type) {
    ack_message_t response = {0};
    response.msg_type = type;
    response.msg_id = msg_id;
    response.payload_len = 0;
    response.checksum = ack_calculate_checksum((uint8_t*)&response, ACK_HEADER_SIZE - 1);

    // Switch to transmit mode
    LoRa_gotoMode(ack->lora, STNBY_MODE);

    uint8_t result = LoRa_transmit(ack->lora, (uint8_t*)&response, ACK_HEADER_SIZE, 1000);

    // Return to receive mode
    LoRa_startReceiving(ack->lora);

    return result ? ACK_RESULT_SUCCESS : ACK_RESULT_ERROR;
}

// Send data message with retry logic
ack_result_t ack_send_with_retry(ack_framework_t* ack, const uint8_t* data, uint8_t len) {
    if (len > ACK_MAX_PAYLOAD_SIZE) {
        return ACK_RESULT_ERROR;
    }

    ack_message_t msg = {0};
    msg.msg_type = MSG_TYPE_DATA;
    msg.msg_id = ack->next_msg_id++;
    msg.payload_len = len;
    memcpy(msg.payload, data, len);
    msg.checksum = ack_calculate_checksum((uint8_t*)&msg, ACK_HEADER_SIZE - 1 + len);

    ack->retry_count = 0;

    while (ack->retry_count <= ack->max_retries) {
        // Send the message
        LoRa_gotoMode(ack->lora, STNBY_MODE);
        uint8_t tx_result = LoRa_transmit(ack->lora, (uint8_t*)&msg, ACK_HEADER_SIZE + len, 1000);

        if (!tx_result) {
            return ACK_RESULT_ERROR;
        }

        ack->stats.messages_sent++;

        // Switch to receive mode and wait for ACK
        LoRa_startReceiving(ack->lora);
        ack->is_waiting_for_ack = true;
        ack->waiting_for_ack_id = msg.msg_id;
        ack->ack_timeout_start = ack->get_time_fn();

        // Wait for ACK with timeout
        while (ack->is_waiting_for_ack) {
            uint32_t current_time = ack->get_time_fn();
            if ((current_time - ack->ack_timeout_start) >= ack->ack_timeout_ms) {
                // Timeout occurred
                ack->is_waiting_for_ack = false;
                ack->stats.timeouts++;

                if (ack->on_timeout) {
                    ack->on_timeout(msg.msg_id, ack->retry_count);
                }

                if (ack->retry_count >= ack->max_retries) {
                    return ACK_RESULT_MAX_RETRIES;
                }

                ack->retry_count++;
                ack->stats.retries++;
                break; // Break to retry
            }

            // Small delay to prevent busy waiting
            if (ack->delay_fn) {
                ack->delay_fn(10);
            }
        }

        // If we're no longer waiting, ACK was received
        if (!ack->is_waiting_for_ack && ack->retry_count <= ack->max_retries) {
            return ACK_RESULT_SUCCESS;
        }
    }

    return ACK_RESULT_MAX_RETRIES;
}

// Process received messages
void ack_process_received_message(ack_framework_t* ack, const uint8_t* raw_data, uint8_t raw_len, int rssi) {
    if (raw_len < ACK_HEADER_SIZE) {
        return; // Invalid message
    }

    ack_message_t* msg = (ack_message_t*)raw_data;

    // Verify checksum
    uint8_t expected_checksum = ack_calculate_checksum(raw_data, raw_len - 1);
    if (msg->checksum != expected_checksum) {
        return; // Checksum mismatch
    }

    switch (msg->msg_type) {
        case MSG_TYPE_DATA:
            // Received data message - send ACK and notify application
            ack_send_response(ack, msg->msg_id, MSG_TYPE_ACK);

            if (ack->on_data_received && msg->payload_len > 0) {
                ack->on_data_received(msg->payload, msg->payload_len, rssi);
            }
            break;

        case MSG_TYPE_ACK:
            // Received ACK - check if it's for our pending message
            if (ack->is_waiting_for_ack && msg->msg_id == ack->waiting_for_ack_id) {
                ack->is_waiting_for_ack = false;
                ack->stats.acks_received++;

                if (ack->on_ack_received) {
                    ack->on_ack_received(msg->msg_id, rssi);
                }
            }
            break;

        case MSG_TYPE_NACK:
            // Received NACK - treat as timeout for retry logic
            if (ack->is_waiting_for_ack && msg->msg_id == ack->waiting_for_ack_id) {
                ack->is_waiting_for_ack = false;
                // Will trigger retry in send_with_retry
            }
            break;

        default:
            // Unknown message type
            break;
    }
}

// Check for ACK timeouts (call this periodically)
void ack_process_timeout_check(ack_framework_t* ack) {
    if (!ack->is_waiting_for_ack) {
        return;
    }

    uint32_t current_time = ack->get_time_fn();
    if ((current_time - ack->ack_timeout_start) >= ack->ack_timeout_ms) {
        ack->is_waiting_for_ack = false;
        ack->stats.timeouts++;

        if (ack->on_timeout) {
            ack->on_timeout(ack->waiting_for_ack_id, ack->retry_count);
        }
    }
}

// Start receiving mode
void ack_start_receiving(ack_framework_t* ack) {
    LoRa_startReceiving(ack->lora);
}

// Check if waiting for ACK
bool ack_is_waiting_for_ack(const ack_framework_t* ack) {
    return ack->is_waiting_for_ack;
}
