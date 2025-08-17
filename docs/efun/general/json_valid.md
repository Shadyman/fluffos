---
layout: doc
title: general / json_valid
---
# json_valid

### NAME

    json_valid

### SYNOPSIS

    int json_valid(string)

### DESCRIPTION

    Checks if the given string is valid JSON.
    Returns 1 if valid, 0 otherwise.

### EXAMPLES

    int valid = json_valid("{\"name\":\"Alice\"}");
    // valid == 1
    valid = json_valid("{name:Alice}");
    // valid == 0

### SEE ALSO

    json_decode(3), json_encode(3), json_get(3), json_pretty(3)
