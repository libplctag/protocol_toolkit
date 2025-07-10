# Timer API build configuration for Protocol Toolkit
# Add this to your main CMakeLists.txt or Makefile

# Timer library sources
TIMER_SOURCES = src/lib/ptk_timer.c

# Timer library headers
TIMER_HEADERS = src/include/ptk_timer.h

# Compiler flags for timer code
TIMER_CFLAGS = -Wall -Wextra -std=c99 -O2

# Debug build flags
TIMER_DEBUG_CFLAGS = -Wall -Wextra -std=c99 -g -DDEBUG

# Example build targets
timer_example: src/examples/timer_example.c $(TIMER_SOURCES)
	$(CC) $(TIMER_CFLAGS) -I src/include -o timer_example \
		src/examples/timer_example.c $(TIMER_SOURCES)

timer_example_debug: src/examples/timer_example.c $(TIMER_SOURCES)
	$(CC) $(TIMER_DEBUG_CFLAGS) -I src/include -o timer_example_debug \
		src/examples/timer_example.c $(TIMER_SOURCES)

# Static library build
libtimer.a: $(TIMER_SOURCES)
	$(CC) $(TIMER_CFLAGS) -I src/include -c $(TIMER_SOURCES)
	$(AR) rcs libtimer.a ptk_timer.o

# Clean timer artifacts
clean_timer:
	rm -f timer_example timer_example_debug libtimer.a *.o

.PHONY: timer_example timer_example_debug clean_timer
