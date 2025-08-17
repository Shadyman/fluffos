---
layout: doc
title: general / json_pretty
---
# json_pretty

### NAME

    json_pretty

### SYNOPSIS

    string json_pretty(mixed, int)

### DESCRIPTION

    Serializes an LPC value into a human-readable, pretty-printed JSON string.
    The second argument specifies the indentation level (number of spaces).

### EXAMPLES

    mapping m = ([ "name": "Alice", "age": 30 ]);
    string pretty = json_pretty(m, 4);
    // pretty == "{\n    \"name\": \"Alice\",\n    \"age\": 30\n}"

### SEE ALSO

    json_decode(3), json_encode(3), json_get(3), json_valid(3)
