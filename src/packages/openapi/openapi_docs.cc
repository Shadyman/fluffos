/**
 * openapi_docs.cc - OpenAPI documentation serving
 *
 * Serves interactive documentation UIs (Swagger UI, ReDoc)
 */

#include "openapi.h"
#include <sstream>

/**
 * Generate Swagger UI HTML
 */
std::string openapi_generate_swagger_html(const std::string &spec_url, const std::string &title) {
    std::ostringstream html;
    
    html << "<!DOCTYPE html>\n";
    html << "<html>\n";
    html << "<head>\n";
    html << "  <title>" << title << "</title>\n";
    html << "  <link rel=\"stylesheet\" type=\"text/css\" href=\"https://unpkg.com/swagger-ui-dist@4.15.5/swagger-ui.css\" />\n";
    html << "  <style>\n";
    html << "    html { box-sizing: border-box; overflow: -moz-scrollbars-vertical; overflow-y: scroll; }\n";
    html << "    *, *:before, *:after { box-sizing: inherit; }\n";
    html << "    body { margin:0; background: #fafafa; }\n";
    html << "  </style>\n";
    html << "</head>\n";
    html << "<body>\n";
    html << "  <div id=\"swagger-ui\"></div>\n";
    html << "  <script src=\"https://unpkg.com/swagger-ui-dist@4.15.5/swagger-ui-bundle.js\"></script>\n";
    html << "  <script src=\"https://unpkg.com/swagger-ui-dist@4.15.5/swagger-ui-standalone-preset.js\"></script>\n";
    html << "  <script>\n";
    html << "    window.onload = function() {\n";
    html << "      const ui = SwaggerUIBundle({\n";
    html << "        url: '" << spec_url << "',\n";
    html << "        dom_id: '#swagger-ui',\n";
    html << "        deepLinking: true,\n";
    html << "        presets: [\n";
    html << "          SwaggerUIBundle.presets.apis,\n";
    html << "          SwaggerUIStandalonePreset\n";
    html << "        ],\n";
    html << "        plugins: [\n";
    html << "          SwaggerUIBundle.plugins.DownloadUrl\n";
    html << "        ],\n";
    html << "        layout: \"StandaloneLayout\"\n";
    html << "      });\n";
    html << "    };\n";
    html << "  </script>\n";
    html << "</body>\n";
    html << "</html>\n";
    
    return html.str();
}

/**
 * Generate ReDoc HTML
 */
std::string openapi_generate_redoc_html(const std::string &spec_url, const std::string &title) {
    std::ostringstream html;
    
    html << "<!DOCTYPE html>\n";
    html << "<html>\n";
    html << "<head>\n";
    html << "  <title>" << title << "</title>\n";
    html << "  <meta charset=\"utf-8\"/>\n";
    html << "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n";
    html << "  <link href=\"https://fonts.googleapis.com/css?family=Montserrat:300,400,700|Roboto:300,400,700\" rel=\"stylesheet\">\n";
    html << "  <style>\n";
    html << "    body { margin: 0; padding: 0; }\n";
    html << "  </style>\n";
    html << "</head>\n";
    html << "<body>\n";
    html << "  <redoc spec-url='" << spec_url << "'></redoc>\n";
    html << "  <script src=\"https://cdn.jsdelivr.net/npm/redoc@2.0.0/bundles/redoc.standalone.js\"></script>\n";
    html << "</body>\n";
    html << "</html>\n";
    
    return html.str();
}

/**
 * Serve documentation UI implementation
 */
int rest_docs_serve_impl(int router_id, const char *path, const char *ui_type) {
    if (!path || !ui_type) {
        return 0;
    }
    
    // Check if router exists
    auto router_it = g_rest_routers.find(router_id);
    if (router_it == g_rest_routers.end()) {
        return 0; // Router not found
    }
    
    std::string ui_type_str = ui_type;
    std::string path_str = path;
    
    // Create documentation routes
    auto router_ctx = router_it->second;
    
    // Add route for documentation UI
    auto docs_route = std::make_shared<rest_route>();
    docs_route->method = "GET";
    docs_route->pattern = path_str;
    
    try {
        std::string regex_str = rest_pattern_to_regex(path_str, docs_route->param_names);
        docs_route->compiled_pattern = std::regex(regex_str);
    } catch (const std::exception &e) {
        return 0; // Invalid pattern
    }
    
    // Create handler for documentation
    docs_route->handler.type = T_STRING;
    if (ui_type_str == "swagger") {
        docs_route->handler.u.string = make_shared_string("__openapi_swagger_handler");
    } else if (ui_type_str == "redoc") {
        docs_route->handler.u.string = make_shared_string("__openapi_redoc_handler");
    } else {
        return 0; // Unknown UI type
    }
    
    docs_route->metadata = allocate_mapping(2);
    svalue_t key, value;
    
    // Store UI type in metadata
    key.type = T_STRING; key.u.string = make_shared_string("ui_type");
    value.type = T_STRING; value.u.string = make_shared_string(ui_type);
    svalue_t *entry1 = find_for_insert(docs_route->metadata, &key, 0); *entry1 = value;
    free_string(key.u.string);
    free_string(value.u.string);
    
    // Store router ID in metadata
    key.type = T_STRING; key.u.string = make_shared_string("router_id");
    value.type = T_NUMBER; value.u.number = router_id;
    svalue_t *entry2 = find_for_insert(docs_route->metadata, &key, 0); *entry2 = value;
    free_string(key.u.string);
    
    router_ctx->routes.push_back(docs_route);
    
    // Add route for OpenAPI spec JSON
    std::string spec_path = path_str + ".json";
    auto spec_route = std::make_shared<rest_route>();
    spec_route->method = "GET";
    spec_route->pattern = spec_path;
    
    try {
        std::string regex_str = rest_pattern_to_regex(spec_path, spec_route->param_names);
        spec_route->compiled_pattern = std::regex(regex_str);
    } catch (const std::exception &e) {
        return 0; // Invalid pattern
    }
    
    spec_route->handler.type = T_STRING;
    spec_route->handler.u.string = make_shared_string("__openapi_spec_handler");
    
    spec_route->metadata = allocate_mapping(1);
    key.type = T_STRING; key.u.string = make_shared_string("router_id");
    value.type = T_NUMBER; value.u.number = router_id;
    svalue_t *entry3 = find_for_insert(spec_route->metadata, &key, 0); *entry3 = value;
    free_string(key.u.string);
    
    router_ctx->routes.push_back(spec_route);
    
    return 1; // Success
}