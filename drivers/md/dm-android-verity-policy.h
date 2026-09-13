/* SPDX-License-Identifier: GPL-2.0 */
#ifndef DM_ANDROID_VERITY_POLICY_H
#define DM_ANDROID_VERITY_POLICY_H

static inline int dm_android_verity_allow_linear_without_key(
	int is_engineering_build, int is_bootloader_unlocked)
{
	return is_engineering_build || is_bootloader_unlocked;
}

#endif /* DM_ANDROID_VERITY_POLICY_H */
