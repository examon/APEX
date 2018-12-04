int foo(int n) {
    int x = n + 10;
    return x;
}

int bar(void) {
    int y = 42;
    return y;
}

int main(void) {
    int some_int = 10;
    int foo_result = foo(some_int);
    int bar_result = bar();

    return 0;
}
