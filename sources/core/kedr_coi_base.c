/*
 * Implementation of common interceptor's operations.
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
#include "kedr_coi_base_internal.h"

enum interceptor_state
{
	interceptor_state_uninitialized = 0,
	interceptor_state_initialized,
	interceptor_state_started,
};

void
interceptor_common_init(
    struct kedr_coi_interceptor* interceptor,
    const char* name,
    const struct kedr_coi_interceptor_operations* ops)
{
    interceptor->name = name;
    interceptor->ops = ops;
    
    interceptor->state = interceptor_state_initialized;
}

int kedr_coi_interceptor_start(struct kedr_coi_interceptor* interceptor)
{
	int result;
	
	BUG_ON(interceptor->state != interceptor_state_initialized);
	
	if(interceptor->ops->start)
		result = interceptor->ops->start(interceptor);
	else
		result = 0;
	
	if(result == 0)
		interceptor->state = interceptor_state_started;
	
	return result;
}


/*
 * After call of this function payload set for interceptor become
 * flexible again.
 */
void kedr_coi_interceptor_stop(struct kedr_coi_interceptor* interceptor)
{
	BUG_ON(interceptor->state == interceptor_state_uninitialized);
	
	if(interceptor->state == interceptor_state_initialized)
		return;// Assume that start() was called but failed

	if(interceptor->ops->stop)
		interceptor->ops->stop(interceptor);
	
	interceptor->state = interceptor_state_initialized;
}

/*
 * Watch for the operations of the particular object and intercepts them.
 * 
 * Object should have type for which interceptor is created.
 * 
 * This operation somewhat change content of object's fields concerning
 * operations. If these fields are changed outside of the replacer,
 * this function should be called again.
 * 
 */
int kedr_coi_interceptor_watch(
    struct kedr_coi_interceptor* interceptor,
    void* object)
{
	if(interceptor->state == interceptor_state_initialized)
		return -EPERM;

	BUG_ON(interceptor->state != interceptor_state_started);
	
	return interceptor->ops->watch(interceptor, object);
}

/*
 * Stop to watch for the object and restore content of the
 * object's fields concerned operations.
 */
int kedr_coi_interceptor_forget(
    struct kedr_coi_interceptor* interceptor,
    void* object)
{
	if(interceptor->state == interceptor_state_initialized)
		return -EPERM;

	BUG_ON(interceptor->state != interceptor_state_started);

	return interceptor->ops->forget(interceptor, object, 0);
}

/*
 * Stop to watch for the object but do not restore content of the
 * object's operations field.
 * 
 * This function is intended to call instead of
 * kedr_coi_interceptor_forget_object()
 * when object may be already freed and access
 * to its operations field may cause memory fault.
 */
int kedr_coi_interceptor_forget_norestore(
    struct kedr_coi_interceptor* interceptor,
    void* object)
{
	if(interceptor->state == interceptor_state_initialized)
		return -EPERM;

	BUG_ON(interceptor->state != interceptor_state_started);

	return interceptor->ops->forget(interceptor, object, 1);
}

int kedr_coi_payload_register(
	struct kedr_coi_interceptor* interceptor,
	struct kedr_coi_payload* payload)
{
	// "hot" registering of payload is permitted at abstract level
	BUG_ON((interceptor->state != interceptor_state_initialized)
		&& (interceptor->state != interceptor_state_started));

	if(interceptor->ops->payload_register)
		return interceptor->ops->payload_register(interceptor, payload);
	else
		return -EPERM;//not supported
}

void kedr_coi_payload_unregister(
	struct kedr_coi_interceptor* interceptor,
	struct kedr_coi_payload* payload)
{
	BUG_ON((interceptor->state != interceptor_state_initialized)
		&& (interceptor->state != interceptor_state_started));

	if(interceptor->ops->payload_unregister)	
		interceptor->ops->payload_unregister(interceptor, payload);
}

int kedr_coi_payload_foreign_register(
	struct kedr_coi_interceptor* interceptor,
	struct kedr_coi_payload_foreign* payload)
{
	// "hot" registering of payload is permitted at abstract level
	BUG_ON((interceptor->state != interceptor_state_initialized)
		&& (interceptor->state != interceptor_state_started));

	if(interceptor->ops->payload_foreign_register)
		return interceptor->ops->payload_foreign_register(interceptor, payload);
	else
		return -EPERM;//not supported
}

void kedr_coi_payload_foreign_unregister(
	struct kedr_coi_interceptor* interceptor,
	struct kedr_coi_payload_foreign* payload)
{
	BUG_ON((interceptor->state != interceptor_state_initialized)
		&& (interceptor->state != interceptor_state_started));

	if(interceptor->ops->payload_foreign_unregister)
		interceptor->ops->payload_foreign_unregister(interceptor, payload);
}

void* kedr_coi_interceptor_get_orig_operation(
	struct kedr_coi_interceptor* interceptor,
	void* object,
	size_t operation_offset)
{
	/*
	 * This functions may be called only from intermediate operation,
	 * and intermediate operation may be called only in case of 
	 * successfull watch_for_object().
	 */
	BUG_ON(interceptor->state != interceptor_state_started);
	
	BUG_ON(interceptor->ops->get_orig_operation == 0);
	
	return interceptor->ops->get_orig_operation(interceptor,
		object, operation_offset);
}

int kedr_coi_interceptor_foreign_restore_copy(
	struct kedr_coi_interceptor* interceptor,
	void* object,
	void* foreign_object)
{
	if(interceptor->state == interceptor_state_initialized)
		return -EPERM;

	BUG_ON(interceptor->state != interceptor_state_started);

	if(interceptor->ops->foreign_restore_copy)
		return interceptor->ops->foreign_restore_copy(interceptor, object, foreign_object);
	else
		return -EPERM;//not supported
}

void kedr_coi_interceptor_destroy(
	struct kedr_coi_interceptor* interceptor)
{
	BUG_ON(interceptor->state != interceptor_state_initialized);
	
	// For selfcontrol
	interceptor->state = interceptor_state_uninitialized;
	
	interceptor->ops->destroy(interceptor);
}

