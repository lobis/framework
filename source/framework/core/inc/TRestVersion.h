#ifndef REST_Version
#define REST_Version
/* Version information automatically generated by installer. *//*
 * These macros can be used in the following way:
 * 
 * #if REST_VERSION_CODE >= REST_VERSION(2,23,4)
 *     #include <newheader.h>
 * #else
 *     #include <oldheader.h>
 * #endif
 *
 */
#define REST_RELEASE "2.3.5"
#define REST_RELEASE_DATE "Sun Apr 11"
#define REST_RELEASE_TIME "13:18:42 CEST 2021"
#define REST_GIT_COMMIT "487a71ba"
#define REST_VERSION_CODE 131845
#define REST_VERSION(a,b,c) (((a) << 16) + ((b) << 8) + (c))
#define REST_SCHEMA_EVOLUTION "ON"
#endif
