Rules for creating functions:

- Functions must be spaced at least one blank line apart.
- Functions must have comments in Doxygen-compatible format that include all parameters and the return value.
- Where error handling is a bit non-standard (i.e. use of errno or  code like in ptk_err.h) document where and how the error can be retrieved.
- Creating or finding/search functions should return their creations. Use a different channel for error.
- Use the functions and definitions in ptk_log.h for all logging. The macros defined there will automatically include the surrounding function name and the line number in the file so do not put the
function name or line number in the log messages.
- Failures that are somewhat recoverable like running out of filedescriptors should log with warn().
- Failures that are catastrophic like malloc returning NULL should be logged with error().
- Function entry and exit should be logged at level info().
- Important steps within functions should be logged at debug().
- Code that would generate a lot of logging (i.e. big looks, callbacks every 10ms) should be logged at level trace().
- all allocation of memory should be done with ptk_alloc() and a destructor should be included if there is any attached data that needs to be freed or disposed of safely.  If you find yourself writing code that looks like this:
```c
     if(obj->field) { ptk_free(obj->field); }
     ptk_free(obj);
```
That means that the place where obj was created should have provided a destructor function.  There should only be one place that all the details of disposing of an object are known.
- if there is more than 10% of a file to change, rewrite it in a different file and mv it over.
- do not provide publically accessible destructor functions for your data.  Use the destructor callback with ptk_alloc().  All memory should be freed just with ptk_free().

