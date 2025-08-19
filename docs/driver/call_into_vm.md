---
layout: doc
title: general / call_into_vm
---
# call_into_vm

### Author

<sunyucong@gmail.com>

### Date

2017-08-29

The VM Driver code consists of roughly 2 parts:

1. VM-Related Code. This is the LPC VM and associated stack machine.
   All EFUN functions belong in this part.
2. Non-VM Code: this is mostly communication-related functionality.

To execute LPC, Non-VM code first needs to be prepared:

1. Push control frame to control frame stack.
2. Push svalue_t arguments into stack
3. Set PC and various variables for VM code.
4. Call `execute_instruction(pc)`
5. Deal with any error conditions; if no errors, use the value and reset everything.

Normally, Non-VM code _should not_ go through this process, as there are
currently several helper functions that most of the driver code uses, such as:

- safe_apply()
- safe_call_function_pointer()
- ...

However, several parts of driver can't use these functions, instead they are
using:

- apply()
- call_direct()
- call_program()

For this code, the only correct way of doing this is documented here.

```cpp
//  use push_number() , push_malloc_string() etc to push arguments into stack
    num_arg = X; // MUST remember how many arguments were pushed.

    // setup error context
    error_context_t econ;
    save_context(&econ);

    try {
        ret = call_function_pointer(funp, num_arg);
    } catch (const char *) {
        restore_context(&econ);
        /* condition was restored to where it was when we came in */
        pop_n_elems(num_arg);
        ret = nullptr;
    }
    pop_context(&econ);
```

_**(unfinished)**_

LPC VM use C++ Exception to handle _any_ error encountered during LPC execution.
This could come from the following non-exhaustive list:

- an error generated from `throw()` in LPC code
- the `error()` function from EFUN implementations
