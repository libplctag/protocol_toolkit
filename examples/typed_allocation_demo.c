/**
 * @file typed_allocation_demo.c
 * @brief Demonstration of type-safe allocation approaches for Protocol Toolkit
 */

#include <stdio.h>
#include <ptk.h>
#include <ptk_mem.h>
#include <ptk_buf.h>
#include <ptk_sock.h>

//=============================================================================
// APPROACH 1: Simple Type-Tagged Handles
//=============================================================================

typedef enum {
    PTK_ALLOC_BUFFER = 1,
    PTK_ALLOC_SOCKET,
    PTK_ALLOC_STRING,
    PTK_ALLOC_ADDRESS
} ptk_alloc_type_t;

typedef struct {
    ptk_shared_handle_t handle;
    ptk_alloc_type_t type;
    size_t size;
} ptk_typed_ptr_t;

// Type-safe allocation macro using _Generic
#define ptk_safe_alloc(type_ptr, count) _Generic((type_ptr), \
    ptk_buf**: ptk_alloc_buffer_impl, \
    ptk_sock**: ptk_alloc_socket_impl, \
    char**: ptk_alloc_string_impl, \
    ptk_address_t**: ptk_alloc_address_impl \
)((void**)type_ptr, count)

// Implementation functions
static ptk_err_t ptk_alloc_buffer_impl(void **ptr, size_t count) {
    printf("Allocating %zu ptk_buf objects\n", count);
    *ptr = ptk_local_alloc(sizeof(ptk_buf) * count, NULL);
    return *ptr ? PTK_OK : PTK_ERR_NO_RESOURCES;
}

static ptk_err_t ptk_alloc_socket_impl(void **ptr, size_t count) {
    printf("Allocating %zu ptk_sock objects\n", count);
    *ptr = ptk_local_alloc(sizeof(ptk_sock) * count, NULL);
    return *ptr ? PTK_OK : PTK_ERR_NO_RESOURCES;
}

static ptk_err_t ptk_alloc_string_impl(void **ptr, size_t count) {
    printf("Allocating %zu char buffer\n", count);
    *ptr = ptk_local_alloc(count, NULL);
    return *ptr ? PTK_OK : PTK_ERR_NO_RESOURCES;
}

static ptk_err_t ptk_alloc_address_impl(void **ptr, size_t count) {
    printf("Allocating %zu ptk_address_t objects\n", count);
    *ptr = ptk_local_alloc(sizeof(ptk_address_t) * count, NULL);
    return *ptr ? PTK_OK : PTK_ERR_NO_RESOURCES;
}

//=============================================================================
// APPROACH 2: Macro with typeof (GCC/Clang specific)
//=============================================================================

#ifdef __GNUC__
#define ptk_typed_new(type, count) ({ \
    type *_ptr = (type*)ptk_local_alloc(sizeof(type) * (count), NULL); \
    if (_ptr) printf("Allocated %zu %s objects at %p\n", (size_t)(count), #type, _ptr); \
    _ptr; \
})
#endif

//=============================================================================
// APPROACH 3: Enhanced Handle System
//=============================================================================

typedef struct {
    ptk_shared_handle_t handle;
    const char *type_name;
    size_t element_size;
    size_t element_count;
} ptk_smart_handle_t;

#define PTK_SMART_ALLOC(type, count) \
    ptk_smart_alloc_impl(sizeof(type), count, #type)

static ptk_smart_handle_t ptk_smart_alloc_impl(size_t element_size, size_t count, const char *type_name) {
    ptk_smart_handle_t result = {
        .handle = ptk_shared_alloc(element_size * count, NULL),
        .type_name = type_name,
        .element_size = element_size,
        .element_count = count
    };
    
    if (PTK_SHARED_IS_VALID(result.handle)) {
        printf("Smart allocated %zu %s objects (total %zu bytes)\n", 
               count, type_name, element_size * count);
    }
    
    return result;
}

static void* ptk_smart_get(ptk_smart_handle_t handle, const char *expected_type) {
    if (!PTK_SHARED_IS_VALID(handle.handle)) {
        printf("ERROR: Invalid handle for type %s\n", expected_type);
        return NULL;
    }
    
    // In debug builds, you could add type name comparison
    #ifdef DEBUG
    if (strcmp(handle.type_name, expected_type) != 0) {
        printf("WARNING: Type mismatch - expected %s, got %s\n", 
               expected_type, handle.type_name);
    }
    #endif
    
    void *ptr = NULL;
    use_shared(handle.handle, void*, ptr, PTK_TIME_NO_WAIT) {
        // ptr is now valid
        return ptr;
    } on_shared_fail {
        printf("ERROR: Failed to acquire handle for type %s\n", expected_type);
        return NULL;
    }
    return ptr;
}

#define PTK_SMART_GET(handle, type) \
    ((type*)ptk_smart_get(handle, #type))

//=============================================================================
// DEMONSTRATION FUNCTION
//=============================================================================

void demonstrate_type_safe_allocation(void) {
    printf("=== Type-Safe Allocation Demo ===\n\n");
    
    // Approach 1: _Generic dispatch
    printf("1. Using _Generic dispatch:\n");
    ptk_buf *buffers = NULL;
    char *string = NULL;
    ptk_address_t *addresses = NULL;
    
    if (ptk_safe_alloc(&buffers, 5) == PTK_OK) {
        printf("   Successfully allocated ptk_buf array\n");
        ptk_local_free(&buffers);
    }
    
    if (ptk_safe_alloc(&string, 256) == PTK_OK) {
        printf("   Successfully allocated string buffer\n");
        ptk_local_free(&string);
    }
    
    if (ptk_safe_alloc(&addresses, 3) == PTK_OK) {
        printf("   Successfully allocated address array\n");
        ptk_local_free(&addresses);
    }
    
    printf("\n");
    
    // Approach 2: typeof macro (GCC/Clang)
    #ifdef __GNUC__
    printf("2. Using typeof macro:\n");
    int *numbers = ptk_typed_new(int, 10);
    ptk_buf *single_buffer = ptk_typed_new(ptk_buf, 1);
    
    if (numbers) {
        printf("   typeof macro allocated int array\n");
        ptk_local_free(&numbers);
    }
    
    if (single_buffer) {
        printf("   typeof macro allocated single buffer\n");
        ptk_local_free(&single_buffer);
    }
    printf("\n");
    #endif
    
    // Approach 3: Smart handles
    printf("3. Using smart handles:\n");
    ptk_smart_handle_t buf_handle = PTK_SMART_ALLOC(ptk_buf, 2);
    ptk_smart_handle_t int_handle = PTK_SMART_ALLOC(int, 50);
    
    ptk_buf *smart_buffers = PTK_SMART_GET(buf_handle, ptk_buf);
    int *smart_numbers = PTK_SMART_GET(int_handle, int);
    
    if (smart_buffers) {
        printf("   Smart handle provided ptk_buf access\n");
    }
    
    if (smart_numbers) {
        printf("   Smart handle provided int array access\n");
    }
    
    // Cleanup happens automatically when handles go out of scope
    // or you can explicitly free them
    ptk_shared_free(&buf_handle.handle);
    ptk_shared_free(&int_handle.handle);
    
    printf("\n=== Demo Complete ===\n");
}

// Example integration with existing PTK buffer system
void demonstrate_buffer_integration(void) {
    printf("\n=== Buffer Integration Demo ===\n");
    
    // Enhanced buffer allocation that's type-safe
    #define ptk_buf_new_safe(size) ({ \
        ptk_buf *_buf = ptk_buf_alloc(size); \
        printf("Type-safe buffer allocation: %p (size %u)\n", _buf, (unsigned)(size)); \
        _buf; \
    })
    
    ptk_buf *buffer1 = ptk_buf_new_safe(1024);
    ptk_buf *buffer2 = ptk_buf_new_safe(512);
    
    if (buffer1 && buffer2) {
        printf("Successfully created type-safe buffers\n");
        
        // Use the buffers normally
        ptk_buf_size_t len1 = ptk_buf_get_capacity(buffer1);
        ptk_buf_size_t len2 = ptk_buf_get_capacity(buffer2);
        
        printf("Buffer 1 capacity: %u\n", (unsigned)len1);
        printf("Buffer 2 capacity: %u\n", (unsigned)len2);
    }
    
    // Cleanup
    ptk_local_free(&buffer1);
    ptk_local_free(&buffer2);
}

int main(void) {
    if (ptk_startup() != PTK_OK) {
        printf("Failed to initialize PTK\n");
        return 1;
    }
    
    demonstrate_type_safe_allocation();
    demonstrate_buffer_integration();
    
    ptk_shutdown();
    return 0;
}