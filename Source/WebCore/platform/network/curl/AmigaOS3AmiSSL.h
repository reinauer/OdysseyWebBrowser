/* AmiSSL task lifetime for the single curl request worker on classic OS. */
#pragma once
#ifdef __cplusplus
extern "C" {
#endif
int odysseyAmigaOS3AmiSSLInitialize(void);
void odysseyAmigaOS3AmiSSLCleanup(void);
int odysseyAmigaOS3AmiSSLWorkerInitialize(void);
void odysseyAmigaOS3AmiSSLWorkerCleanup(void);
int odysseyAmigaOS3SocketReceiveBuffer(int socket, int size);
int* odysseyAmigaOS3SocketError(void);
#ifdef __cplusplus
}
#endif
