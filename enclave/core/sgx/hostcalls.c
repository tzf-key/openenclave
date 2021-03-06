// Copyright (c) Open Enclave SDK contributors.
// Licensed under the MIT License.

#include <openenclave/edger8r/enclave.h>
#include <openenclave/enclave.h>
#include <openenclave/internal/sgx/ecall_context.h>
#include "td.h"

/**
 * Validate and fetch this thread's ecall context.
 */
static oe_ecall_context_t* _get_ecall_context()
{
    td_t* td = oe_get_td();
    oe_ecall_context_t* ecall_context = td->host_ecall_context;
    return oe_is_outside_enclave(ecall_context, sizeof(*ecall_context))
               ? ecall_context
               : NULL;
}

/**
 * Fetch the ocall_args field if an ecall context has been passed in.
 */
oe_call_host_function_args_t* oe_ecall_context_get_ocall_args()
{
    oe_ecall_context_t* ecall_context = _get_ecall_context();
    return ecall_context ? &ecall_context->ocall_args : NULL;
}

/**
 * Get the ecall context's buffer if it is of an equal or larger size than the
 * given size.
 */
void* oe_ecall_context_get_ocall_buffer(uint64_t size)
{
    oe_ecall_context_t* ecall_context = _get_ecall_context();
    if (ecall_context)
    {
        // Copy to volatile variables to prevent TOCTOU attacks.
        uint8_t* volatile ocall_buffer = ecall_context->ocall_buffer;
        volatile uint64_t ocall_buffer_size = ecall_context->ocall_buffer_size;
        if (ocall_buffer_size >= size &&
            oe_is_outside_enclave(ocall_buffer, ocall_buffer_size))
            return (void*)ocall_buffer;
    }
    return NULL;
}

// Thread local variable to keep track of whether ecall context's buffer
// was used for making the current ocall.
static __thread bool _thread_ecall_context_buffer_used;

// Function used by oeedger8r for allocating ocall buffers.
void* oe_allocate_ocall_buffer(size_t size)
{
    // Fetch the ecall context's ocall buffer if it is equal to or larger than
    // given size. Use it if available.
    void* buffer = oe_ecall_context_get_ocall_buffer(size);
    if (buffer)
    {
        _thread_ecall_context_buffer_used = true;
        return buffer;
    }

    // Perform host allocation by making an ocall.
    _thread_ecall_context_buffer_used = false;
    return oe_host_malloc(size);
}

// Function used by oeedger8r for freeing ocall buffers.
void oe_free_ocall_buffer(void* buffer)
{
    // ecall context's buffer is managed by the host and does not have to be
    // freed.
    if (_thread_ecall_context_buffer_used)
        return;

    oe_host_free(buffer);
}

void* oe_allocate_arena(size_t capacity)
{
    return oe_host_malloc(capacity);
}

void oe_deallocate_arena(void* buffer)
{
    oe_host_free(buffer);
}
