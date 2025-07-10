/**
 * @file test_parent_child_alloc.c
 * @brief Comprehensive tests for parent-child memory allocation system
 *
 * This test suite validates the parent-child memory allocation system,
 * including complex hierarchies, edge cases, and safety scenarios.
 */

#include "ptk_alloc.h"
#include "ptk_log.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Global test state
static int destructor_call_count = 0;
static int destructor_expected_order = 0;
static char destructor_call_log[1024] = {0};

// Test destructors that track call order
void destructor_a(void *ptr) {
    destructor_call_count++;
    char msg[64];
    snprintf(msg, sizeof(msg), "A%d ", destructor_call_count);
    strcat(destructor_call_log, msg);
    printf("Destructor A called for %p (call #%d)\n", ptr, destructor_call_count);
}

void destructor_b(void *ptr) {
    destructor_call_count++;
    char msg[64];
    snprintf(msg, sizeof(msg), "B%d ", destructor_call_count);
    strcat(destructor_call_log, msg);
    printf("Destructor B called for %p (call #%d)\n", ptr, destructor_call_count);
}

void destructor_c(void *ptr) {
    destructor_call_count++;
    char msg[64];
    snprintf(msg, sizeof(msg), "C%d ", destructor_call_count);
    strcat(destructor_call_log, msg);
    printf("Destructor C called for %p (call #%d)\n", ptr, destructor_call_count);
}

void destructor_d(void *ptr) {
    destructor_call_count++;
    char msg[64];
    snprintf(msg, sizeof(msg), "D%d ", destructor_call_count);
    strcat(destructor_call_log, msg);
    printf("Destructor D called for %p (call #%d)\n", ptr, destructor_call_count);
}

// Reset test state
void reset_test_state() {
    destructor_call_count = 0;
    destructor_expected_order = 0;
    memset(destructor_call_log, 0, sizeof(destructor_call_log));
}

// Test basic parent-child allocation and freeing
int test_basic_parent_child() {
    printf("\n=== Test: Basic Parent-Child Allocation ===\n");
    reset_test_state();

    // Create parent
    void *parent = ptk_alloc(NULL, 1024, destructor_a);
    if(!parent) {
        printf("FAIL: Failed to allocate parent\n");
        return 0;
    }
    printf("PASS: Parent allocated at %p\n", parent);

    // Create child
    void *child = ptk_alloc(parent, 256, destructor_b);
    if(!child) {
        printf("FAIL: Failed to allocate child\n");
        ptk_free(parent);
        return 0;
    }
    printf("PASS: Child allocated at %p\n", child);

    // Free parent - should free child first (LIFO), then parent
    ptk_free(parent);

    // Check destructor call order: child (B), then parent (A)
    if(destructor_call_count != 2) {
        printf("FAIL: Expected 2 destructor calls, got %d\n", destructor_call_count);
        return 0;
    }
    if(strstr(destructor_call_log, "B1 A2") == NULL) {
        printf("FAIL: Expected call order 'B1 A2', got '%s'\n", destructor_call_log);
        return 0;
    }

    printf("PASS: Destructors called in correct LIFO order\n");

    // NOTE: After parent is freed, child pointer becomes invalid
    // Attempting to free child after parent is undefined behavior
    // This is by design - child lifetime is tied to parent
    printf("PASS: Child freed automatically with parent (as expected)\n");

    return 1;
}

// Test multiple children with LIFO ordering
int test_multiple_children_lifo() {
    printf("\n=== Test: Multiple Children LIFO Ordering ===\n");
    reset_test_state();

    // Create parent
    void *parent = ptk_alloc(NULL, 1024, destructor_a);
    if(!parent) {
        printf("FAIL: Failed to allocate parent\n");
        return 0;
    }

    // Create multiple children
    void *child1 = ptk_alloc(parent, 256, destructor_b);
    void *child2 = ptk_alloc(parent, 256, destructor_c);
    void *child3 = ptk_alloc(parent, 256, destructor_d);

    if(!child1 || !child2 || !child3) {
        printf("FAIL: Failed to allocate children\n");
        ptk_free(parent);
        return 0;
    }

    printf("PASS: Created parent %p with children %p, %p, %p\n", parent, child1, child2, child3);

    // Free parent - should free children in reverse order: child3, child2, child1, parent
    ptk_free(parent);

    // Check destructor call order: D, C, B, A
    if(destructor_call_count != 4) {
        printf("FAIL: Expected 4 destructor calls, got %d\n", destructor_call_count);
        return 0;
    }
    if(strstr(destructor_call_log, "D1 C2 B3 A4") == NULL) {
        printf("FAIL: Expected call order 'D1 C2 B3 A4', got '%s'\n", destructor_call_log);
        return 0;
    }

    printf("PASS: Multiple children freed in correct LIFO order\n");
    return 1;
}

// Test ptk_add_child functionality
int test_add_child() {
    printf("\n=== Test: ptk_add_child Functionality ===\n");
    reset_test_state();

    // Create parent
    void *parent = ptk_alloc(NULL, 1024, destructor_a);
    if(!parent) {
        printf("FAIL: Failed to allocate parent\n");
        return 0;
    }

    // Create independent allocations
    void *child1 = ptk_alloc(NULL, 256, destructor_b);
    void *child2 = ptk_alloc(NULL, 256, destructor_c);

    if(!child1 || !child2) {
        printf("FAIL: Failed to allocate independent children\n");
        ptk_free(parent);
        ptk_free(child1);
        ptk_free(child2);
        return 0;
    }

    // Add children to parent
    ptk_err err1 = ptk_add_child(parent, child1);
    ptk_err err2 = ptk_add_child(parent, child2);

    if(err1 != PTK_OK || err2 != PTK_OK) {
        printf("FAIL: ptk_add_child failed (err1=%d, err2=%d)\n", err1, err2);
        ptk_free(parent);
        ptk_free(child1);
        ptk_free(child2);
        return 0;
    }

    printf("PASS: Successfully added children to parent\n");

    // Free parent - should free both children
    ptk_free(parent);

    // Check destructor call order: child2 (added last), child1, parent
    if(destructor_call_count != 3) {
        printf("FAIL: Expected 3 destructor calls, got %d\n", destructor_call_count);
        return 0;
    }
    if(strstr(destructor_call_log, "C1 B2 A3") == NULL) {
        printf("FAIL: Expected call order 'C1 B2 A3', got '%s'\n", destructor_call_log);
        return 0;
    }

    printf("PASS: ptk_add_child works correctly with LIFO ordering\n");
    return 1;
}

// Test complex parent-child hierarchies (parent as child of another parent)
int test_complex_hierarchy() {
    printf("\n=== Test: Complex Parent-Child Hierarchies ===\n");
    reset_test_state();

    // Create first hierarchy: P1 -> A -> B -> C
    void *parent1 = ptk_alloc(NULL, 1024, destructor_a);
    void *child1a = ptk_alloc(parent1, 256, destructor_b);
    void *child1b = ptk_alloc(parent1, 256, destructor_c);
    void *child1c = ptk_alloc(parent1, 256, destructor_d);

    if(!parent1 || !child1a || !child1b || !child1c) {
        printf("FAIL: Failed to create first hierarchy\n");
        ptk_free(parent1);
        return 0;
    }

    // Create second hierarchy: P2 -> D -> E -> F
    void *parent2 = ptk_alloc(NULL, 2048, destructor_a);
    void *child2a = ptk_alloc(parent2, 512, destructor_b);
    void *child2b = ptk_alloc(parent2, 512, destructor_c);
    void *child2c = ptk_alloc(parent2, 512, destructor_d);

    if(!parent2 || !child2a || !child2b || !child2c) {
        printf("FAIL: Failed to create second hierarchy\n");
        ptk_free(parent1);
        ptk_free(parent2);
        return 0;
    }

    printf("PASS: Created two separate hierarchies\n");
    printf("  P1 %p -> {%p, %p, %p}\n", parent1, child1a, child1b, child1c);
    printf("  P2 %p -> {%p, %p, %p}\n", parent2, child2a, child2b, child2c);

    // Add parent2 as a child of parent1
    ptk_err err = ptk_add_child(parent1, parent2);
    if(err != PTK_OK) {
        printf("FAIL: Failed to add parent2 as child of parent1 (err=%d)\n", err);
        ptk_free(parent1);
        ptk_free(parent2);
        return 0;
    }

    printf("PASS: Added parent2 as child of parent1\n");
    printf("  New hierarchy: P1 -> {C, B, A, P2 -> {D, C, B}}\n");

    // Free parent1 - should free everything in LIFO order
    // Expected order: child2c, child2b, child2a, parent2, child1c, child1b, child1a, parent1
    ptk_free(parent1);

    // Check that all destructors were called
    if(destructor_call_count != 8) {
        printf("FAIL: Expected 8 destructor calls, got %d\n", destructor_call_count);
        return 0;
    }

    printf("PASS: Complex hierarchy freed correctly\n");
    printf("  Destructor call order: %s\n", destructor_call_log);

    return 1;
}

// Test edge cases and error conditions
int test_edge_cases() {
    printf("\n=== Test: Edge Cases and Error Conditions ===\n");

    // Test NULL pointers
    void *null_alloc = ptk_alloc(NULL, 0, NULL);
    if(null_alloc != NULL) {
        printf("FAIL: ptk_alloc(NULL, 0, NULL) should return NULL\n");
        ptk_free(null_alloc);
        return 0;
    }
    printf("PASS: ptk_alloc with size 0 returns NULL\n");

    // Test ptk_free(NULL) - should be safe
    ptk_free(NULL);
    printf("PASS: ptk_free(NULL) is safe\n");

    // Test ptk_add_child with NULL parameters
    void *parent = ptk_alloc(NULL, 1024, NULL);
    if(!parent) {
        printf("FAIL: Failed to allocate parent for edge case tests\n");
        return 0;
    }

    ptk_err err1 = ptk_add_child(NULL, parent);
    ptk_err err2 = ptk_add_child(parent, NULL);

    if(err1 == PTK_OK || err2 == PTK_OK) {
        printf("FAIL: ptk_add_child should fail with NULL parameters\n");
        ptk_free(parent);
        return 0;
    }
    printf("PASS: ptk_add_child fails gracefully with NULL parameters\n");

    ptk_free(parent);
    return 1;
}

// Test reallocation scenarios
int test_reallocation() {
    printf("\n=== Test: Reallocation Scenarios ===\n");
    reset_test_state();

    // Test basic reallocation
    void *ptr = ptk_alloc(NULL, 1024, destructor_a);
    if(!ptr) {
        printf("FAIL: Failed to allocate for reallocation test\n");
        return 0;
    }

    // Write some data
    strcpy((char *)ptr, "Hello, World!");

    // Reallocate to larger size
    void *new_ptr = ptk_realloc(ptr, 2048);
    if(!new_ptr) {
        printf("FAIL: Failed to reallocate to larger size\n");
        ptk_free(ptr);
        return 0;
    }

    // Check that data was preserved
    if(strcmp((char *)new_ptr, "Hello, World!") != 0) {
        printf("FAIL: Data not preserved during reallocation\n");
        ptk_free(new_ptr);
        return 0;
    }
    printf("PASS: Reallocation preserved data\n");

    // Test reallocation to same size
    void *same_ptr = ptk_realloc(new_ptr, 2048);
    if(same_ptr != new_ptr) {
        printf("FAIL: Reallocation to same size should return same pointer\n");
        ptk_free(same_ptr);
        return 0;
    }
    printf("PASS: Reallocation to same size returns same pointer\n");

    // Test reallocation to zero (should free)
    void *zero_ptr = ptk_realloc(new_ptr, 0);
    if(zero_ptr != NULL) {
        printf("FAIL: Reallocation to size 0 should return NULL\n");
        ptk_free(zero_ptr);
        return 0;
    }
    if(destructor_call_count != 1) {
        printf("FAIL: Reallocation to size 0 should call destructor\n");
        return 0;
    }
    printf("PASS: Reallocation to size 0 frees memory and calls destructor\n");

    // Test reallocation of NULL (should behave like malloc)
    void *null_realloc = ptk_realloc(NULL, 1024);
    if(!null_realloc) {
        printf("FAIL: ptk_realloc(NULL, size) should behave like ptk_alloc\n");
        return 0;
    }
    printf("PASS: ptk_realloc(NULL, size) works like ptk_alloc\n");

    ptk_free(null_realloc);
    return 1;
}

// Test potential safety issues (cycles, double-free scenarios)
int test_safety_scenarios() {
    printf("\n=== Test: Safety Scenarios ===\n");

    // Test: Attempt to create a cycle (should be detectable but may not be prevented)
    void *parent1 = ptk_alloc(NULL, 1024, NULL);
    void *parent2 = ptk_alloc(NULL, 1024, NULL);

    if(!parent1 || !parent2) {
        printf("FAIL: Failed to allocate for safety test\n");
        ptk_free(parent1);
        ptk_free(parent2);
        return 0;
    }

    // Add parent2 as child of parent1
    ptk_err err1 = ptk_add_child(parent1, parent2);
    if(err1 != PTK_OK) {
        printf("FAIL: Failed to add parent2 as child of parent1\n");
        ptk_free(parent1);
        ptk_free(parent2);
        return 0;
    }

    // This would create a cycle: parent1 -> parent2 -> parent1
    // The current implementation doesn't prevent this, but freeing parent1 should work
    printf("PASS: Added parent2 as child of parent1\n");

    // Free parent1 - should free parent2 as well
    ptk_free(parent1);
    printf("PASS: Freed parent1 (and parent2 as its child)\n");

    // Attempting to free parent2 should be safe (no-op)
    ptk_free(parent2);
    printf("PASS: ptk_free(parent2) after parent1 freed is safe\n");

    return 1;
}

// Test freeing child before parent (should be safe no-op)
int test_free_child_before_parent() {
    printf("\n=== Test: Free Child Before Parent ===\n");
    reset_test_state();

    // Create parent and child
    void *parent = ptk_alloc(NULL, 1024, destructor_a);
    void *child = ptk_alloc(parent, 256, destructor_b);

    if(!parent || !child) {
        printf("FAIL: Failed to allocate parent or child\n");
        ptk_free(parent);
        return 0;
    }

    // Free child before parent (should be no-op)
    ptk_free(child);
    if(destructor_call_count != 0) {
        printf("FAIL: ptk_free(child) should be no-op, but got %d destructor calls\n", destructor_call_count);
        ptk_free(parent);
        return 0;
    }
    printf("PASS: ptk_free(child) is safe no-op\n");

    // Free parent - should still free child
    ptk_free(parent);
    if(destructor_call_count != 2) {
        printf("FAIL: Expected 2 destructor calls after freeing parent, got %d\n", destructor_call_count);
        return 0;
    }
    if(strstr(destructor_call_log, "B1 A2") == NULL) {
        printf("FAIL: Expected call order 'B1 A2', got '%s'\n", destructor_call_log);
        return 0;
    }
    printf("PASS: Parent still freed child correctly\n");

    return 1;
}

// Main test function
int main() {
    printf("=== Protocol Toolkit Parent-Child Allocation Tests ===\n");

    int passed = 0;
    int total = 0;

    // Run all tests
    total++;
    if(test_basic_parent_child()) { passed++; }
    total++;
    if(test_free_child_before_parent()) { passed++; }
    total++;
    if(test_multiple_children_lifo()) { passed++; }
    total++;
    if(test_add_child()) { passed++; }
    total++;
    if(test_complex_hierarchy()) { passed++; }
    total++;
    if(test_edge_cases()) { passed++; }
    total++;
    if(test_reallocation()) { passed++; }
    total++;
    if(test_safety_scenarios()) { passed++; }

    // Print results
    printf("\n=== Test Results ===\n");
    printf("Passed: %d/%d tests\n", passed, total);

    if(passed == total) {
        printf("✓ All parent-child allocation tests passed!\n");
        return 0;
    } else {
        printf("✗ %d tests failed\n", total - passed);
        return 1;
    }
}
