/*!The Treasure Box Library
 * 
 * TBox is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 * 
 * TBox is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with TBox; 
 * If not, see <a href="http://www.gnu.org/licenses/"> http://www.gnu.org/licenses/</a>
 * 
 * Copyright (C) 2009 - 2015, ruki All rights reserved.
 *
 * @author      ruki
 * @file        pool.c
 * @ingroup     memory
 *
 */

/* //////////////////////////////////////////////////////////////////////////////////////
 * trace
 */
#define TB_TRACE_MODULE_NAME            "pool"
#define TB_TRACE_MODULE_DEBUG           (0)

/* //////////////////////////////////////////////////////////////////////////////////////
 * includes
 */
#include "pool.h"
#include "allocator.h"
#include "large_pool.h"
#include "small_pool.h"
#include "impl/prefix.h"

/* //////////////////////////////////////////////////////////////////////////////////////
 * types
 */

// the pool type
typedef struct __tb_pool_t
{
    // the large pool
    tb_large_pool_ref_t     large_pool;

    // the small pool
    tb_small_pool_ref_t     small_pool;

    // the allocator
    tb_allocator_ref_t      allocator;

    // the lock
    tb_spinlock_t           lock;

}tb_pool_t;

/* //////////////////////////////////////////////////////////////////////////////////////
 * globals
 */

// the allocator 
__tb_extern_c__ tb_allocator_ref_t  g_allocator = tb_null;

/* //////////////////////////////////////////////////////////////////////////////////////
 * private implementation
 */
static tb_handle_t tb_pool_instance_init(tb_cpointer_t* ppriv)
{
    // init it
    return tb_pool_init(g_allocator, tb_null);
}
static tb_void_t tb_pool_instance_exit(tb_handle_t pool, tb_cpointer_t priv)
{
#ifdef __tb_debug__
    // dump it
    tb_pool_dump((tb_pool_ref_t)pool);
#endif

    // exit it
    tb_pool_exit((tb_pool_ref_t)pool);
}

/* //////////////////////////////////////////////////////////////////////////////////////
 * implementation
 */
tb_pool_ref_t tb_pool()
{
    return (tb_pool_ref_t)tb_singleton_instance(TB_SINGLETON_TYPE_POOL, tb_pool_instance_init, tb_pool_instance_exit, tb_null);
}
tb_pool_ref_t tb_pool_init(tb_allocator_ref_t allocator, tb_large_pool_ref_t large_pool)
{
    // done
    tb_bool_t   ok = tb_false;
    tb_pool_t*  pool = tb_null;
    do
    {
        // uses allocator?
        if (allocator)
        {
            // make pool
            pool = (tb_pool_t*)tb_allocator_malloc0(allocator, sizeof(tb_pool_t));
            tb_assert_and_check_break(pool);

            // save allocator
            pool->allocator = allocator;

            // ok
            ok = tb_true;
            break;
        }

        // using the default large pool 
        if (!large_pool) large_pool = tb_large_pool();
        tb_assert_and_check_break(large_pool);

        // make pool
        pool = (tb_pool_t*)tb_large_pool_malloc0(large_pool, sizeof(tb_pool_t), tb_null);
        tb_assert_and_check_break(pool);

        // init lock
        if (!tb_spinlock_init(&pool->lock)) break;

        // init pool
        pool->large_pool = large_pool;
        pool->small_pool = tb_small_pool_init(large_pool);
        tb_assert_and_check_break(pool->small_pool);

        // register lock profiler
#ifdef TB_LOCK_PROFILER_ENABLE
        tb_lock_profiler_register(tb_lock_profiler(), (tb_pointer_t)&pool->lock, TB_TRACE_MODULE_NAME);
#endif

        // ok
        ok = tb_true;

    } while (0);

    // failed?
    if (!ok)
    {
        if (pool) tb_pool_exit((tb_pool_ref_t)pool);
        pool = tb_null;
    }

    // ok?
    return (tb_pool_ref_t)pool;
}
tb_void_t tb_pool_exit(tb_pool_ref_t self)
{
    // check
    tb_pool_t* pool = (tb_pool_t*)self;
    tb_assert_and_check_return(pool);

    // uses allocator?
    if (pool->allocator)
    {
        // exit it
        tb_allocator_free(pool->allocator, pool);
        return ;
    }

    // enter
    tb_spinlock_enter(&pool->lock);

    // exit small pool
    if (pool->small_pool) tb_small_pool_exit(pool->small_pool);
    pool->small_pool = tb_null;

    // leave
    tb_spinlock_leave(&pool->lock);

    // exit lock
    tb_spinlock_exit(&pool->lock);

    // exit pool
    if (pool->large_pool) tb_large_pool_free(pool->large_pool, pool);
}
tb_pointer_t tb_pool_malloc_(tb_pool_ref_t self, tb_size_t size __tb_debug_decl__)
{
    // check
    tb_pool_t* pool = (tb_pool_t*)self;
    tb_assert_and_check_return_val(pool, tb_null);
 
    // uses allocator?
    if (pool->allocator) return tb_allocator_malloc_(pool->allocator, size __tb_debug_args__);

    // check
    tb_assert_and_check_return_val(pool->large_pool && pool->small_pool && size, tb_null);

    // enter
    tb_spinlock_enter(&pool->lock);

    // done
    tb_pointer_t data = size <= TB_SMALL_POOL_DATA_SIZE_MAXN? tb_small_pool_malloc_(pool->small_pool, size __tb_debug_args__) : tb_large_pool_malloc_(pool->large_pool, size, tb_null __tb_debug_args__);

    // leave
    tb_spinlock_leave(&pool->lock);

    // ok?
    return data;
}
tb_pointer_t tb_pool_malloc0_(tb_pool_ref_t self, tb_size_t size __tb_debug_decl__)
{
    // check
    tb_pool_t* pool = (tb_pool_t*)self;
    tb_assert_and_check_return_val(pool, tb_null);
    
    // uses allocator?
    if (pool->allocator) return tb_allocator_malloc0_(pool->allocator, size __tb_debug_args__);

    // check
    tb_assert_and_check_return_val(pool->large_pool && pool->small_pool && size, tb_null);

    // enter
    tb_spinlock_enter(&pool->lock);

    // done
    tb_pointer_t data = size <= TB_SMALL_POOL_DATA_SIZE_MAXN? tb_small_pool_malloc0_(pool->small_pool, size __tb_debug_args__) : tb_large_pool_malloc0_(pool->large_pool, size, tb_null __tb_debug_args__);

    // leave
    tb_spinlock_leave(&pool->lock);

    // ok?
    return data;
}
tb_pointer_t tb_pool_nalloc_(tb_pool_ref_t self, tb_size_t item, tb_size_t size __tb_debug_decl__)
{
    // check
    tb_pool_t* pool = (tb_pool_t*)self;
    tb_assert_and_check_return_val(pool, tb_null);
 
    // uses allocator?
    if (pool->allocator) return tb_allocator_nalloc_(pool->allocator, item, size __tb_debug_args__);

    // check
    tb_assert_and_check_return_val(pool->large_pool && pool->small_pool && size, tb_null);

    // enter
    tb_spinlock_enter(&pool->lock);

    // done
    tb_pointer_t data = (item * size) <= TB_SMALL_POOL_DATA_SIZE_MAXN? tb_small_pool_nalloc_(pool->small_pool, item, size __tb_debug_args__) : tb_large_pool_nalloc_(pool->large_pool, item, size, tb_null __tb_debug_args__);

    // leave
    tb_spinlock_leave(&pool->lock);

    // ok?
    return data;
}
tb_pointer_t tb_pool_nalloc0_(tb_pool_ref_t self, tb_size_t item, tb_size_t size __tb_debug_decl__)
{
    // check
    tb_pool_t* pool = (tb_pool_t*)self;
    tb_assert_and_check_return_val(pool, tb_null);
 
    // uses allocator?
    if (pool->allocator) return tb_allocator_nalloc0_(pool->allocator, item, size __tb_debug_args__);

    // check
    tb_assert_and_check_return_val(pool->large_pool && pool->small_pool && size, tb_null);

    // enter
    tb_spinlock_enter(&pool->lock);

    // done
    tb_pointer_t data = (item * size) <= TB_SMALL_POOL_DATA_SIZE_MAXN? tb_small_pool_nalloc0_(pool->small_pool, item, size __tb_debug_args__) : tb_large_pool_nalloc0_(pool->large_pool, item, size, tb_null __tb_debug_args__);

    // leave
    tb_spinlock_leave(&pool->lock);

    // ok?
    return data;
}
tb_pointer_t tb_pool_ralloc_(tb_pool_ref_t self, tb_pointer_t data, tb_size_t size __tb_debug_decl__)
{
    // check
    tb_pool_t* pool = (tb_pool_t*)self;
    tb_assert_and_check_return_val(pool, tb_null);
 
    // uses allocator?
    if (pool->allocator) return tb_allocator_ralloc_(pool->allocator, data, size __tb_debug_args__);

    // check
    tb_assert_and_check_return_val(pool && pool->large_pool && pool->small_pool && size, tb_null);

    // enter
    tb_spinlock_enter(&pool->lock);

    // done
    tb_pointer_t data_new = tb_null;
    do
    {
        // no data?
        if (!data)
        {
            // malloc it directly
            data_new = size <= TB_SMALL_POOL_DATA_SIZE_MAXN? tb_small_pool_malloc_(pool->small_pool, size __tb_debug_args__) : tb_large_pool_malloc_(pool->large_pool, size, tb_null __tb_debug_args__);
            break;
        }

        // the data head
        tb_pool_data_head_t* data_head = &(((tb_pool_data_head_t*)data)[-1]);
        tb_assertf_break(data_head->debug.magic == TB_POOL_DATA_MAGIC, "ralloc invalid data: %p", data);
        tb_assert_and_check_break(data_head->size);

        // small => small
        if (data_head->size <= TB_SMALL_POOL_DATA_SIZE_MAXN && size <= TB_SMALL_POOL_DATA_SIZE_MAXN)
            data_new = tb_small_pool_ralloc_(pool->small_pool, data, size __tb_debug_args__);
        // small => large
        else if (data_head->size <= TB_SMALL_POOL_DATA_SIZE_MAXN)
        {
            // make the new data
            data_new = tb_large_pool_malloc_(pool->large_pool, size, tb_null __tb_debug_args__);
            tb_assert_and_check_break(data_new);

            // copy the old data
            tb_memcpy_(data_new, data, tb_min(data_head->size, size));

            // free the old data
            tb_small_pool_free_(pool->small_pool, data __tb_debug_args__);
        }
        // large => small
        else if (size <= TB_SMALL_POOL_DATA_SIZE_MAXN)
        {
            // make the new data
            data_new = tb_small_pool_malloc_(pool->small_pool, size __tb_debug_args__);
            tb_assert_and_check_break(data_new);

            // copy the old data
            tb_memcpy_(data_new, data, tb_min(data_head->size, size));

            // free the old data
            tb_large_pool_free_(pool->large_pool, data __tb_debug_args__);
        }
        // large => large
        else data_new = tb_large_pool_ralloc_(pool->large_pool, data, size, tb_null __tb_debug_args__);

    } while (0);

    // failed? dump it
#ifdef __tb_debug__
    if (!data_new) 
    {
        // trace
        tb_trace_e("ralloc(%p, %lu) failed! at %s(): %lu, %s", data, size, func_, line_, file_);

        // dump data
        if (data) tb_pool_data_dump((tb_byte_t const*)data, tb_true, "[pool]: [error]: ");

        // abort
        tb_abort();
    }
#endif

    // leave
    tb_spinlock_leave(&pool->lock);

    // ok?
    return data_new;
}
tb_bool_t tb_pool_free_(tb_pool_ref_t self, tb_pointer_t data __tb_debug_decl__)
{
    // check
    tb_pool_t* pool = (tb_pool_t*)self;
    tb_assert_and_check_return_val(pool, tb_false);
 
    // uses allocator?
    if (pool->allocator) return tb_allocator_free_(pool->allocator, data __tb_debug_args__);

    // check
    tb_assert_and_check_return_val(pool->large_pool && pool->small_pool && data, tb_false);

    // enter
    tb_spinlock_enter(&pool->lock);

    // done
    tb_bool_t ok = tb_false;
    do
    {
        // the data head
        tb_pool_data_head_t* data_head = &(((tb_pool_data_head_t*)data)[-1]);
        tb_assertf_break(data_head->debug.magic == TB_POOL_DATA_MAGIC, "free invalid data: %p", data);

        // free it
        ok = (data_head->size <= TB_SMALL_POOL_DATA_SIZE_MAXN)? tb_small_pool_free_(pool->small_pool, data __tb_debug_args__) : tb_large_pool_free_(pool->large_pool, data __tb_debug_args__);

    } while (0);

    // failed? dump it
#ifdef __tb_debug__
    if (!ok) 
    {
        // trace
        tb_trace_e("free(%p) failed! at %s(): %lu, %s", data, func_, line_, file_);

        // dump data
        tb_pool_data_dump((tb_byte_t const*)data, tb_true, "[pool]: [error]: ");

        // abort
        tb_abort();
    }
#endif

    // leave
    tb_spinlock_leave(&pool->lock);

    // ok?
    return ok;
}
tb_pointer_t tb_pool_align_malloc_(tb_pool_ref_t self, tb_size_t size, tb_size_t align __tb_debug_decl__)
{
    // check
    tb_assertf_abort(!(align & 3), "invalid alignment size: %lu", align);
    tb_check_return_val(!(align & 3), tb_null);

    // malloc it
    tb_byte_t* data = (tb_byte_t*)tb_pool_malloc_(self, size + align __tb_debug_args__);
    tb_check_return_val(data, tb_null);

    // the different bytes
    tb_byte_t diff = (tb_byte_t)((~(tb_long_t)data) & (align - 1)) + 1;

    // adjust the address
    data += diff;

    // check
    tb_assert_abort(!((tb_size_t)data & (align - 1)));

    // save the different bytes
    data[-1] = diff;

    // ok?
    return (tb_pointer_t)data;
}
tb_pointer_t tb_pool_align_malloc0_(tb_pool_ref_t self, tb_size_t size, tb_size_t align __tb_debug_decl__)
{
    // malloc it
    tb_pointer_t data = tb_pool_align_malloc_(self, size, align __tb_debug_args__);
    tb_assert_and_check_return_val(data, tb_null);

    // clear it
    tb_memset(data, 0, size);

    // ok
    return data;
}
tb_pointer_t tb_pool_align_nalloc_(tb_pool_ref_t self, tb_size_t item, tb_size_t size, tb_size_t align __tb_debug_decl__)
{
    return tb_pool_align_malloc_(self, item * size, align __tb_debug_args__);
}
tb_pointer_t tb_pool_align_nalloc0_(tb_pool_ref_t self, tb_size_t item, tb_size_t size, tb_size_t align __tb_debug_decl__)
{
    // nalloc it
    tb_pointer_t data = tb_pool_align_nalloc_(self, item, size, align __tb_debug_args__);
    tb_assert_and_check_return_val(data, tb_null);

    // clear it
    tb_memset(data, 0, item * size);

    // ok
    return data;
}
tb_pointer_t tb_pool_align_ralloc_(tb_pool_ref_t self, tb_pointer_t data, tb_size_t size, tb_size_t align __tb_debug_decl__)
{
    // check align
    tb_assertf_abort(!(align & 3), "invalid alignment size: %lu", align);
    tb_check_return_val(!(align & 3), tb_null);

    // ralloc?
    tb_byte_t diff = 0;
    if (data)
    {
        // check address 
        tb_assertf_abort(!((tb_size_t)data & (align - 1)), "invalid address %p", data);
        tb_check_return_val(!((tb_size_t)data & (align - 1)), tb_null);

        // the different bytes
        diff = ((tb_byte_t*)data)[-1];

        // adjust the address
        data = (tb_byte_t*)data - diff;

        // ralloc it
        data = tb_pool_ralloc_(self, data, size + align __tb_debug_args__);
        tb_check_return_val(data, tb_null);
    }
    // no data?
    else
    {
        // malloc it directly
        data = tb_pool_malloc_(self, size + align __tb_debug_args__);
        tb_check_return_val(data, tb_null);
    }

    // the different bytes
    diff = (tb_byte_t)((~(tb_long_t)data) & (align - 1)) + 1;

    // adjust the address
    data = (tb_byte_t*)data + diff;

    // check
    tb_assert_abort(!((tb_size_t)data & (align - 1)));

    // save the different bytes
    ((tb_byte_t*)data)[-1] = diff;

    // ok?
    return data;
}
tb_bool_t tb_pool_align_free_(tb_pool_ref_t self, tb_pointer_t data __tb_debug_decl__)
{
    // check
    tb_assert_and_check_return_val(data, tb_false);
    tb_assert_abort(!((tb_size_t)data & 3));

    // the different bytes
    tb_byte_t diff = ((tb_byte_t*)data)[-1];

    // adjust the address
    data = (tb_byte_t*)data - diff;

    // free it
    return tb_pool_free_(self, data __tb_debug_args__);
}
#ifdef __tb_debug__
tb_void_t tb_pool_dump(tb_pool_ref_t self)
{
    // check
    tb_pool_t* pool = (tb_pool_t*)self;
    tb_assert_and_check_return(pool);

    // uses allocator?
    if (pool->allocator)
    {
        // dump it
        tb_allocator_dump(pool->allocator);
        return ;
    }

    // check
    tb_assert_and_check_return(pool->large_pool && pool->small_pool);

    // enter
    tb_spinlock_enter(&pool->lock);

    // dump pool
    tb_small_pool_dump(pool->small_pool);

    // leave
    tb_spinlock_leave(&pool->lock);
}
#endif
