/** @file
 * @brief Advance Audio Distribution Profile header.
 */

/*
 * Copyright (c) 2015-2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef ZEPHYR_INCLUDE_BLUETOOTH_A2DP_H_
#define ZEPHYR_INCLUDE_BLUETOOTH_A2DP_H_

#include <acts_bluetooth/a2dp-codec.h>
#include <acts_bluetooth/avdtp.h>

#ifdef __cplusplus
extern "C" {
#endif


/** @brief Stream End Point */
struct bt_a2dp_endpoint {
	/** Stream End Point Information */
	struct bt_avdtp_seid_lsep info;
};

enum {
	BT_A2DP_MEDIA_STATE_OPEN	= 0x06,
	BT_A2DP_MEDIA_STATE_START	= 0x07,
	BT_A2DP_MEDIA_STATE_CLOSE	= 0x08,
	BT_A2DP_MEDIA_STATE_SUSPEND	= 0x09,
	BT_A2DP_MEDIA_STATE_PENDING_AHEAD_START = 0x80,
};

struct bt_a2dp_app_cb {
	void (*connected)(struct bt_conn *conn,u8_t session_type);
	void (*disconnected)(struct bt_conn *conn,u8_t session_type);
	void (*media_handler)(struct bt_conn *conn, uint8_t *data, uint16_t len);

	/* Return 0: accepte state request, other: reject state request */
	int (*media_state_req)(struct bt_conn *conn, uint8_t state);
	void (*seted_codec)(struct bt_conn *conn, struct bt_a2dp_media_codec *codec, uint8_t cp_type);
	void (*exception_handler)(struct bt_conn *conn, u8_t err);
};

/** @brief A2DP Connect.
 *
 *  This function is to be called after the conn parameter is obtained by
 *  performing a GAP procedure. The API is to be used to establish A2DP
 *  connection between devices.
 *
 *  @param conn Pointer to bt_conn structure.
 *  @param role a2dp as source or sink role
 *
 *  @return 0 in case of success and error code in case of error.
 *  of error.
 */
int bt_a2dp_connect(struct bt_conn *conn, uint8_t role);

/* Disconnect a2dp session */
int bt_a2dp_disconnect(struct bt_conn *conn);

/** @brief Endpoint Registration.
 *
 *  This function is used for registering the stream end points. The user has
 *  to take care of allocating the memory, the preset pointer and then pass the
 *  required arguments. Also, only one sep can be registered at a time.
 *
 *  @param endpoint Pointer to bt_a2dp_endpoint structure.
 *  @param media_type Media type that the Endpoint is.
 *  @param role Role of Endpoint.
 *
 *  @return 0 in case of success and error code in case of error.
 */
int bt_a2dp_register_endpoint(struct bt_a2dp_endpoint *endpoint,
			      uint8_t media_type, uint8_t role);

/** @brief halt/resume registed endpoint.
 *
 *  This function is used for halt/resume registed endpoint
 *
 *  @param endpoint Pointer to bt_a2dp_endpoint structure.
 *  @param halt true: halt , false: resume;
 *
 *  @return 0 in case of success and error code in case of error.
 */
int bt_a2dp_halt_endpoint(struct bt_a2dp_endpoint *endpoint, bool halt);

/* Register app callback */
int bt_a2dp_register_cb(struct bt_a2dp_app_cb *cb);

/* Start a2dp play */
int bt_a2dp_start(struct bt_conn *conn);

/* Suspend a2dp play */
int bt_a2dp_suspend(struct bt_conn *conn);

/* Reconfig a2dp codec config */
int bt_a2dp_reconfig(struct bt_conn *conn, struct bt_a2dp_media_codec *codec);

/* Send delay report to source */
int bt_a2dp_send_delay_report(struct bt_conn *conn, uint16_t delay_time);

/* Send a2dp audio data */
int bt_a2dp_send_audio_data(struct bt_conn *conn, uint8_t *data, uint16_t len);

/* Get a2dp seted codec */
struct bt_a2dp_media_codec *bt_a2dp_get_seted_codec(struct bt_conn *conn);

/* Get a2dp role(source or sink) */
uint8_t bt_a2dp_get_a2dp_role(struct bt_conn *conn);

/* Get a2dp media rx channel ID */
uint16_t bt_a2dp_get_media_rx_cid(struct bt_conn *conn);

/* A2dp pts interface */
int bt_a2dp_discover(struct bt_conn *conn);
int bt_a2dp_get_capabilities(struct bt_conn *conn);
int bt_a2dp_get_all_capabilities(struct bt_conn *conn);
int bt_a2dp_set_configuration(struct bt_conn *conn);
int bt_a2dp_open(struct bt_conn *conn);
int bt_a2dp_close(struct bt_conn *conn);
int bt_a2dp_abort(struct bt_conn *conn);
int bt_a2dp_disconnect_media_session(struct bt_conn *conn);

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_INCLUDE_BLUETOOTH_A2DP_H_ */
