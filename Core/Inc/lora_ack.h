/*
 * lora_ack.h
 *
 * LoRa ACK Framework with dependency injection
 */

#ifndef LORA_ACK_H
#define LORA_ACK_H

#include <stdint.h>
#include <stdbool.h>
#include "LoRa.h"

// ACK Framework Configuration
#define ACK_MAX_PAYLOAD_SIZE    200
#define ACK_MAX_RETRIES         3
#define ACK_TIMEOUT_MS          2000
#define ACK_HEADER_SIZE         4

// Message Types
typedef enum {
    MSG_TYPE_DATA = 0x01,
    MSG_TYPE_ACK  = 0x02,
    MSG_TYPE_NACK = 0x03
} ack_msg_type_t;

// ACK Results
typedef enum {
    ACK_RESULT_SUCCESS = 0,
    ACK_RESULT_TIMEOUT,
    ACK_RESULT_MAX_RETRIES,
    ACK_RESULT_INVALID_MSG,
    ACK_RESULT_NACK_RECEIVED,
    ACK_RESULT_ERROR
} ack_result_t;

// Message structure
typedef struct {
    uint8_t msg_type;       // MSG_TYPE_DATA, MSG_TYPE_ACK, MSG_TYPE_NACK
    uint8_t msg_id;         // Message ID for tracking
    uint8_t payload_len;    // Length of actual payload
    uint8_t checksum;       // Simple checksum
    uint8_t payload[ACK_MAX_PAYLOAD_SIZE];
} ack_message_t;

// Forward declarations
typedef struct ack_framework ack_framework_t;

// Function pointer types for dependency injection
typedef void (*ack_delay_fn_t)(uint32_t ms);
typedef uint32_t (*ack_get_time_fn_t)(void);
typedef void (*ack_on_data_received_fn_t)(const uint8_t* data, uint8_t len, int rssi);
typedef void (*ack_on_ack_received_fn_t)(uint8_t msg_id, int rssi);
typedef void (*ack_on_timeout_fn_t)(uint8_t msg_id, uint8_t retry_count);

// ACK Framework structure with dependency injection
struct ack_framework {
    // LoRa hardware interface
    LoRa* lora;

    // Dependency injected functions
    ack_delay_fn_t          delay_fn;
    ack_get_time_fn_t       get_time_fn;
    ack_on_data_received_fn_t on_data_received;
    ack_on_ack_received_fn_t  on_ack_received;
    ack_on_timeout_fn_t       on_timeout;

    // State management
    uint8_t next_msg_id;
    uint8_t waiting_for_ack_id;
    bool is_waiting_for_ack;
    uint32_t ack_timeout_start;
    uint8_t retry_count;

    // Configuration
    uint32_t ack_timeout_ms;
    uint8_t max_retries;

    // Statistics (optional)
    struct {
        uint32_t messages_sent;
        uint32_t acks_received;
        uint32_t timeouts;
        uint32_t retries;
    } stats;
};

// Core ACK Framework Functions
ack_framework_t* ack_create(LoRa* lora);
void ack_init(ack_framework_t* ack,
              ack_delay_fn_t delay_fn,
              ack_get_time_fn_t get_time_fn);

void ack_set_callbacks(ack_framework_t* ack,
                       ack_on_data_received_fn_t on_data_received,
                       ack_on_ack_received_fn_t on_ack_received,
                       ack_on_timeout_fn_t on_timeout);

void ack_set_config(ack_framework_t* ack, uint32_t timeout_ms, uint8_t max_retries);

// Main API functions
ack_result_t ack_send_with_retry(ack_framework_t* ack, const uint8_t* data, uint8_t len);
void ack_process_received_message(ack_framework_t* ack, const uint8_t* raw_data, uint8_t raw_len, int rssi);
void ack_process_timeout_check(ack_framework_t* ack);

// Utility functions
void ack_start_receiving(ack_framework_t* ack);
bool ack_is_waiting_for_ack(const ack_framework_t* ack);
uint8_t ack_calculate_checksum(const uint8_t* data, uint8_t len);

#endif // LORA_ACK_H
