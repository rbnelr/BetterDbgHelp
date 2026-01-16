#include <stdio.h>
#include <iostream>

__declspec(noinline)
void print (const char* prog_name) {
	printf("Executable name: %s\n", prog_name);
}

__declspec(noinline)
int fib (int n) {
	if (n <= 0) return 0;
	if (n <= 1) return 1;
	return fib(n-1) + fib(n-2);
}

__declspec(noinline)
int fib_iter (int n) {
	int a = 0;
	int b = 1;
	printf("Fibbonaci: %d %d", a, b);

	for (int i=0; i<n-2; i++) {
		int c = a + b;
		printf(" %d", c);
		a = b;
		b = c;
	}
	printf("\n");
	return b;
}

__declspec(noinline)
int sqrt (int square) {
	for (int i=0; i<100; i++) {
		if (i*i == square) {
			printf("sqrt(%d) is %d\n", square, i);
			return i;
		}
	}

	printf("sqrt(%d) is unknown\n", square);
	return -1;
}


struct Vec3 {
	float x, y, z;

	__forceinline
	std::string to_string () const {
		char buf[1024];
		int len = sprintf_s(buf, sizeof(buf), "(%f, %f, %f)", x, y, z);
		return std::string(buf, len);
	}
};
__forceinline
Vec3 operator+ (Vec3 const& l, Vec3 const& r) {
	return {
		l.x + r.x,
		l.y + r.y,
		l.z + r.z,
	};
}

__forceinline
void inlined2 (float x, float y, float z) {
	printf("(%s)\n", (Vec3{x, y, z} + Vec3{x, y, -z}).to_string().c_str());
}
__forceinline
void inlined3 (float x, float y, float z) {
	inlined2(x, y, z);
}

__declspec(noinline)
void inlining (float a, float b) {
	inlined3(a, 3, b);
}

int main(int argc, const char** argv) {
	print(argv[0]);

	printf("Fibbonaci(%d): %d\n", 8, fib(8));

	int count = 20;
	printf("first %d numbers of Fibbonaci (iteratively):\n", count);
	fib_iter(count);

	printf("Fibbonaci(%d): %d\n", 8, fib(8));

	sqrt(28*28);
	sqrt(100*100);

	inlining(1, 3);
	inlining(-1, 3);
	return 0;
}
