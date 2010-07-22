#include "test.hpp"
#include "allocator.hpp"

#include <exception>
#include <stdio.h>
#include <float.h>

test_runner* test_runner::_tests = 0;
size_t test_runner::_memory_fail_threshold = 0;
jmp_buf test_runner::_failure_buffer;
const char* test_runner::_failure_message;

static size_t g_memory_total_size = 0;
static size_t g_memory_total_count = 0;

static void* custom_allocate(size_t size)
{
	if (test_runner::_memory_fail_threshold > 0 && test_runner::_memory_fail_threshold < g_memory_total_size + size)
		return 0;
	else
	{
		void* ptr = memory_allocate(size);

		g_memory_total_size += memory_size(ptr);
		g_memory_total_count++;
		
		return ptr;
	}
}

static void custom_deallocate(void* ptr)
{
	if (ptr)
	{
		g_memory_total_size -= memory_size(ptr);
		g_memory_total_count--;
		
		memory_deallocate(ptr);
	}
}

static void replace_memory_management()
{
	// create some document to touch original functions
	{
		pugi::xml_document doc;
		doc.append_child().set_name(STR("node"));
	}

	// replace functions
	pugi::set_memory_management_functions(custom_allocate, custom_deallocate);
}

#if defined(_MSC_VER) && _MSC_VER > 1200 && _MSC_VER < 1400 && !defined(__INTEL_COMPILER) && !defined(__DMC__)
namespace std
{
	_CRTIMP2 _Prhand _Raise_handler;
	_CRTIMP2 void __cdecl _Throw(const exception&) {}
}
#endif

static bool run_test(test_runner* test)
{
#ifndef PUGIXML_NO_EXCEPTIONS
	try
	{
#endif
		g_memory_total_size = 0;
		g_memory_total_count = 0;
		test_runner::_memory_fail_threshold = 0;
	
#ifdef _MSC_VER
#	pragma warning(push)
#	pragma warning(disable: 4611) // interaction between _setjmp and C++ object destruction is non-portable
#endif

		volatile int result = setjmp(test_runner::_failure_buffer);
	
#ifdef _MSC_VER
#	pragma warning(pop)
#endif

		if (result)
		{
			printf("Test %s failed: %s\n", test->_name, test_runner::_failure_message);
			return false;
		}

		test->run();

		if (g_memory_total_size != 0 || g_memory_total_count != 0)
		{
			printf("Test %s failed: memory leaks found (%u bytes in %u allocations)\n", test->_name, (unsigned int)g_memory_total_size, (unsigned int)g_memory_total_count);
			return false;
		}

		return true;
#ifndef PUGIXML_NO_EXCEPTIONS
	}
	catch (const std::exception& e)
	{
		printf("Test %s failed: exception %s\n", test->_name, e.what());
		return false;
	}
	catch (...)
	{
		printf("Test %s failed for unknown reason\n", test->_name);
		return false;
	}
#endif
}

int main()
{
#ifdef __BORLANDC__
	_control87(MCW_EM | PC_53, MCW_EM | MCW_PC);
#endif

	replace_memory_management();

	unsigned int total = 0;
	unsigned int passed = 0;

	test_runner* test = 0; // gcc3 "variable might be used uninitialized in this function" bug workaround

	for (test = test_runner::_tests; test; test = test->_next)
	{
		total++;
		passed += run_test(test);
	}

	unsigned int failed = total - passed;

	if (failed != 0)
		printf("FAILURE: %u out of %u tests failed.\n", failed, total);
	else
		printf("Success: %u tests passed.\n", total);

	return failed;
}