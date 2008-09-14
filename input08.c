#define A(x) int x;
#define B(x) A(x ## 1) A(x ## 2)

A(g)
B(g)

int main()
{
}
