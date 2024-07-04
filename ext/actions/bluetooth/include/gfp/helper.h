#ifndef _HELPER_H
#define _HELPER_H

#define AES_KEY_SIZE 16
#define AES_BLOCK_SIZE 16
#define SHA256_HASH_SIZE 32
#define ECDH_PUBLIC_KEY_SIZE 64
#define ECDH_PRIVATE_KEY_SIZE 32
#define ECDH_SHARED_KEY_SIZE 32
#define NUM_BLOOM_FILTER_INDEXES_PER_KEY 8
#define BLOOM_FILTER_INDEX_BYTE_LENGTH 4

#define PERSONALIZED_NAME_SIZE 64
#define AES_NONCEZ_SIZE 8
#define AES_COUNTER_SIZE 8
#define HMAC_SHA256_SIZE 32
#define HMAC_SHA256_HEAD8 8
#define COUNTER_SIZE 16

#include <stdint.h>
#include <bluetooth_provider.h>
#include <crypto_provider.h>
#include <fast_pair.h>
#include <storage_provider.h>

typedef struct
{
	uint8_t current_shared_key[AES_KEY_SIZE];
	uint8_t phone_public_address[MAC_ADDRESS_LENGTH];
	uint32_t remote_passkey;
	uint32_t local_passkey;
	int account_keys_length;
	uint8_t fp_started;
	uint8_t account_key_writed;
	uint8_t headset_initiates_pairing;
} __attribute__((packed)) gfp_ble_info_t;

/** Initializes Fast Pair with the implemented providers. */
FastPair* init(BluetoothProvider*, StorageProvider*, CryptoProvider*);
void fastpair_timeout_handle(os_work *work);
int additional_data_written(uint8_t* packet, int packet_length);
int account_key_written(uint8_t* packet, int packet_length);
int passkey_written(uint8_t* packet, int packet_length);
int perform_key_based_pairing(uint8_t* request, int request_length);


#endif // _HELPER_H

