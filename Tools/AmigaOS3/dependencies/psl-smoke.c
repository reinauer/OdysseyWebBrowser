#include <libpsl.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    const psl_ctx_t* psl = psl_builtin();
    if (!psl || !psl_is_public_suffix(psl, "co.uk")
        || psl_is_public_suffix(psl, "example.co.uk")) return 1;
    const char* domain = psl_registrable_domain(psl, "www.example.co.uk");
    if (!domain || strcmp(domain, "example.co.uk")) return 2;
    if (psl_is_cookie_domain_acceptable(psl, "www.example.co.uk", "co.uk")
        || !psl_is_cookie_domain_acceptable(psl, "www.example.co.uk", "example.co.uk")) return 3;
    if (!psl_is_cookie_domain_acceptable(psl, "2001:db8::1", "2001:db8::1")
        || psl_is_cookie_domain_acceptable(psl, "2001:db8::1", "2001:db8::2")) return 4;
    if (!psl_is_public_suffix(psl, "公司.cn")) return 5;
    puts("PASS native public suffix, cookie boundaries, IPv6 and IDN");
    return 0;
}
