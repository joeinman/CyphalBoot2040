#include <pico/stdlib.h>
#include <stdio.h>

int main()
{
    while(1)
    {
        printf("Hello, world!\n");
        tight_loop_contents();
    }
}