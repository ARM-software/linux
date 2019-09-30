// SPDX-License-Identifier: GPL-2.0
/*
 * (C) COPYRIGHT 2019 ARM Limited. All rights reserved.
 * Author: James.Qian.Wang <james.qian.wang@arm.com>
 *
 */
#include <linux/slab.h>
#include "komeda_coeffs.h"

static inline bool is_table_in_using(struct komeda_coeffs_table *table)
{
	return refcount_read(&table->refcount) > 1;
}

/* request a coeffs table for the coeffs data specified by argument coeffs */
struct komeda_coeffs_table *
komeda_coeffs_request(struct komeda_coeffs_manager *mgr, void *coeffs)
{
	struct komeda_coeffs_table *table;
	u32 i;

	mutex_lock(&mgr->mutex);

	/* search table list to find if there is a in-using table with the
	 * same coefficient content, if find, reuse this table.
	 */
	for (i = 0; i < mgr->n_tables; i++) {
		table = mgr->tables[i];

		/* skip the unused table */
		if (!is_table_in_using(table))
			continue;

		if (!memcmp(table->coeffs, coeffs, mgr->coeffs_sz))
			goto found;
	}

	/* can not reuse the existing in-using table, loop for a new one */
	for (i = 0; i < mgr->n_tables; i++) {
		table = mgr->tables[i];

		if (!is_table_in_using(table)) {
			memcpy(table->coeffs, coeffs, mgr->coeffs_sz);
			table->needs_update = true;
			goto found;
		}
	}

	/* Since previous two search loop will directly goto found if found an
	 * available table, so once program ran here means search failed.
	 * clear the table to NULL, unlock(mgr->mutex) and return NULL.
	 */
	table = NULL;

found:
	komeda_coeffs_get(table);
	mutex_unlock(&mgr->mutex);
	return table;
}

/* Add a coeffs table to manager */
int komeda_coeffs_add(struct komeda_coeffs_manager *mgr,
		      u32 hw_id, u32 __iomem *reg,
		      void (*update)(struct komeda_coeffs_table *table))
{
	struct komeda_coeffs_table *table;

	if (mgr->n_tables >= ARRAY_SIZE(mgr->tables))
		return -ENOSPC;

	table = kzalloc(sizeof(*table), GFP_KERNEL);
	if (!table)
		return -ENOMEM;

	table->coeffs = kzalloc(mgr->coeffs_sz, GFP_KERNEL);
	if (!table->coeffs) {
		kfree(table);
		return -ENOMEM;
	}

	refcount_set(&table->refcount, 1);
	table->mgr = mgr;
	table->hw_id = hw_id;
	table->update = update;
	table->reg = reg;

	mgr->tables[mgr->n_tables++] = table;
	return 0;
}

struct komeda_coeffs_manager *komeda_coeffs_create_manager(u32 coeffs_sz)
{
	struct komeda_coeffs_manager *mgr;

	mgr = kzalloc(sizeof(*mgr), GFP_KERNEL);
	if (!mgr)
		return ERR_PTR(-ENOMEM);

	mutex_init(&mgr->mutex);
	mgr->coeffs_sz = coeffs_sz;

	return mgr;
}

void komeda_coeffs_destroy_manager(struct komeda_coeffs_manager *mgr)
{
	u32 i;

	if (!mgr)
		return;

	for (i = 0; i < mgr->n_tables; i++) {
		WARN_ON(is_table_in_using(mgr->tables[i]));
		kfree(mgr->tables[i]->coeffs);
		kfree(mgr->tables[i]);
	}

	kfree(mgr);
}
