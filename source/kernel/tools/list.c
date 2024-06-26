#include "tools/list.h"

// we define a list type because there will be many kinds of 
// different lists, so we don't include a pointer in types (ex: task_t)

void list_init(list_t *list) {
    list->first = (list_node_t*)0;
    list->last = (list_node_t*)0;
    list->count = 0;
}

void list_insert_first(list_t *list, list_node_t *node) {
    node->next = list->first;
    node->pre = (list_node_t *)0;
    list->first = node;
    if (list_count(list) > 0) {
        node->next->pre = node;
    } else {
        list->last = node;
    }

    list->count++;
}

void list_insert_last(list_t *list, list_node_t *node) {
    node->next = (list_node_t *)0;
    node->pre = list->last;
    list->last = node;
    if (list_count(list) == 0) {
        list->first = node;
    } else {
        node->pre->next = node;
    }   

    list->count++;
}

void list_remove_first(list_t *list) {
    if (list->count > 1) {
        list->first->next->pre = (list_node_t*)0;
        list->first = list->first->next;
    } else if (list->count == 1) {
        list->first = list->first->next;
        list->last = (list_node_t*)0;
    } else {
        return;
    }

    list->count--; 
}

void list_remove_node(list_t *list, list_node_t *node) {
    if (list->count == 0) {
        return;
    }
    if (list->count == 1 && list->first == node) {
        list->first = list->last = (list_node_t*)0;
        list->count--;
        return;
    }

    list_node_t * it = list->first;
    while (it) {
        if (it != node) {
            it = it->next;
            continue;
        }
        if (it == list->first) {
            it->next->pre = it->pre;
            list->first = it->next;
            list->count--;
            break;
        } else if (it == list->last) {
            it->pre->next = it->next;
            list->last = it->pre;
            list->count--;
            break;
        } else {
            it->next->pre = it->pre;
            it->pre->next = it->next;
            list->count--;
            break;
        }
    }
}