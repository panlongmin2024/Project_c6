/*
 * Copyright (c) 2023 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief pts test.
 */
#ifndef PTS_TEST_H_
#define PTS_TEST_H_
#include <acts_bluetooth/bluetooth.h>

#ifdef __cplusplus
	extern "C" {
#endif

typedef enum
{
	PTS_TEST_FLAG_CSIP = 0x01,
} pts_test_flags_e;

uint8_t bt_is_pts_test(void);

void pts_test_set_flag(pts_test_flags_e flags);

void pts_test_clear_flag(pts_test_flags_e flags);

uint8_t pts_test_is_flag(pts_test_flags_e flag);

#ifdef __cplusplus
}
#endif

#endif /* PTS_TEST_H_ */

