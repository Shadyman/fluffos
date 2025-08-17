---
layout: doc
title: general / json_get
---
# json_get

### NAME

    json_get

### SYNOPSIS

    mixed json_get(string, string)

### DESCRIPTION

    Extracts a value from a JSON string using a JSONPath-like query string.
    Returns the value found at the specified path, or throws an error if the path is invalid.

### EXAMPLES

    string json = "{\"user\":{\"name\":\"Alice\",\"age\":30}}";
    mixed name = json_get(json, "user.name");
    // name == "Alice"

### SEE ALSO

    json_decode(3), json_encode(3), json_pretty(3), json_valid(3)
