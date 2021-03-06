/*
 * Copyright (c) 2017-2018 Bailey Thompson
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdlib.h>
#include <memory.h>
#include <errno.h>
#include "deque.h"

static const int BLOCK_SIZE = 8;
static const double RESIZE_RATIO = 1.5;

struct internal_deque {
    size_t data_size;
    int start_index;
    int end_index;
    int block_count;
    struct node *block;
};

struct node {
    void *data;
};

/**
 * Initializes a deque, which is a doubly-ended queue.
 *
 * @param data_size The size of each element in the deque. Must be positive.
 *
 * @return The newly-initialized deque, or NULL if memory allocation error.
 */
deque deque_init(const size_t data_size)
{
    if (data_size == 0) {
        return NULL;
    }
    struct internal_deque *const init = malloc(sizeof(struct internal_deque));
    if (!init) {
        return NULL;
    }
    init->data_size = data_size;
    init->start_index = BLOCK_SIZE / 2;
    init->end_index = init->start_index + 1;
    init->block_count = 1;
    init->block = malloc(sizeof(struct node));
    if (!init->block) {
        free(init);
        return NULL;
    }
    struct node *const block = init->block;
    block->data = malloc(BLOCK_SIZE * init->data_size);
    if (!block->data) {
        free(init->block);
        free(init);
        return NULL;
    }
    return init;
}

/**
 * Determines the size of the deque. The size is the amount of data spaces being
 * used. The size starts at zero, and every time an element is added, it
 * increases by one.
 *
 * @param me The deque to check size of.
 *
 * @return The size of the deque.
 */
int deque_size(deque me)
{
    return me->end_index - me->start_index - 1;
}

/**
 * Determines if the deque is empty. It is empty if it has no elements.
 *
 * @param me The deque to check if empty.
 *
 * @return If empty.
 */
bool deque_is_empty(deque me)
{
    return deque_size(me) == 0;
}

/**
 * Trims the deque so that it does not use memory which does not need to be
 * used.
 *
 * @param me The deque to trim.
 *
 * @return 0       No error.
 *         -ENOMEM Out of memory.
 */
int deque_trim(deque me)
{
    const int start_block = me->start_index / BLOCK_SIZE;
    const int end_block = me->end_index / BLOCK_SIZE;
    const int new_block_count = end_block - start_block + 1;
    void *const new_block = malloc(new_block_count * sizeof(struct node));
    if (!new_block) {
        return -ENOMEM;
    }
    for (int i = 0; i < start_block; i++) {
        const struct node block_item = me->block[i];
        free(block_item.data);
    }
    for (int i = end_block + 1; i < me->block_count; i++) {
        const struct node block_item = me->block[i];
        free(block_item.data);
    }
    memmove(new_block,
            &me->block[start_block],
            new_block_count * sizeof(struct node));
    free(me->block);
    me->block = new_block;
    me->block_count = new_block_count;
    me->start_index -= start_block * BLOCK_SIZE;
    me->end_index -= start_block * BLOCK_SIZE;
    return 0;
}

/**
 * Copies the deque to an array representation.
 *
 * @param arr The array to copy the deque to.
 * @param me  The deque to copy from.
 */
void deque_copy_to_array(void *const arr, deque me)
{
    for (int i = 0; i < deque_size(me); i++) {
        deque_get_at(arr + i * me->data_size, me, i);
    }
}

/**
 * Adds an element to the front of the deque.
 *
 * @param me   The deque to add an element to.
 * @param data The element to add.
 *
 * @return 0       No error.
 *         -ENOMEM Out of memory.
 */
int deque_push_front(deque me, void *const data)
{
    int block_index = me->start_index / BLOCK_SIZE;
    int inner_index = me->start_index % BLOCK_SIZE;
    if (inner_index == -1) {
        block_index = -1;
        inner_index = BLOCK_SIZE - 1;
    }
    if (inner_index == BLOCK_SIZE - 1) {
        if (block_index == -1) {
            const int old_block_count = me->block_count;
            const int new_block_count =
                    (int) (RESIZE_RATIO * me->block_count) + 1;
            const int added_blocks = new_block_count - old_block_count;
            void *temp = realloc(me->block,
                                 new_block_count * sizeof(struct node));
            if (!temp) {
                return -ENOMEM;
            }
            me->block = temp;
            me->block_count = new_block_count;
            memmove(&me->block[added_blocks],
                    me->block,
                    old_block_count * sizeof(struct node));
            block_index = added_blocks - 1;
            me->start_index += added_blocks * BLOCK_SIZE;
            me->end_index += added_blocks * BLOCK_SIZE;
            for (int i = 0; i < added_blocks; i++) {
                struct node *const block_item = &me->block[i];
                block_item->data = NULL;
            }
        }
        struct node *const block_item = &me->block[block_index];
        if (!block_item->data) {
            block_item->data = malloc(BLOCK_SIZE * me->data_size);
            if (!block_item->data) {
                return -ENOMEM;
            }
        }
    }
    const struct node block_item = me->block[block_index];
    memcpy(block_item.data + inner_index * me->data_size, data, me->data_size);
    me->start_index--;
    return 0;
}

/**
 * Adds an element to the back of the deque.
 *
 * @param me   The deque to add an element to.
 * @param data The element to add.
 *
 * @return 0       No error.
 *         -ENOMEM Out of memory.
 */
int deque_push_back(deque me, void *const data)
{
    const int block_index = me->end_index / BLOCK_SIZE;
    const int inner_index = me->end_index % BLOCK_SIZE;
    if (inner_index == 0) {
        if (block_index == me->block_count) {
            const int new_block_count =
                    (int) (RESIZE_RATIO * me->block_count) + 1;
            void *temp = realloc(me->block,
                                 new_block_count * sizeof(struct node));
            if (!temp) {
                return -ENOMEM;
            }
            me->block = temp;
            me->block_count = new_block_count;
            for (int i = block_index; i < me->block_count; i++) {
                struct node *const block_item = &me->block[i];
                block_item->data = NULL;
            }
        }
        struct node *const block_item = &me->block[block_index];
        if (!block_item->data) {
            block_item->data = malloc(BLOCK_SIZE * me->data_size);
            if (!block_item->data) {
                return -ENOMEM;
            }
        }
    }
    const struct node block_item = me->block[block_index];
    memcpy(block_item.data + inner_index * me->data_size, data, me->data_size);
    me->end_index++;
    return 0;
}

/**
 * Removes the front element from the deque and copies it to a data value.
 *
 * @param data The value to copy to.
 * @param me   The deque to remove from.
 *
 * @return 0       No error.
 *         -EINVAL Invalid argument.
 */
int deque_pop_front(void *const data, deque me)
{
    if (deque_is_empty(me)) {
        return -EINVAL;
    }
    me->start_index++;
    const int block_index = me->start_index / BLOCK_SIZE;
    const int inner_index = me->start_index % BLOCK_SIZE;
    const struct node block_item = me->block[block_index];
    memcpy(data, block_item.data + inner_index * me->data_size, me->data_size);
    return 0;
}

/**
 * Removes the back element from the deque and copies it to a data value.
 *
 * @param data The value to copy to.
 * @param me   The deque to remove from.
 *
 * @return 0       No error.
 *         -EINVAL Invalid argument.
 */
int deque_pop_back(void *const data, deque me)
{
    if (deque_is_empty(me)) {
        return -EINVAL;
    }
    me->end_index--;
    const int block_index = me->end_index / BLOCK_SIZE;
    const int inner_index = me->end_index % BLOCK_SIZE;
    const struct node block_item = me->block[block_index];
    memcpy(data, block_item.data + inner_index * me->data_size, me->data_size);
    return 0;
}

/**
 * Sets the first value of the deque.
 *
 * @param me   The deque to set value of.
 * @param data The data to set.
 *
 * @return 0       No error.
 *         -EINVAL Invalid argument.
 */
int deque_set_first(deque me, void *const data)
{
    return deque_set_at(me, 0, data);
}

/**
 * Sets the value of the deque at the specified index.
 *
 * @param me    The deque to set value of.
 * @param index The index to set at.
 * @param data  The data to set.
 *
 * @return 0       No error.
 *         -EINVAL Invalid argument.
 */
int deque_set_at(deque me, int index, void *const data)
{
    if (index < 0 || index >= deque_size(me)) {
        return -EINVAL;
    }
    index += me->start_index + 1;
    const int block_index = index / BLOCK_SIZE;
    const int inner_index = index % BLOCK_SIZE;
    const struct node block_item = me->block[block_index];
    memcpy(block_item.data + inner_index * me->data_size, data, me->data_size);
    return 0;
}

/**
 * Sets the last value of the deque.
 *
 * @param me   The deque to set value of.
 * @param data The data to set.
 *
 * @return 0       No error.
 *         -EINVAL Invalid argument.
 */
int deque_set_last(deque me, void *const data)
{
    return deque_set_at(me, deque_size(me) - 1, data);
}

/**
 * Gets the first value of the deque.
 *
 * @param data The data to set.
 * @param me   The deque to set value of.
 *
 * @return 0       No error.
 *         -EINVAL Invalid argument.
 */
int deque_get_first(void *const data, deque me)
{
    return deque_get_at(data, me, 0);
}

/**
 * Gets the value of the deque at the specified index.
 *
 * @param data  The data to set.
 * @param me    The deque to set value of.
 * @param index The index to set at.
 *
 * @return 0       No error.
 *         -EINVAL Invalid argument.
 */
int deque_get_at(void *const data, deque me, int index)
{
    if (index < 0 || index >= deque_size(me)) {
        return -EINVAL;
    }
    index += me->start_index + 1;
    const int block_index = index / BLOCK_SIZE;
    const int inner_index = index % BLOCK_SIZE;
    const struct node block_item = me->block[block_index];
    memcpy(data, block_item.data + inner_index * me->data_size, me->data_size);
    return 0;
}

/**
 * Gets the last value of the deque.
 *
 * @param data The data to set.
 * @param me   The deque to set value of.
 *
 * @return 0       No error.
 *         -EINVAL Invalid argument.
 */
int deque_get_last(void *const data, deque me)
{
    return deque_get_at(data, me, deque_size(me) - 1);
}

/**
 * Clears the deque and sets it to the original state from initialization.
 *
 * @param me The deque to clear.
 *
 * @return 0       No error.
 *         -ENOMEM Out of memory.
 */
int deque_clear(deque me)
{
    struct node *const temp_block = malloc(sizeof(struct node));
    if (!temp_block) {
        return -ENOMEM;
    }
    void *const temp_block_data = malloc(BLOCK_SIZE * me->data_size);
    if (!temp_block_data) {
        free(temp_block);
        return -ENOMEM;
    }
    for (int i = 0; i < me->block_count; i++) {
        const struct node block_item = me->block[i];
        free(block_item.data);
    }
    free(me->block);
    me->start_index = BLOCK_SIZE / 2;
    me->end_index = me->start_index + 1;
    me->block_count = 1;
    me->block = temp_block;
    struct node *const block = me->block;
    block->data = temp_block_data;
    return 0;
}

/**
 * Destroys the deque.
 *
 * @param me The deque to destroy.
 *
 * @return NULL
 */
deque deque_destroy(deque me)
{
    for (int i = 0; i < me->block_count; i++) {
        const struct node block_item = me->block[i];
        free(block_item.data);
    }
    free(me->block);
    free(me);
    return NULL;
}
