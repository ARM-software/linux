/* SPDX-License-Identifier: GPL-2.0 */
/*
 * (C) COPYRIGHT 2019 ARM Limited. All rights reserved.
 * Author: James.Qian.Wang <james.qian.wang@arm.com>
 *
 */
#ifndef _KOMEDA_COEFFS_H_
#define _KOMEDA_COEFFS_H_

#include <linux/refcount.h>

/* Komeda display HWs have kinds of coefficient tables for various purposes,
 * like gamma/degamma. ususally these tables are shared by multiple HW component
 * and limited number.
 * The komeda_coeffs_table/manager are imported for describing and managing
 * these tables for table reuse and racing.
 */
struct komeda_coeffs_table {
	struct komeda_coeffs_manager *mgr;
	refcount_t refcount;
	bool needs_update;
	u32 hw_id;
	void *coeffs;
	u32 __iomem *reg;
	void (*update)(struct komeda_coeffs_table *table);
};

struct komeda_coeffs_manager {
	struct mutex mutex; /* for tables accessing */
	u32 n_tables;
	u32 coeffs_sz;
	struct komeda_coeffs_table *tables[8];
};

static inline struct komeda_coeffs_table *
komeda_coeffs_get(struct komeda_coeffs_table *table)
{
	if (table)
		refcount_inc(&table->refcount);

	return table;
}

static inline void __komeda_coeffs_put(struct komeda_coeffs_table *table)
{
	if (table)
		refcount_dec(&table->refcount);
}

#define komeda_coeffs_put(table) \
do { \
	__komeda_coeffs_put(table); \
	(table) = NULL; \
} while (0)

static inline void komeda_coeffs_update(struct komeda_coeffs_table *table)
{
	if (!table || !table->needs_update)
		return;

	table->update(table);
	table->needs_update = false;
}

struct komeda_coeffs_manager *komeda_coeffs_create_manager(u32 coeffs_sz);
void komeda_coeffs_destroy_manager(struct komeda_coeffs_manager *mgr);

int komeda_coeffs_add(struct komeda_coeffs_manager *mgr,
		      u32 hw_id, u32 __iomem *reg,
		      void (*update)(struct komeda_coeffs_table *table));
struct komeda_coeffs_table *
komeda_coeffs_request(struct komeda_coeffs_manager *mgr, void *coeffs);

#endif /*_KOMEDA_COEFFS_H_*/
