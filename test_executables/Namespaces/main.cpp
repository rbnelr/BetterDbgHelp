#include <stdio.h>

__declspec(noinline)
void global () {
	printf("global\n");
}
struct StructA {
	__declspec(noinline)
	void memberA () {
		printf("memberA\n");
	}
};

namespace space {
	__declspec(noinline)
	void namespaced () {
		printf("namespaced\n");
	}
	struct StructB {
		__declspec(noinline)
		void memberB () {
			printf("memberB\n");
		}
	};
	
	namespace nested {
		__declspec(noinline)
		void namespaced2_same_code () {
			printf("namespaced\n");
		}

		struct StructC {
			struct StructD {
				__declspec(noinline)
				void memberCD () {
					printf("memberCD\n");
				}
				__declspec(noinline)
				static void smemberCD () {
					printf("smemberCD\n");
				}
			};
		};
	}
}

int main(int argc, const char** argv) {
	global();
	space::namespaced();
	space::nested::namespaced2_same_code();

	StructA a;
	space::StructB b;
	space::nested::StructC::StructD d;
	a.memberA();
	b.memberB();
	d.memberCD();

	space::nested::StructC::StructD::smemberCD();
	return 0;
}
