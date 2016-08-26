// Copyright 2016 Silicon Laboratories, Inc.                                *80*

#include "app/framework/include/af.h"
#include "link-list.h"
#include <stdlib.h>
#include <pthread.h>

static pthread_mutex_t listMutex;

LinkList* linkListInit(void)
{
  LinkList* list = malloc(sizeof(LinkList));
  memset(list, 0, sizeof(LinkList));
  return list;
}

void linkListPushBack(LinkList* list, void* content)
{ 
  pthread_mutex_lock(&listMutex);
  LinkListElement* element = malloc(sizeof(LinkListElement));
  element->content = content;
  element->next = NULL;
  element->previous = list->tail;
  if (list->head == NULL) {
    list->head = element;
  } else {
    list->tail->next = element;
  }
  list->tail = element;
  ++(list->count);
  pthread_mutex_unlock(&listMutex);
}

void linkListPopFront(LinkList* list)
{
  if (list->count > 0) {
    LinkListElement* head = list->head;
    if (list->current == head) {
      list->current = head->next;
    }
    if (list->tail == head) {
      list->tail = NULL;
    }
    list->head = list->head->next;
    if (list->head) {
      list->head->previous = NULL;
    }
    free(head);
    --(list->count);
  }
}

LinkListElement* linkListNextElement(LinkList* list, LinkListElement** elementPosition)
{
  return *elementPosition = (*elementPosition == NULL) ? list->head : (*elementPosition)->next;
}

