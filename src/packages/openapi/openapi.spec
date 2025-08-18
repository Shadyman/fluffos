/*
 * OpenAPI package efuns for FluffOS
 * 
 * OpenAPI 3.x documentation generation and serving
 */
mapping rest_generate_openapi(int, mapping);
int rest_set_route_docs(int, string, string, mapping);
int rest_serve_docs(int, string, string);
