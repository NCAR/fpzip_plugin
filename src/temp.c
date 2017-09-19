#include <stdio.h>

#define VERSION 0x110
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

#define MINOR_VERSION(foo) ((foo >>4)&0xF)
#define MAJOR_VERSION(foo) ((foo >>8)&0xF)
#define RELEASE_VERSION(foo) ((foo )&0xF)
int main() {
  printf("hello " TOSTRING(VERSION) "\n");
   printf("%d\n",MINOR_VERSION(VERSION));
   printf("%d\n",MAJOR_VERSION(VERSION));
   printf("%d\n",RELEASE_VERSION(VERSION));
}
