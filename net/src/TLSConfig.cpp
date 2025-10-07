#include "TLSConfig.h"

TLSConfig::TLSConfig() : version_(TLSVersion::TLS_1_2), cipherList_("HIGH:!aNULL:!MDS"), verifyClient_(false), verifyDepth_(4), sessionTimeout_(300), sessionCacheSize_(20480L) {}
