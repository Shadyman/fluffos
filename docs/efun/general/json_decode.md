---
layout: doc
title: general / json_decode
---
# json_decode

### NAME

    json_decode

### SYNOPSIS

    mixed json_decode(string)

### DESCRIPTION

    Parses a JSON-formatted string and returns the corresponding LPC value.
    The returned value can be a mapping, array, string, int, float, or nested combinations,
    depending on the JSON input. Throws an error if the input is not valid JSON.

### EXAMPLES

        string json = "{\"name\":\"Alice\",\"age\":30}";
        mapping m = json_decode(json);
        // m == ([ "name": "Alice", "age": 30 ])

### SEE ALSO

    json_encode(3), json_get(3), json_pretty(3), json_valid(3)
