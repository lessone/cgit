#include "cgit.h"
#include "html.h"

#define MAX_PATH 4096

/* return 1 if path contains a objects/ directory and a HEAD file */
static int is_git_dir(const char *path)
{
	struct stat st;
	static char buf[MAX_PATH];

	if (snprintf(buf, MAX_PATH, "%s/objects", path) >= MAX_PATH) {
		fprintf(stderr, "Insanely long path: %s\n", path);
		return 0;
	}
	if (stat(buf, &st)) {
		if (errno != ENOENT)
			fprintf(stderr, "Error checking path %s: %s (%d)\n",
				path, strerror(errno), errno);
		return 0;
	}
	if (!S_ISDIR(st.st_mode))
		return 0;

	sprintf(buf, "%s/HEAD", path);
	if (stat(buf, &st)) {
		if (errno != ENOENT)
			fprintf(stderr, "Error checking path %s: %s (%d)\n",
				path, strerror(errno), errno);
		return 0;
	}
	if (!S_ISREG(st.st_mode))
		return 0;

	return 1;
}

char *readfile(const char *path)
{
	FILE *f;
	static char buf[MAX_PATH];

	if (!(f = fopen(path, "r")))
		return NULL;
	fgets(buf, MAX_PATH, f);
	fclose(f);
	return buf;
}

static void add_repo(const char *base, const char *path)
{
	struct cgit_repo *repo;
	struct stat st;
	struct passwd *pwd;
	char *p;

	if (stat(path, &st)) {
		fprintf(stderr, "Error accessing %s: %s (%d)\n",
			path, strerror(errno), errno);
		return;
	}
	if ((pwd = getpwuid(st.st_uid)) == NULL) {
		fprintf(stderr, "Error reading owner-info for %s: %s (%d)\n",
			path, strerror(errno), errno);
		return;
	}
	if (base == path)
		p = fmt("%s", path);
	else
		p = fmt("%s", path + strlen(base) + 1);

	if (!strcmp(p + strlen(p) - 5, "/.git"))
		p[strlen(p) - 5] = '\0';

	repo = cgit_add_repo(xstrdup(p));
	repo->name = repo->url;
	repo->path = xstrdup(path);
	repo->owner = (pwd ? xstrdup(pwd->pw_gecos ? pwd->pw_gecos : pwd->pw_name) : "");

	p = fmt("%s/description", path);
	if (!stat(p, &st))
		repo->desc = xstrdup(readfile(p));

	p = fmt("%s/README.html", path);
	if (!stat(p, &st))
		repo->readme = "README.html";
}

static void scan_path(const char *base, const char *path)
{
	DIR *dir;
	struct dirent *ent;
	char *buf;
	struct stat st;

	if (is_git_dir(path)) {
		add_repo(base, path);
		return;
	}
	dir = opendir(path);
	if (!dir) {
		fprintf(stderr, "Error opening directory %s: %s (%d)\n",
			path, strerror(errno), errno);
		return;
	}
	while((ent = readdir(dir)) != NULL) {
		if (ent->d_name[0] == '.') {
			if (ent->d_name[1] == '\0')
				continue;
			if (ent->d_name[1] == '.' && ent->d_name[2] == '\0')
				continue;
		}
		buf = malloc(strlen(path) + strlen(ent->d_name) + 2);
		if (!buf) {
			fprintf(stderr, "Alloc error on %s: %s (%d)\n",
				path, strerror(errno), errno);
			exit(1);
		}
		sprintf(buf, "%s/%s", path, ent->d_name);
		if (stat(buf, &st)) {
			fprintf(stderr, "Error checking path %s: %s (%d)\n",
				buf, strerror(errno), errno);
			free(buf);
			continue;
		}
		if (S_ISDIR(st.st_mode))
			scan_path(base, buf);
		free(buf);
	}
	closedir(dir);
}

void scan_tree(const char *path)
{
	scan_path(path, path);
}
