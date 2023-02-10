#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdio.h>

char *str_gsub(char *restrict *restrict haystack, char const *restrict needle, char const *restrict sub);



int main(int argc, char *argv[])
{
  if (argc != 3) exit(1);
  char *line = NULL;
  size_t n = 0;
  getline(&line, &n, stdin);
  {
    char *ret = str_gsub(&line, argv[1], argv[2]);
    if (!ret) exit(1);
    line = ret;
  }
  printf("%s", line);

  free(line);
  return 0;
}

char *str_gsub(char *restrict *restrict haystack, char const *restrict needle, char const *restrict sub){
  char *str = *haystack;
  size_t haystack_len = strlen(str);
  size_t const needle_len = strlen(needle),
         sub_len = strlen(sub);

  for (; (str = strstr(str, needle));){
    ptrdiff_t off = str - *haystack;
    if (sub_len > needle_len) {
      str = realloc(*haystack, sizeof **haystack * (haystack_len + sub_len - needle_len + 1));
      if (!str) goto exit;
      *haystack = str;
      str = *haystack + off;
    }
    memmove(str + sub_len, str + needle_len, haystack_len + 1 - off - needle_len);
    memcpy(str, sub, sub_len);
    haystack_len = haystack_len + sub_len - needle_len;
    str += sub_len;
  }
  str = *haystack;
  if (sub_len < needle_len) {
    str = realloc(*haystack, sizeof **haystack * (haystack_len + 1));
    if (!str) goto exit;
    *haystack = str;
  }
  exit:
    return str;
}
     
