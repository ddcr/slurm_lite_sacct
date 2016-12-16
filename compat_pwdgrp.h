#include <sys/types.h>

struct passwd *compat_getpwuid(uid_t uid);
struct passwd *compat_getpwnam(const char *name);
struct group  *compat_getgrgid(gid_t gid);
struct group  *compat_getgrnam(const char *name);

int compat_getpwuid_r(uid_t, struct passwd *__restrict,
					  char *__restrict, size_t, struct passwd **__restrict);
int compat_getpwnam_r(const char *__restrict, struct passwd *__restrict,
					  char *__restrict, size_t, struct passwd **__restrict);
int compat_getgrgid_r(gid_t, struct group *__restrict,
					  char *__restrict, size_t, struct group **__restrict);
int compat_getgrnam_r(const char *__restrict, struct group *__restrict,
					  char *__restrict, size_t, struct group **__restrict);
