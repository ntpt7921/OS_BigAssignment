#include "queue.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

/* we will implement the priority queue as a binary heap which will have the
 * max size of MAX_QUEUE_SIZE
 *
 * the binary heap will have a comparison function that favors the pcb with the
 * higher priority
 *
 * the binary heap is a max-heap, meaning that it have its maximum element at
 * the top most (index 0 element)
 */

typedef struct pcb_t pcb_t;
typedef struct queue_t queue_t;

static inline void swap_elem(pcb_t **a, pcb_t **b)
{
    pcb_t *temp = *a;
    *a = *b;
    *b = temp;
}

static inline bool compare_pcb_smaller(pcb_t *p_elem1, pcb_t *p_elem2)
{
    // compare by priority for each element
    return (p_elem1->priority < p_elem2->priority);
}

static inline void add_to_end(queue_t *queue, pcb_t *proc)
{
    queue->proc[queue->size] = proc;
    queue->size++;
}

static inline void swap_start_with_end(queue_t *queue)
{
    swap_elem(&queue->proc[0], &queue->proc[queue->size - 1]);
}

static inline int get_parent_index(int child_index)
{
    if (child_index == 0)
        return 0;
    else
        return (child_index - 1) / 2;
}

static inline int get_left_child_index(int parent_index)
{
    return (2 * parent_index + 1);
}

static inline int get_right_child_index(int parent_index)
{
    return (2 * parent_index + 2);
}

static void sift_up(queue_t *queue, int index)
{
    int current_index = index;
    while (current_index > 0)
    {
        int current_parent_index = get_parent_index(current_index);
        pcb_t *current_parent = queue->proc[current_parent_index];
        pcb_t *current = queue->proc[current_index];

        if (compare_pcb_smaller(current_parent, current))
        {
            // if parent is smaller than current, swap them
            swap_elem(&queue->proc[current_parent_index],
                      &queue->proc[current_index]);
        }
        else
        {
            break;
        }

        current_index = current_parent_index;
    }
}

static void sift_down(queue_t *queue, int index)
{
    int current_index = index;
    while (current_index < queue->size)
    {
        pcb_t *current = queue->proc[current_index];

        int left_child_index = get_left_child_index(current_index);
        if (left_child_index >= queue->size)
            return;
        pcb_t *left_child = queue->proc[left_child_index];

        int right_child_index = get_right_child_index(current_index);
        if (right_child_index >= queue->size)
        {
            // only left child exist, compare and swap with left child
            if (compare_pcb_smaller(current, left_child))
            {
                swap_elem(&queue->proc[current_index],
                          &queue->proc[left_child_index]);
            }
            return;
        }
        pcb_t *right_child = queue->proc[right_child_index];

        // both child exist, find the max child
        // then compare and swap with the max child
        pcb_t *max_child;
        int max_child_index;
        if (compare_pcb_smaller(left_child, right_child))
        {
            max_child = right_child;
            max_child_index = right_child_index;
        }
        else
        {
            max_child = left_child;
            max_child_index = left_child_index;
        }


        if (compare_pcb_smaller(current, max_child))
        {
            swap_elem(&queue->proc[current_index],
                      &queue->proc[max_child_index]);
            current_index = max_child_index;
        }
        else
        {
            return;
        }
    }

    return;
}

int empty(struct queue_t * q) {
    return (q->size == 0);
}

void enqueue(struct queue_t * q, struct pcb_t * proc) {
    /* TODO: put a new process to queue [q] */
    if (q->size >= MAX_QUEUE_SIZE)
        return;

    // add the new elem to end of queue
    add_to_end(q, proc);
    // then sift it up
    sift_up(q, q->size - 1);
}

struct pcb_t * dequeue(struct queue_t * q) {
    /* TODO: return a pcb whose prioprity is the highest
     * in the queue [q] and remember to remove it from q
     * */
    if (q->size == 0)
        return NULL;

    pcb_t *top = q->proc[0];
    swap_start_with_end(q);
    q->size--;
    sift_down(q, 0);

    return top;
}

