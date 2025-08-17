---
layout: doc
title: general / json_encode
---
# json_encode

### NAME

    json_encode

### SYNOPSIS

    string json_encode(mixed)

### DESCRIPTION

    Serializes an LPC value (mapping, array, string, int, float, or nested combinations)
    into a JSON-formatted string. Useful for saving data, communicating with web APIs,
    or exporting complex structures. Throws an error if the value contains unsupported types.

### EXAMPLES

    mapping m = ([ "name": "Alice", "age": 30 ]);
    string json = json_encode(m);
    // json == "{\"name\":\"Alice\",\"age\":30}"

### SEE ALSO

    json_decode(3), json_get(3), json_pretty(3), json_valid(3)
