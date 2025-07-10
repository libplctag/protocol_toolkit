# Long term goals of the Protocol Toolkit

- KISS.
- Tools to simplify code.
- Tools to make code more clear.
- Tools to make code safer.


## Memory Handling

Use of malloc()/free() is extremely discouraged.

The PTK provides memory handling functions that aid in cleanup and clear ownership.

User code should use this to make clean up of memory resources simpler and make it
less likely to miss something.

### Memory API

```c
extern void *ptk_alloc(void *parent, size_t size, void (*destructor)(void *));
extern void *ptk_realloc(void *parent, void *ptr, size_t size);
extern ptk_err ptk_add_child(void *parent, void *child);
extern void ptk_free(void **ptr);
```

### Parents and Children

A parent block of memory is a block that is allocated with a NULL parent.  Calling
`ptk_free()` on a parent frees all its child blocks in reverse order that they were chained.

If you allocate a container as a parent, then all its child objects should be allocated
as its children.

A child is a block of memory that is allocated with a valid parent pointer.  When a child is freed, nothing
happens except marking it as free.  If a child is freed more than once a warning will be logged.

Adding a child that already has a parent to a different parent will take the child and all the children after it and move
them to the new parent at the end of the new parent's child chain.

The children are a simple doubly linked list that is flattened.  There are no sibling pointers.  It is not a tree.
