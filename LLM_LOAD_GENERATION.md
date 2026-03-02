# Load Testing with LLM-formatted Requests

## Overview

If you would like to perform a load test against an LLM backend, using
LLM-formatted requests, there is an LLM Request Source plugin that can
emulate that workload. These request bodies looks like the following:

```
{
    "model": "Qwen/Qwen2.5-1.5B-Instruct",
    "max_tokens": 10,
    "messages": [
          {
            "role": "user",
            "content": "L Q 5 i x D q v p X"
          }
    ]
}
```

This is generated based on input you provide. The 4 inputs are:

1. Model Name (required)
    - Name of the LLM model the requests are being sent to
2. Request Token Count (default 0)
    - Number of "tokens" generated for the request
3. Response Max Token Count (default 0)
    - Maximum number of tokens for the model to respond with
4. [Request Options List](https://github.com/envoyproxy/nighthawk/blob/09d64d769972513989a95766a98e28f5d6bb05c2/api/client/options.proto#L32) (optional)
    - This allows you to add headers and choose request method of the requests
    - Header 'Content-Type: application/json' added by default
    - Ignore the "request_body_size" and "json_body" in this field
    - Must use ":authority" header instead of ":host"

It's also important to note that all requests are routed to the path "/v1/completions".
There is not currently a way to override this through the CLI. If you need to use
a different path, you will need to edit it in source/request_source/llm_request_source_plugin_impl.cc.

The config for running with this request source is passed into the "--request-source-plugin-config" flag.
Here is an example of how that flag might look for running a load test with this LLM Request Source plugin:
```
--request-source-plugin-config "{name:\"nighthawk.request_source.llm\",typed_config:{\"@type\":\"type.googleapis.com/nighthawk.LlmRequestSourcePluginConfig\", model_name: \"Qwen/Qwen2.5-1.5B-Instruct\", req_token_count: 10, resp_max_tokens: 10, options_list:{options:[{request_headers:[{header:{key:\":authority\",value:\"team1.example.com\"}}]}]}}}"
```

Please be conscious about using escape characters in your string.

## Tokenizer

We do not use a real tokenizer for generating tokens in the requests. Instead,
we do a naive "tokenizer" where each "token" is just a random character in the
range of [A-Za-z0-9] with a space between each. This means that the length of
the requested message will always be 2*req_token_count-1.
