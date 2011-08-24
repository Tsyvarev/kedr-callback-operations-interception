/*
 * Combine differend KEDR coi components into one module.
 */

/* ========================================================================
 * Copyright (C) 2011, Andrey V. Tsyvarev  <tsyvarev@ispras.ru>
 *
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 ======================================================================== */

#include <kedr-coi/operations_interception.h>

#include "kedr_coi_instrumentor_internal.h"

#include <linux/version.h>
#include <linux/module.h>
#include <linux/init.h>

MODULE_AUTHOR("Tsyvarev Andrey");
MODULE_LICENSE("GPL");

static int __init
kedr_coi_module_init(void)
{
    return kedr_coi_instrumentors_init();
}

static void __exit
kedr_coi_module_exit(void)
{
    kedr_coi_instrumentors_destroy();
}

module_init(kedr_coi_module_init);
module_exit(kedr_coi_module_exit);

// The only functions exported from the KEDR COI module.

// 'Normal' instrumentors
EXPORT_SYMBOL(kedr_coi_payload_register);
EXPORT_SYMBOL(kedr_coi_payload_unregister);

EXPORT_SYMBOL(kedr_coi_interceptor_start);
EXPORT_SYMBOL(kedr_coi_interceptor_stop);

EXPORT_SYMBOL(kedr_coi_interceptor_watch);
EXPORT_SYMBOL(kedr_coi_interceptor_forget);
EXPORT_SYMBOL(kedr_coi_interceptor_forget_norestore);

EXPORT_SYMBOL(kedr_coi_interceptor_create);
EXPORT_SYMBOL(kedr_coi_interceptor_create_direct);

EXPORT_SYMBOL(kedr_coi_interceptor_get_intermediate_info);

EXPORT_SYMBOL(kedr_coi_interceptor_destroy);

// Foreign instrumentor
EXPORT_SYMBOL(kedr_coi_foreign_interceptor_watch);
EXPORT_SYMBOL(kedr_coi_foreign_interceptor_forget);
EXPORT_SYMBOL(kedr_coi_foreign_interceptor_forget_norestore);

EXPORT_SYMBOL(kedr_coi_foreign_interceptor_create);

EXPORT_SYMBOL(kedr_coi_bind_prototype_with_object);

EXPORT_SYMBOL(kedr_coi_foreign_interceptor_destroy);