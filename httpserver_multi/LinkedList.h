/***
 * Tanvi Herwadkar
 * therwadk
 * 2023 Fall CSE101 PA2
 * List.h
 * Declares all functions for List ADT
***/

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

typedef int ListElement;
typedef struct ListObj *List;

// Constructors-Destructors ---------------------------------------------------

//newList()
//Returns a reference to an empty List
List newList(void);

//freeList()
//Frees any memory allocated in the List
void freeList(List *pL);

//Access functions -----------------------------------------------------------

//length()
//Returns the number of elements in the list L
int length(List L);

//back()
//Returns back element of L.
//Pre: length()>0
rwlock_t *back(List L);

//index()
//Returns the index of the cursor in the list. -1 if cursor is undefined
int get_index(List L);

//get()
//Returns cursor element of L
//Pre: length()>0, index()>=0
char *get_filename(List L);

//get()
//Returns cursor element of L
//Pre: length()>0, index()>=0
rwlock_t *get_lock(List L);

// Manipulation procedures ----------------------------------------------------

//moveFront()
// If L is non-empty, sets cursor under the front element,otherwise does nothing.
void moveFront(List L);

//moveNext()
// If cursor is defined and not at back, move cursor one
// step toward the back of L; if cursor is defined and at
// back, cursor becomes undefined; if cursor is undefined
// do nothing
void moveNext(List L);

//append()
//Insert new element into L. If L is non-empty,
//insertion takes place after back element.
void append(List L, char *filename);

//deleteBack()
//Delete the back element.
//Pre: length()>0
void deleteBack(List L);

//printList()
//Prints to the file pointed to by out, a
//string representation of L consisting
//of a space separated sequence of integers,
//with front on left.
void printList(FILE *out, List L);
