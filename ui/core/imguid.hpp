#pragma once

#define IMGUID_CONCAT(lhs, rhs) lhs # rhs
#define IMGUID_CONCAT_WRAPPER(lhs, rhs) IMGUID_CONCAT(lhs, rhs)
#define IMGUID_UNIQUE IMGUID_CONCAT_WRAPPER(__FILE__, __LINE__)
#define IMGUID(NAME) NAME "###" IMGUID_UNIQUE
#define IMGUIDU "###" IMGUID_UNIQUE
