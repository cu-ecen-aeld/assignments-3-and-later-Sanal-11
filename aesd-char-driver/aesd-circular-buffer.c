/**
 * @file aesd-circular-buffer.c
 * @brief Functions and data related to a circular buffer imlementation
 *
 * @author Dan Walkes
 * @date 2020-03-01
 * @copyright Copyright (c) 2020
 *
 */

#ifdef __KERNEL__
#include <linux/string.h>
#include <linux/slab.h>  // for kmalloc
#else
#include <string.h>
#include <stdlib.h>      // for malloc
#include <stdio.h>
#endif

// Unified free macro
#ifdef __KERNEL__
#define xfree(ptr) kfree(ptr)
#else
#define xfree(ptr) free(ptr)
#endif

#include "aesd-circular-buffer.h"

/**
 * @param buffer the buffer to search for corresponding offset.  Any necessary locking must be performed by caller.
 * @param char_offset the position to search for in the buffer list, describing the zero referenced
 *      character index if all buffer strings were concatenated end to end
 * @param entry_offset_byte_rtn is a pointer specifying a location to store the byte of the returned aesd_buffer_entry
 *      buffptr member corresponding to char_offset.  This value is only set when a matching char_offset is found
 *      in aesd_buffer.
 * @return the struct aesd_buffer_entry structure representing the position described by char_offset, or
 * NULL if this position is not available in the buffer (not enough data is written).
 */
struct aesd_buffer_entry *aesd_circular_buffer_find_entry_offset_for_fpos(struct aesd_circular_buffer *buffer,
            size_t char_offset, size_t *entry_offset_byte_rtn )
{
    size_t cumulative_offset = 0;
    uint8_t index = buffer->out_offs;
    uint8_t count = 0;

    // Loop through valid entries in order
    while ((count < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED) && 
           (buffer->full || index != buffer->in_offs)) 
    {
        struct aesd_buffer_entry *entry = &buffer->entry[index];

        if (char_offset < (cumulative_offset + entry->size)) {
            *entry_offset_byte_rtn = char_offset - cumulative_offset;
            PDEBUG("Info: Offset found in available data");
            return entry;
        }

        cumulative_offset += entry->size;
        index = (index + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
        count++;
    }
        PDEBUG("while check: %d", (count < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED) && \
                (buffer->full || index != buffer->in_offs));
                
    PDEBUG("Info: in_offs %d", buffer->in_offs);
    PDEBUG("Info: out_offs %d", buffer->out_offs);
    PDEBUG("Info: buff_full %d", buffer->full);
    PDEBUG("Info: buff_full %d", buffer->full);
    PDEBUG("Info: count %d", count);
    return NULL;
}

/**
* Adds entry @param add_entry to @param buffer in the location specified in buffer->in_offs.
* If the buffer was already full, overwrites the oldest entry and advances buffer->out_offs to the
* new start location.
* Any necessary locking must be handled by the caller
* Any memory referenced in @param add_entry must be allocated by and/or must have a lifetime managed by the caller.
*/
void aesd_circular_buffer_add_entry(struct aesd_circular_buffer *buffer, const struct aesd_buffer_entry *add_entry)
{
    if (buffer->full) {
        // Free the memory of the entry being overwritten
        // xfree((void *)buffer->entry[buffer->out_offs].buffptr);

        // Advance out_offs since we're overwriting the oldest entry
        buffer->out_offs = (buffer->out_offs + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    }

    // Copy the new entry into the buffer
    buffer->entry[buffer->in_offs] = *add_entry;

    // Advance in_offs
    buffer->in_offs = (buffer->in_offs + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;

    // Check if we're now full
    buffer->full = (buffer->in_offs == buffer->out_offs);
}

/**
* Initializes the circular buffer described by @param buffer to an empty struct
*/
void aesd_circular_buffer_init(struct aesd_circular_buffer *buffer)
{
    memset(buffer,0,sizeof(struct aesd_circular_buffer));
}
