#ifndef REST_Version
#define REST_Version
/* Version information automatically generated by installer. */ /*
                                                                 * These macros can be used in the following
                                                                 * way:
                                                                 *
                                                                 * #if REST_VERSION_CODE >=
                                                                 * REST_VERSION(2,23,4) #include <newheader.h>
                                                                 * #else
                                                                 *     #include <oldheader.h>
                                                                 * #endif
                                                                 *
                                                                 */
#define REST_RELEASE "2.2.23"
#define REST_RELEASE_DATE "Sat Dec 12"
#define REST_RELEASE_TIME "15:48:32 CET 2020"
#define REST_GIT_COMMIT "7a53c33f"
#define REST_VERSION_CODE 131607
#define REST_VERSION(a, b, c) (((a) << 16) + ((b) << 8) + (c))
#define REST_SCHEMA_EVOLUTION "ON"
#endif
