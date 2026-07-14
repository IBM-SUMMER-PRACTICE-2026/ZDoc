#include <stdio.h>

#include "bob-client.h"

enum ZDoc_Error bob_client(const char * path, Module * module, char * bob_cli) {
    // Build the prompt
    char * prompt = NULL;
    enum ZDoc_Error rc = build_bob_prompt(path, module, prompt);
    if(rc != ZDOC_OK) {
        free(prompt);
        return rc;
    }


    // Call bob
    char * response = NULL;
    size_t response_len = 0;
    rc = bob_call(prompt, response, &response_len, bob_cli);
    if(rc != ZDOC_OK) {
        free(response);
        return rc;
    }


    // Append the diagrams to the symbols
    rc = append_diagrams(response, response_len, module);
    if(rc != ZDOC_OK) {
        return rc;
    }
    
    
    return ZDOC_OK;
}