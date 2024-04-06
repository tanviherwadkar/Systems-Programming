#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdbool.h>
#include <string.h>

#include "rwlock.h"
#include "LinkedList.h"

// ==============================================================================
// Structs
typedef struct NodeObj *Node;
typedef struct NodeObj {
    rwlock_t *mylock;
    char *filename;
    Node next;
    Node prev;
} NodeObj;

//private ListObj type
typedef struct ListObj {
    Node front;
    Node back;
    int length;
    Node cursor;
    int index;
} ListObj;
// ==============================================================================

// ==============================================================================
// Function Definitions
Node newNode(char *filename);
void freeNode(Node *pN);
// ==============================================================================

// ==============================================================================
// Constructor/Destructors
Node newNode(char *filename) {
    Node N = malloc(sizeof(NodeObj));
    assert(N != NULL);
    N->mylock = rwlock_new(N_WAY, 1);
    N->filename = strcpy(malloc(strlen(filename) + 1), filename);
    N->next = NULL;
    N->prev = NULL;
    return (N);
}

void freeNode(Node *pN) {
    if (pN != NULL && *pN != NULL) {
        rwlock_delete(&((*pN)->mylock));
        free(*pN);
        *pN = NULL;
    }
}

List newList(void) {
    List L;
    L = malloc(sizeof(ListObj));
    assert(L != NULL);
    L->front = NULL;
    L->length = 0;
    L->cursor = NULL;
    L->index = -1;
    return (L);
}

void freeList(List *pL) {
    if (pL != NULL && *pL != NULL) {
        while ((*pL)->length != 0) {
            deleteBack(*pL);
        }
        free(*pL);
        *pL = NULL;
    }
}
// ==============================================================================

// ==============================================================================
// Access Functions

int length(List L) {
    if (L == NULL) {
        fprintf(stderr, "List Error: calling length(List L) on a NULL List ref\n");
        exit(1);
    }
    return L->length;
}

int get_index(List L) {
    if (L == NULL) {
        fprintf(stderr, "List Error: calling index(List L) on a NULL List ref\n");
        exit(1);
    }
    if (L->cursor == NULL) {
        return -1;
    }
    return L->index;
}

rwlock_t *back(List L) {
    if (L == NULL) {
        fprintf(stderr, "List Error: calling back(List L) on a NULL List ref\n");
        exit(1);
    }
    if (length(L) == 0) {
        fprintf(stderr, "List Error: calling back(List L) on empty List\n");
        exit(1);
    }
    return L->back->mylock;
}

char *get_filename(List L) {
    if (L == NULL) {
        fprintf(stderr, "List Error: calling get_filename(List L) on a NULL List ref\n");
    }
    if (length(L) <= 0) {
        fprintf(stderr, "List Error: calling get_filename(List L) on empty List\n");
        exit(1);
    }

    if (get_index(L) < 0) {
        fprintf(stderr, "List Error: calling get_filename(List L) on List with undefined cursor\n");
        exit(1);
    }
    return L->cursor->filename;
}

rwlock_t *get_lock(List L) {
    if (L == NULL) {
        fprintf(stderr, "List Error: calling get_lock(List L) on a NULL List ref\n");
    }
    if (length(L) <= 0) {
        fprintf(stderr, "List Error: calling get_lock(List L) on empty List\n");
        exit(1);
    }

    if (get_index(L) < 0) {
        fprintf(stderr, "List Error: calling get_lock(List L) on List with undefined cursor\n");
        exit(1);
    }
    return L->cursor->mylock;
}

// ==============================================================================

// ==============================================================================
// Manipulator Functions

void moveFront(List L) {
    if (L == NULL) {
        fprintf(stderr, "List Error: calling moveFront(List L) on a NULL List ref\n");
        exit(1);
    }

    if (length(L) != 0) {
        L->cursor = L->front;
        L->index = 0;
    }
}

void moveNext(List L) {
    if (L == NULL) {
        fprintf(stderr, "List Error: calling movePrev(List L) on NULL List ref\n");
        exit(1);
    }
    if (get_index(L) == (L->length) - 1) {
        L->index = -1;
        L->cursor = NULL;
    } else if (get_index(L) >= 0) {
        L->index += 1;
        L->cursor = L->cursor->next;
    }
}

void append(List L, char *filename) {
    if (L == NULL) {
        fprintf(stderr, "List Error: calling append(List L, int x) on NULL List ref\n");
        exit(1);
    }

    Node N = newNode(filename);
    if (length(L) == 0) {
        L->front = N;
        L->back = N;
        L->length += 1;
    } else {
        L->back->next = N;
        N->prev = L->back;
        L->back = N;
        L->length += 1;
        //printf("%d\n", back(L));
    }
}

void deleteBack(List L) {
    if (L == NULL) {
        fprintf(stderr, "List Error: calling deleteFront(List L) on NULL List ref\n");
        exit(1);
    }

    if (length(L) == 0) {
        fprintf(stderr, "List Error: calling deleteFront(List L) on empty List\n");
        exit(1);
    }

    if (length(L) == 1) {
        Node back = L->back;
        L->front = NULL;
        L->back = NULL;
        L->cursor = NULL;
        L->index = -1;
        L->length -= 1;
        freeNode(&back);
    } else {
        Node back = L->back;
        if (L->cursor == L->back) {
            L->cursor = NULL;
            L->index = -1;
        }
        L->back = L->back->prev;
        L->back->next = NULL;

        L->length -= 1;

        freeNode(&back);
    }
}

// ==============================================================================

// ==============================================================================
// Extra Guys
void printList(FILE *out, List L) {
    if (L == NULL) {
        fprintf(
            stderr, "List Error: calling printList(FILE *out, List L) on NULL List reference\n");
        exit(1);
    }
    Node N = L->front;
    for (int i = 0; i < L->length; i++) {
        assert(N != NULL);
        fprintf(out, "%s ", N->filename);
        N = N->next;
    }
}
// ==============================================================================
