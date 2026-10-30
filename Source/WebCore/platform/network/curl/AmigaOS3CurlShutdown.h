#pragma once

#ifdef __cplusplus
extern "C" {
#endif
// Call after destroying the browser application, before closing OS libraries.
// This does not construct an unused CurlContext and is safe to call twice.
void odysseyAmigaOS3CurlShutdown(void);
#ifdef __cplusplus
}
#endif
