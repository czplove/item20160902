// Copyright 2016 Silicon Laboratories, Inc.                                *80*

#ifndef __LINK_LIST_H
#define __LINK_LIST_H

typedef struct _LinkListElement
{
  struct _LinkListElement *previous;
  struct _LinkListElement *next;
  void* content;
} LinkListElement;

typedef struct _LinkList
{
  LinkListElement *head;
  LinkListElement *tail;
  LinkListElement *current;
  uint32_t count;
} LinkList;

/** @brief Link List Init
 *
 * This function will return an initialized and empty LinkList object.
 */
LinkList* linkListInit(void);

/** @brief Link List Push Back
 *
 * This function will push an element on to the back of a list.
 *
 * @param content void pointer to an object or value to push to the list
 */
void linkListPushBack(LinkList* list, void* content);

/** @brief Link List Pop Front
 *
 * This function will pop an element off of the front of a list.
 *
 * @param list Pointer to the list to pop an element from
 */
void linkListPopFront(LinkList* list);

/** @brief Link List Next Element
 *
 * This function return a pointer to the next element to the provided element. If
 * the provided element is NULL, it will return the head item on the list.
 *
 * @param list Pointer to the list to get the next element from
 * @param elementPosition Pointer to the list element to get the next item from
 */
LinkListElement* linkListNextElement(LinkList* list, LinkListElement** elementPosition);

#endif //__LINK_LIST_H
