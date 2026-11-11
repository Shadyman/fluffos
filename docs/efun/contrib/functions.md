---
layout: doc
title: contrib / functions.pre
---
# functions

### NAME

    functions() - returns a string array of function names or mixed* array of
    information for all functions in a passed object.

### SYNOPSIS

    mixed *functions(object ob, int flag: 0);

### DESCRIPTION
    Returns an array of strings containing the function names found in
    <ob>. The functions returned will include all functions, whether public,
    protected, or private. The <flag> parameter is by default zero.

    Flag: 0
    Returns: a string array containing function names, including all inherited
    functions.
    ({ "func1", "func2", "func3", "func4", ... })

    Flag: 1
    Returns: A two-dimensional array of mixed arrays containing additional
    information about each function, including all inherited functions.

    ({
        ({
            "func1",    // function name
            2,          // number of parameters in function
            "int",      // return type of function
            "object",   // the first parameter
            "string *", // the second parameter
        }),
        ...
    })

    Flag: 2
    Like Flag 0, but excludes inherited functions.

    Flag: 3
    Like Flag 1, but excludes inherited functions.

    Flag: 4
    Combined with Flag 1 or 3, the return type string is prefixed with the
    function's declaration modifiers, in the same form variables() reports
    them, for example "private varargs int" or "protected nomask void".
    Access modifiers are reported as seen from <ob>, so a function reached
    through a "private inherit" shows the inherit's modifier. "varargs" is
    reported from the defining program. Without Flag 1 or 3 it has no effect.

### SEE ALSO

    variables(3)
