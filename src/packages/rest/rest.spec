/*
 * REST package efuns for FluffOS
 * 
 * High-level REST API functionality built on HTTP package
 */
int rest_create_router();
int rest_add_route(int, string, string, string | function);
mapping rest_process_route(int, mapping);
string rest_create_jwt(mapping, string);
mapping rest_verify_jwt(string, string);
mapping rest_validate_schema(mixed, mapping);
mapping rest_parse_request(mapping);
mapping rest_format_response(mixed, int, mapping);
mapping rest_generate_openapi(int, mapping);
int rest_set_route_docs(int, string, string, mapping);
int rest_serve_docs(int, string, string);
