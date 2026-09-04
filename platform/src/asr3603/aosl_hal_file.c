#include <hal/aosl_hal_file.h>

int aosl_hal_mkdir(const char *path)
{
  (void)path;
  return 0;
}

int aosl_hal_rmdir(const char *path)
{
  (void)path;
  return 0;
}

int aosl_hal_fexist(const char *path)
{
  (void)path;
  return 0;
}

int aosl_hal_fsize(const char *path)
{
  (void)path;
  return 0;
}

int aosl_hal_file_create(const char *filepath)
{
  (void)filepath;
  return 0;
}

int aosl_hal_file_delete(const char *filepath)
{
  (void)filepath;
  return 0;
}

int aosl_hal_file_rename(const char *old_name, const char *new_name)
{
  (void)old_name;
  (void)new_name;
  return 0;
}

aosl_fs_t aosl_hal_fopen(const char *filepath, const char *mode)
{
  (void)filepath;
  (void)mode;
  return NULL;
}

int aosl_hal_fclose(aosl_fs_t fs)
{
  (void)fs;
  return 0;
}

int aosl_hal_fread(aosl_fs_t fs, void *buf, size_t size)
{
  (void)fs;
  (void)buf;
  (void)size;
  return 0;
}

int aosl_hal_fwrite(aosl_fs_t fs, const void *buf, size_t size)
{
  (void)fs;
  (void)buf;
  (void)size;
  return 0;
}
